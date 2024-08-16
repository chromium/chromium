// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/shared_dictionary/shared_dictionary_network_transaction.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/transport_info.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/filter/brotli_source_stream.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/source_stream.h"
#include "net/filter/zstd_source_stream.h"
#include "net/http/http_request_info.h"
#include "net/http/structured_headers.h"
#include "net/shared_dictionary/shared_dictionary_constants.h"
#include "net/shared_dictionary/shared_dictionary_header_checker_source_stream.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "net/ssl/ssl_private_key.h"

namespace net {

namespace {

// Convert the interface from HttpTransaction to SourceStream.
class ProxyingSourceStream : public SourceStream {
 public:
  explicit ProxyingSourceStream(HttpTransaction* transaction)
      : SourceStream(SourceStream::TYPE_NONE), transaction_(transaction) {}

  ProxyingSourceStream(const ProxyingSourceStream&) = delete;
  ProxyingSourceStream& operator=(const ProxyingSourceStream&) = delete;

  ~ProxyingSourceStream() override = default;

  // SourceStream implementation:
  int Read(IOBuffer* dest_buffer,
           int buffer_size,
           CompletionOnceCallback callback) override {
    DCHECK(transaction_);
    return transaction_->Read(dest_buffer, buffer_size, std::move(callback));
  }

  std::string Description() const override { return std::string(); }

  bool MayHaveMoreBytes() const override { return true; }

