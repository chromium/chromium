// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_network_transaction.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/base/completion_once_callback.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/extras/shared_dictionary/shared_dictionary_isolation_key.h"
#include "net/filter/brotli_source_stream.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/source_stream.h"
#include "net/http/http_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "services/network/shared_dictionary/shared_dictionary.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"

namespace network {

namespace {

// Convert the interface from net::HttpTransaction to net::SourceStream.
class ProxyingSourceStream : public net::SourceStream {
 public:
  explicit ProxyingSourceStream(net::HttpTransaction* transaction)
      : SourceStream(SourceStream::TYPE_NONE), transaction_(transaction) {}

  ProxyingSourceStream(const ProxyingSourceStream&) = delete;
  ProxyingSourceStream& operator=(const ProxyingSourceStream&) = delete;

  ~ProxyingSourceStream() override = default;

  // SourceStream implementation:
  int Read(net::IOBuffer* dest_buffer,
           int buffer_size,
           net::CompletionOnceCallback callback) override {
    DCHECK(transaction_);
    return transaction_->Read(dest_buffer, buffer_size, std::move(callback));
  }

  std::string Description() const override { return std::string(); }

  bool MayHaveMoreBytes() const override { return true; }

 private:
  const raw_ptr<net::HttpTransaction> transaction_;
};

bool ContentEncodingIsSbrOnly(const net::HttpResponseHeaders& headers) {
  std::string content_encoding;
  if (!headers.GetNormalizedHeader("Content-Encoding", &content_encoding)) {
    return false;
  }
  return content_encoding ==
         network::shared_dictionary::kSbrContentEncodingName;
}

}  // namespace

SharedDictionaryNetworkTransaction::PendingReadTask::PendingReadTask(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback)
    : buf(buf), buf_len(buf_len), callback(std::move(callback)) {}

SharedDictionaryNetworkTransaction::PendingReadTask::~PendingReadTask() =
    default;

SharedDictionaryNetworkTransaction::SharedDictionaryNetworkTransaction(
    SharedDictionaryManager& shared_dictionary_manager,
    std::unique_ptr<net::HttpTransaction> network_transaction)
    : shared_dictionary_manager_(shared_dictionary_manager),
      network_transaction_(std::move(network_transaction)) {}

SharedDictionaryNetworkTransaction::~SharedDictionaryNetworkTransaction() =
    default;

int SharedDictionaryNetworkTransaction::Start(
    const net::HttpRequestInfo* request,
    net::CompletionOnceCallback callback,
    const net::NetLogWithSource& net_log) {
  if (!(request->load_flags & net::LOAD_CAN_USE_SHARED_DICTIONARY)) {
    return network_transaction_->Start(request, std::move(callback), net_log);
  }
  absl::optional<net::SharedDictionaryIsolationKey> isolation_key =
      net::SharedDictionaryIsolationKey::MaybeCreate(
          request->network_isolation_key, request->frame_origin);
  if (!isolation_key) {
    return network_transaction_->Start(request, std::move(callback), net_log);
  }

  shared_dictionary_storage_ =
      shared_dictionary_manager_->GetStorage(*isolation_key);
  // Safe to bind unretained `this` because the callback is owned by
  // `network_transaction_` which is owned by `this`.
  network_transaction_->SetModifyRequestHeadersCallback(base::BindRepeating(
      &SharedDictionaryNetworkTransaction::ModifyRequestHeaders,
      base::Unretained(this), request->url));
  return network_transaction_->Start(
      request,
      base::BindOnce(&SharedDictionaryNetworkTransaction::OnStartCompleted,
                     base::Unretained(this), std::move(callback)),
      net_log);
}

void SharedDictionaryNetworkTransaction::OnStartCompleted(
    net::CompletionOnceCallback callback,
    int result) {
  if (result == net::OK && shared_dictionary_ &&
      ContentEncodingIsSbrOnly(
          *network_transaction_->GetResponseInfo()->headers)) {
    shared_dictionary_used_response_info_ =
        std::make_unique<net::HttpResponseInfo>(
            *network_transaction_->GetResponseInfo());
    shared_dictionary_used_response_info_->did_use_shared_dictionary = true;
  }
  std::move(callback).Run(result);
}

void SharedDictionaryNetworkTransaction::ModifyRequestHeaders(
    const GURL& request_url,
    net::HttpRequestHeaders* request_headers) {
  DCHECK(shared_dictionary_storage_);
  // `shared_dictionary_` may have been already set if this transaction was
  // restarted
  if (!shared_dictionary_) {
    shared_dictionary_ =
        shared_dictionary_storage_->GetDictionarySync(request_url);
  }
  if (!shared_dictionary_) {
    return;
  }

  // `is_shared_dictionary_read_allowed_callback_` triggers a notification of
  // the shared dictionary usage to the browser process. So we need to call
  // `is_shared_dictionary_read_allowed_callback_` after checking the result
  // of `GetDictionarySync()`.
  CHECK(is_shared_dictionary_read_allowed_callback_);
  if (!is_shared_dictionary_read_allowed_callback_.Run()) {
    shared_dictionary_.reset();
    return;
  }

  request_headers->SetHeader(
      network::shared_dictionary::kSecAvailableDictionaryHeaderName,
      base::ToLowerASCII(
          base::HexEncode(shared_dictionary_->hash().data,
                          sizeof(shared_dictionary_->hash().data))));

  std::string accept_encoding;
  request_headers->SetHeader(
      net::HttpRequestHeaders::kAcceptEncoding,
      request_headers->GetHeader(net::HttpRequestHeaders::kAcceptEncoding,
                                 &accept_encoding)
          ? accept_encoding + ", sbr"
          : "sbr");

  if (dictionary_status_ == DictionaryStatus::kNoDictionary) {
    dictionary_status_ = DictionaryStatus::kReading;
    auto split_callback = base::SplitOnceCallback(base::BindOnce(
        &SharedDictionaryNetworkTransaction::OnReadSharedDictionary,
        base::Unretained(this), /*read_start_time=*/base::Time::Now()));
    int read_result =
        shared_dictionary_->ReadAll(std::move(split_callback.first));
    if (read_result != net::ERR_IO_PENDING) {
      std::move(split_callback.second).Run(read_result);
    }
  }
}

void SharedDictionaryNetworkTransaction::OnReadSharedDictionary(
    base::Time read_start_time,
    int result) {
  bool succeeded = result == net::OK;
  base::UmaHistogramTimes(
      base::StrCat({"Net.SharedDictionaryTransaction.DictionaryReadLatency.",
                    succeeded ? "Success" : "Failure"}),
      base::Time::Now() - read_start_time);
  if (!succeeded) {
    dictionary_status_ = DictionaryStatus::kFailed;
  } else {
    dictionary_status_ = DictionaryStatus::kFinished;
    CHECK(shared_dictionary_->data());
  }
  if (pending_read_task_) {
    auto task = std::move(pending_read_task_);
    auto split_callback = base::SplitOnceCallback(std::move(task->callback));
    int ret =
        Read(task->buf.get(), task->buf_len, std::move(split_callback.first));
    if (ret != net::ERR_IO_PENDING) {
      std::move(split_callback.second).Run(ret);
    }
  }
}

int SharedDictionaryNetworkTransaction::RestartIgnoringLastError(
    net::CompletionOnceCallback callback) {
  shared_dictionary_used_response_info_.reset();
  return network_transaction_->RestartIgnoringLastError(
      base::BindOnce(&SharedDictionaryNetworkTransaction::OnStartCompleted,
                     base::Unretained(this), std::move(callback)));
}

int SharedDictionaryNetworkTransaction::RestartWithCertificate(
    scoped_refptr<net::X509Certificate> client_cert,
    scoped_refptr<net::SSLPrivateKey> client_private_key,
    net::CompletionOnceCallback callback) {
  shared_dictionary_used_response_info_.reset();
  return network_transaction_->RestartWithCertificate(
      std::move(client_cert), std::move(client_private_key),
      base::BindOnce(&SharedDictionaryNetworkTransaction::OnStartCompleted,
                     base::Unretained(this), std::move(callback)));
}

int SharedDictionaryNetworkTransaction::RestartWithAuth(
    const net::AuthCredentials& credentials,
    net::CompletionOnceCallback callback) {
  shared_dictionary_used_response_info_.reset();
  return network_transaction_->RestartWithAuth(
      credentials,
      base::BindOnce(&SharedDictionaryNetworkTransaction::OnStartCompleted,
                     base::Unretained(this), std::move(callback)));
}

bool SharedDictionaryNetworkTransaction::IsReadyToRestartForAuth() {
  return network_transaction_->IsReadyToRestartForAuth();
}

int SharedDictionaryNetworkTransaction::Read(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback) {
  if (!shared_dictionary_used_response_info_) {
    return network_transaction_->Read(buf, buf_len, std::move(callback));
  }

  switch (dictionary_status_) {
    case DictionaryStatus::kNoDictionary:
      NOTREACHED_NORETURN();
    case DictionaryStatus::kReading:
      CHECK(!pending_read_task_);
      pending_read_task_ =
          std::make_unique<PendingReadTask>(buf, buf_len, std::move(callback));
      return net::ERR_IO_PENDING;
    case DictionaryStatus::kFinished:
      if (!shared_brotli_stream_) {
        shared_brotli_stream_ = net::CreateBrotliSourceStreamWithDictionary(
            std::make_unique<ProxyingSourceStream>(network_transaction_.get()),
            shared_dictionary_->data(), shared_dictionary_->size());
      }
      return shared_brotli_stream_->Read(buf, buf_len, std::move(callback));
    case DictionaryStatus::kFailed:
      return net::ERR_DICTIONARY_LOAD_FAILED;
  }
}

void SharedDictionaryNetworkTransaction::StopCaching() {
  network_transaction_->StopCaching();
}

int64_t SharedDictionaryNetworkTransaction::GetTotalReceivedBytes() const {
  return network_transaction_->GetTotalReceivedBytes();
}

int64_t SharedDictionaryNetworkTransaction::GetTotalSentBytes() const {
  return network_transaction_->GetTotalSentBytes();
}

void SharedDictionaryNetworkTransaction::DoneReading() {
  network_transaction_->DoneReading();
}

const net::HttpResponseInfo*
SharedDictionaryNetworkTransaction::GetResponseInfo() const {
  if (shared_dictionary_used_response_info_) {
    return shared_dictionary_used_response_info_.get();
  }
  return network_transaction_->GetResponseInfo();
}

net::LoadState SharedDictionaryNetworkTransaction::GetLoadState() const {
  return network_transaction_->GetLoadState();
}

void SharedDictionaryNetworkTransaction::SetQuicServerInfo(
    net::QuicServerInfo* quic_server_info) {
  network_transaction_->SetQuicServerInfo(quic_server_info);
}

bool SharedDictionaryNetworkTransaction::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  return network_transaction_->GetLoadTimingInfo(load_timing_info);
}