 private:
  const raw_ptr<HttpTransaction> transaction_;
};

void AddAcceptEncoding(HttpRequestHeaders* request_headers,
                       std::string_view encoding_header) {
  std::optional<std::string> accept_encoding =
      request_headers->GetHeader(HttpRequestHeaders::kAcceptEncoding);
  request_headers->SetHeader(
      HttpRequestHeaders::kAcceptEncoding,
      accept_encoding ? base::StrCat({*accept_encoding, ", ", encoding_header})
                      : std::string(encoding_header));
}

}  // namespace

SharedDictionaryNetworkTransaction::PendingReadTask::PendingReadTask(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback)
    : buf(buf), buf_len(buf_len), callback(std::move(callback)) {}

SharedDictionaryNetworkTransaction::PendingReadTask::~PendingReadTask() =
    default;

SharedDictionaryNetworkTransaction::SharedDictionaryNetworkTransaction(
    std::unique_ptr<HttpTransaction> network_transaction,
    bool enable_shared_zstd)
    : enable_shared_zstd_(enable_shared_zstd),
      network_transaction_(std::move(network_transaction)) {
  network_transaction_->SetConnectedCallback(
      base::BindRepeating(&SharedDictionaryNetworkTransaction::OnConnected,
                          base::Unretained(this)));
}

SharedDictionaryNetworkTransaction::~SharedDictionaryNetworkTransaction() =
    default;

int SharedDictionaryNetworkTransaction::Start(const HttpRequestInfo* request,
                                              CompletionOnceCallback callback,
                                              const NetLogWithSource& net_log) {
  if (!(request->load_flags & LOAD_CAN_USE_SHARED_DICTIONARY) ||
      !request->dictionary_getter) {
    return network_transaction_->Start(request, std::move(callback), net_log);
  }
  std::optional<SharedDictionaryIsolationKey> isolation_key =
      SharedDictionaryIsolationKey::MaybeCreate(request->network_isolation_key,
                                                request->frame_origin);
  shared_dictionary_getter_ = base::BindRepeating(request->dictionary_getter,
                                                   isolation_key, request->url);

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

SharedDictionaryNetworkTransaction::SharedDictionaryEncodingType
SharedDictionaryNetworkTransaction::ParseSharedDictionaryEncodingType(
    const HttpResponseHeaders& headers) {
  std::string content_encoding;
  if (!headers.GetNormalizedHeader("Content-Encoding", &content_encoding)) {
    return SharedDictionaryEncodingType::kNotUsed;
  } else if (content_encoding ==
             shared_dictionary::kSharedBrotliContentEncodingName) {
    return SharedDictionaryEncodingType::kSharedBrotli;
  } else if (enable_shared_zstd_ &&
             content_encoding ==
                 shared_dictionary::kSharedZstdContentEncodingName) {
    return SharedDictionaryEncodingType::kSharedZstd;
  }
  return SharedDictionaryEncodingType::kNotUsed;
}

void SharedDictionaryNetworkTransaction::OnStartCompleted(
    CompletionOnceCallback callback,
    int result) {
  if (shared_dictionary_) {
    base::UmaHistogramSparse(
        base::StrCat({"Net.SharedDictionaryTransaction.NetResultWithDict.",
                      cert_is_issued_by_known_root_
                          ? "KnownRootCert"
                          : "UnknownRootCertOrNoCert"}),
        -result);
  }

  if (result != OK || !shared_dictionary_) {
    std::move(callback).Run(result);
    return;
  }

  shared_dictionary_encoding_type_ = ParseSharedDictionaryEncodingType(
      *network_transaction_->GetResponseInfo()->headers);
  if (shared_dictionary_encoding_type_ ==
      SharedDictionaryEncodingType::kNotUsed) {
    std::move(callback).Run(result);
    return;
  }

  shared_dictionary_used_response_info_ = std::make_unique<HttpResponseInfo>(
      *network_transaction_->GetResponseInfo());
  shared_dictionary_used_response_info_->did_use_shared_dictionary = true;
  std::move(callback).Run(result);
}

void SharedDictionaryNetworkTransaction::ModifyRequestHeaders(
    const GURL& request_url,
    HttpRequestHeaders* request_headers) {
  // `shared_dictionary_` may have been already set if this transaction was
  // restarted
  if (!shared_dictionary_) {
    shared_dictionary_ = shared_dictionary_getter_.Run();
  }
  if (!shared_dictionary_) {
    return;
  }

  if (!IsLocalhost(request_url)) {
    if (!base::FeatureList::IsEnabled(
            features::kCompressionDictionaryTransportOverHttp1) &&
        negotiated_protocol_ != kProtoHTTP2 &&
        negotiated_protocol_ != kProtoQUIC) {
      shared_dictionary_.reset();
      return;
    }
    if (!base::FeatureList::IsEnabled(
            features::kCompressionDictionaryTransportOverHttp2) &&
        negotiated_protocol_ == kProtoHTTP2) {
      shared_dictionary_.reset();
      return;
    }
  }
  if (base::FeatureList::IsEnabled(
          features::kCompressionDictionaryTransportRequireKnownRootCert) &&
      !cert_is_issued_by_known_root_ && !IsLocalhost(request_url)) {
    shared_dictionary_.reset();
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
  dictionary_hash_base64_ = base::StrCat(
      {":", base::Base64Encode(shared_dictionary_->hash().data), ":"});
  request_headers->SetHeader(shared_dictionary::kAvailableDictionaryHeaderName,
                             dictionary_hash_base64_);
  if (enable_shared_zstd_) {
    AddAcceptEncoding(
        request_headers,
        base::StrCat({shared_dictionary::kSharedBrotliContentEncodingName, ", ",
                      shared_dictionary::kSharedZstdContentEncodingName}));
  } else {
    AddAcceptEncoding(request_headers,
                      shared_dictionary::kSharedBrotliContentEncodingName);
  }

  if (!shared_dictionary_->id().empty()) {
    std::optional<std::string> serialized_id =
        structured_headers::SerializeItem(shared_dictionary_->id());
    if (serialized_id) {
      request_headers->SetHeader("Dictionary-ID", *serialized_id);
    }
  }

  if (dictionary_status_ == DictionaryStatus::kNoDictionary) {
    dictionary_status_ = DictionaryStatus::kReading;
    auto split_callback = base::SplitOnceCallback(base::BindOnce(
        [](base::WeakPtr<SharedDictionaryNetworkTransaction> self,
           base::Time read_start_time, int result) {
          if (!self) {
            bool succeeded = result == OK;
            base::UmaHistogramTimes(
                base::StrCat({"Net.SharedDictionaryTransaction."
                              "AbortedWhileReadingDictionary.",
                              succeeded ? "Success" : "Failure"}),
                base::Time::Now() - read_start_time);
            return;
          }
          self->OnReadSharedDictionary(read_start_time, result);
        },
        weak_factory_.GetWeakPtr(), /*read_start_time=*/base::Time::Now()));

    int read_result =
        shared_dictionary_->ReadAll(std::move(split_callback.first));
    if (read_result != ERR_IO_PENDING) {
      std::move(split_callback.second).Run(read_result);
    }
  }
}

void SharedDictionaryNetworkTransaction::OnReadSharedDictionary(
    base::Time read_start_time,
    int result) {
  bool succeeded = result == OK;
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
    if (ret != ERR_IO_PENDING) {
      std::move(split_callback.second).Run(ret);
    }
  }
}

int SharedDictionaryNetworkTransaction::RestartIgnoringLastError(
    CompletionOnceCallback callback) {
  shared_dictionary_used_response_info_.reset();
  return network_transaction_->RestartIgnoringLastError(
      base::BindOnce(&SharedDictionaryNetworkTransaction::OnStartCompleted,
                     base::Unretained(this), std::move(callback)));
}

int SharedDictionaryNetworkTransaction::RestartWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key,
    CompletionOnceCallback callback) {
  shared_dictionary_used_response_info_.reset();
  return network_transaction_->RestartWithCertificate(
      std::move(client_cert), std::move(client_private_key),
      base::BindOnce(&SharedDictionaryNetworkTransaction::OnStartCompleted,
                     base::Unretained(this), std::move(callback)));
}

int SharedDictionaryNetworkTransaction::RestartWithAuth(
    const AuthCredentials& credentials,
    CompletionOnceCallback callback) {
  shared_dictionary_used_response_info_.reset();
  return network_transaction_->RestartWithAuth(
      credentials,
      base::BindOnce(&SharedDictionaryNetworkTransaction::OnStartCompleted,
                     base::Unretained(this), std::move(callback)));
}

bool SharedDictionaryNetworkTransaction::IsReadyToRestartForAuth() {
  return network_transaction_->IsReadyToRestartForAuth();
}

int SharedDictionaryNetworkTransaction::Read(IOBuffer* buf,
                                             int buf_len,
                                             CompletionOnceCallback callback) {
  if (!shared_dictionary_used_response_info_) {
    return network_transaction_->Read(buf, buf_len, std::move(callback));
  }

  switch (dictionary_status_) {
    case DictionaryStatus::kNoDictionary:
      NOTREACHED();
    case DictionaryStatus::kReading:
      CHECK(!pending_read_task_);
      pending_read_task_ =
          std::make_unique<PendingReadTask>(buf, buf_len, std::move(callback));
      return ERR_IO_PENDING;
    case DictionaryStatus::kFinished:
      if (!shared_compression_stream_) {
        // Wrap the source `network_transaction_` with a
        // SharedDictionaryHeaderCheckerSourceStream to check the header
        // of Dictionary-Compressed stream.
        std::unique_ptr<SourceStream> header_checker_source_stream =
            std::make_unique<SharedDictionaryHeaderCheckerSourceStream>(
                std::make_unique<ProxyingSourceStream>(
                    network_transaction_.get()),
                shared_dictionary_encoding_type_ ==
                        SharedDictionaryEncodingType::kSharedBrotli
                    ? SharedDictionaryHeaderCheckerSourceStream::Type::
                          kDictionaryCompressedBrotli
                    : SharedDictionaryHeaderCheckerSourceStream::Type::
                          kDictionaryCompressedZstd,
                shared_dictionary_->hash());
        if (shared_dictionary_encoding_type_ ==
            SharedDictionaryEncodingType::kSharedBrotli) {
          SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
              "Network.SharedDictionary."
              "CreateBrotliSourceStreamWithDictionary");
          shared_compression_stream_ = CreateBrotliSourceStreamWithDictionary(
              std::move(header_checker_source_stream),
              shared_dictionary_->data(), shared_dictionary_->size());
        } else if (shared_dictionary_encoding_type_ ==
                   SharedDictionaryEncodingType::kSharedZstd) {
          SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
              "Network.SharedDictionary.CreateZstdSourceStreamWithDictionary");
          shared_compression_stream_ = CreateZstdSourceStreamWithDictionary(
              std::move(header_checker_source_stream),
              shared_dictionary_->data(), shared_dictionary_->size());
        }

        UMA_HISTOGRAM_ENUMERATION("Network.SharedDictionary.EncodingType",
                                  shared_dictionary_encoding_type_);
      }
      // When NET_DISABLE_BROTLI or NET_DISABLE_ZSTD is set,
      // `shared_compression_stream_` can be null.
      if (!shared_compression_stream_) {
        return ERR_CONTENT_DECODING_FAILED;
      }
      return shared_compression_stream_->Read(buf, buf_len,
                                              std::move(callback));
    case DictionaryStatus::kFailed:
      return ERR_DICTIONARY_LOAD_FAILED;
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

int64_t SharedDictionaryNetworkTransaction::GetReceivedBodyBytes() const {
  return network_transaction_->GetReceivedBodyBytes();
}

void SharedDictionaryNetworkTransaction::DoneReading() {
  network_transaction_->DoneReading();
}

const HttpResponseInfo* SharedDictionaryNetworkTransaction::GetResponseInfo()
    const {
  if (shared_dictionary_used_response_info_) {
    return shared_dictionary_used_response_info_.get();
  }
  return network_transaction_->GetResponseInfo();
}

LoadState SharedDictionaryNetworkTransaction::GetLoadState() const {
  return network_transaction_->GetLoadState();
}

void SharedDictionaryNetworkTransaction::SetQuicServerInfo(
    QuicServerInfo* quic_server_info) {
  network_transaction_->SetQuicServerInfo(quic_server_info);
}

bool SharedDictionaryNetworkTransaction::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  return network_transaction_->GetLoadTimingInfo(load_timing_info);
}

bool SharedDictionaryNetworkTransaction::GetRemoteEndpoint(
    IPEndPoint* endpoint) const {
  return network_transaction_->GetRemoteEndpoint(endpoint);
}

void SharedDictionaryNetworkTransaction::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  return network_transaction_->PopulateNetErrorDetails(details);
}

void SharedDictionaryNetworkTransaction::SetPriority(RequestPriority priority) {
  network_transaction_->SetPriority(priority);
}

void SharedDictionaryNetworkTransaction::
    SetWebSocketHandshakeStreamCreateHelper(
        WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  network_transaction_->SetWebSocketHandshakeStreamCreateHelper(create_helper);
}

void SharedDictionaryNetworkTransaction::SetBeforeNetworkStartCallback(
    BeforeNetworkStartCallback callback) {
  network_transaction_->SetBeforeNetworkStartCallback(std::move(callback));
}

void SharedDictionaryNetworkTransaction::SetRequestHeadersCallback(
    RequestHeadersCallback callback) {
  network_transaction_->SetRequestHeadersCallback(std::move(callback));
}

void SharedDictionaryNetworkTransaction::SetResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  network_transaction_->SetResponseHeadersCallback(std::move(callback));
}

void SharedDictionaryNetworkTransaction::SetEarlyResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  network_transaction_->SetEarlyResponseHeadersCallback(std::move(callback));
}