bool SharedDictionaryNetworkTransaction::GetRemoteEndpoint(
    net::IPEndPoint* endpoint) const {
  return network_transaction_->GetRemoteEndpoint(endpoint);
}

void SharedDictionaryNetworkTransaction::PopulateNetErrorDetails(
    net::NetErrorDetails* details) const {
  return network_transaction_->PopulateNetErrorDetails(details);
}

void SharedDictionaryNetworkTransaction::SetPriority(
    net::RequestPriority priority) {
  network_transaction_->SetPriority(priority);
}

void SharedDictionaryNetworkTransaction::
    SetWebSocketHandshakeStreamCreateHelper(
        net::WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  network_transaction_->SetWebSocketHandshakeStreamCreateHelper(create_helper);
}

void SharedDictionaryNetworkTransaction::SetBeforeNetworkStartCallback(
    BeforeNetworkStartCallback callback) {
  network_transaction_->SetBeforeNetworkStartCallback(std::move(callback));
}

void SharedDictionaryNetworkTransaction::SetRequestHeadersCallback(
    net::RequestHeadersCallback callback) {
  network_transaction_->SetRequestHeadersCallback(std::move(callback));
}

void SharedDictionaryNetworkTransaction::SetResponseHeadersCallback(
    net::ResponseHeadersCallback callback) {
  network_transaction_->SetResponseHeadersCallback(std::move(callback));
}