void SharedDictionaryNetworkTransaction::SetConnectedCallback(
    const ConnectedCallback& callback) {
  connected_callback_ = callback;
}

int SharedDictionaryNetworkTransaction::ResumeNetworkStart() {
  return network_transaction_->ResumeNetworkStart();
}

void SharedDictionaryNetworkTransaction::SetModifyRequestHeadersCallback(
    base::RepeatingCallback<void(HttpRequestHeaders*)> callback) {
  // This method should not be called for this class.
  NOTREACHED_IN_MIGRATION();
}

void SharedDictionaryNetworkTransaction::
    SetIsSharedDictionaryReadAllowedCallback(
        base::RepeatingCallback<bool()> callback) {
  is_shared_dictionary_read_allowed_callback_ = std::move(callback);
}

ConnectionAttempts SharedDictionaryNetworkTransaction::GetConnectionAttempts()
    const {
  return network_transaction_->GetConnectionAttempts();
}

void SharedDictionaryNetworkTransaction::CloseConnectionOnDestruction() {
  network_transaction_->CloseConnectionOnDestruction();
}

bool SharedDictionaryNetworkTransaction::IsMdlMatchForMetrics() const {
  return network_transaction_->IsMdlMatchForMetrics();
}

int SharedDictionaryNetworkTransaction::OnConnected(
    const TransportInfo& info,
    CompletionOnceCallback callback) {
  cert_is_issued_by_known_root_ = info.cert_is_issued_by_known_root;
  negotiated_protocol_ = info.negotiated_protocol;

  if (connected_callback_) {
    return connected_callback_.Run(info, std::move(callback));
  }
  return OK;
}

}  // namespace net