void SharedDictionaryNetworkTransaction::SetEarlyResponseHeadersCallback(
    net::ResponseHeadersCallback callback) {
  network_transaction_->SetEarlyResponseHeadersCallback(std::move(callback));
}

void SharedDictionaryNetworkTransaction::SetConnectedCallback(
    const ConnectedCallback& callback) {
  network_transaction_->SetConnectedCallback(callback);
}

int SharedDictionaryNetworkTransaction::ResumeNetworkStart() {
  return network_transaction_->ResumeNetworkStart();
}

void SharedDictionaryNetworkTransaction::SetModifyRequestHeadersCallback(
    base::RepeatingCallback<void(net::HttpRequestHeaders*)> callback) {
  // This method should not be called for this class.
  NOTREACHED();
}

void SharedDictionaryNetworkTransaction::
    SetIsSharedDictionaryReadAllowedCallback(
        base::RepeatingCallback<bool()> callback) {
  is_shared_dictionary_read_allowed_callback_ = std::move(callback);
}

net::ConnectionAttempts
SharedDictionaryNetworkTransaction::GetConnectionAttempts() const {
  return network_transaction_->GetConnectionAttempts();
}

void SharedDictionaryNetworkTransaction::CloseConnectionOnDestruction() {
  network_transaction_->CloseConnectionOnDestruction();
}

}  // namespace network
