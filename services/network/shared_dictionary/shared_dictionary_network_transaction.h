// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "net/http/http_transaction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace net {
class SourceStream;
}  // namespace net

namespace network {

class SharedDictionary;
class SharedDictionaryManager;
class SharedDictionaryStorage;

// A `HttpTransaction` that decodes shared dictionary compression.
// If the `LOAD_CAN_USE_SHARED_DICTIONARY` flag is not set in the `request`'s
// `load_flags`, this class delegates all function calls to the underlying
// transaction.
// Otherwise, this class registers a callback with the underlying transaction
// that will be called just before the request is sent to the network. When this
// callback is called, this class tries to get a registered dictionary from the
// `shared_dictionary_manager`. If a matching dictionary is found, and the
// "content-encoding" header of the response from the server is "sbr", this
// class will decode the response body using a `BrotliSourceStream` with the
// dictionary.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryNetworkTransaction
    : public net::HttpTransaction {
 public:
  explicit SharedDictionaryNetworkTransaction(
      SharedDictionaryManager& shared_dictionary_manager,
      std::unique_ptr<net::HttpTransaction> network_transaction);

  SharedDictionaryNetworkTransaction(
      const SharedDictionaryNetworkTransaction&) = delete;
  SharedDictionaryNetworkTransaction& operator=(
      const SharedDictionaryNetworkTransaction&) = delete;

  ~SharedDictionaryNetworkTransaction() override;

  // HttpTransaction methods:
  int Start(const net::HttpRequestInfo* request,
            net::CompletionOnceCallback callback,
            const net::NetLogWithSource& net_log) override;
  int RestartIgnoringLastError(net::CompletionOnceCallback callback) override;
  int RestartWithCertificate(
      scoped_refptr<net::X509Certificate> client_cert,
      scoped_refptr<net::SSLPrivateKey> client_private_key,
      net::CompletionOnceCallback callback) override;
  int RestartWithAuth(const net::AuthCredentials& credentials,
                      net::CompletionOnceCallback callback) override;
  bool IsReadyToRestartForAuth() override;

  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  void StopCaching() override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  void DoneReading() override;
  const net::HttpResponseInfo* GetResponseInfo() const override;
  net::LoadState GetLoadState() const override;
  void SetQuicServerInfo(net::QuicServerInfo* quic_server_info) override;
  bool GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;
  bool GetRemoteEndpoint(net::IPEndPoint* endpoint) const override;
  void PopulateNetErrorDetails(net::NetErrorDetails* details) const override;
  void SetPriority(net::RequestPriority priority) override;
  void SetWebSocketHandshakeStreamCreateHelper(
      net::WebSocketHandshakeStreamBase::CreateHelper* create_helper) override;
  void SetBeforeNetworkStartCallback(
      BeforeNetworkStartCallback callback) override;
  void SetConnectedCallback(const ConnectedCallback& callback) override;
  void SetRequestHeadersCallback(net::RequestHeadersCallback callback) override;
  void SetResponseHeadersCallback(
      net::ResponseHeadersCallback callback) override;
  void SetEarlyResponseHeadersCallback(
      net::ResponseHeadersCallback callback) override;
  void SetModifyRequestHeadersCallback(
      base::RepeatingCallback<void(net::HttpRequestHeaders*)> callback)
      override;
  void SetIsSharedDictionaryReadAllowedCallback(
      base::RepeatingCallback<bool()> callback) override;
  int ResumeNetworkStart() override;
  net::ConnectionAttempts GetConnectionAttempts() const override;
  void CloseConnectionOnDestruction() override;

 private:
  enum class DictionaryStatus {
    kNoDictionary,
    kReading,
    kFinished,
    kFailed,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SharedDictionaryEncodingType {
    kNotUsed = 0,
    kSharedBrotli = 1,
    kSharedZstd = 2,
    kMaxValue = kSharedZstd,
  };

  class PendingReadTask {
   public:
    PendingReadTask(net::IOBuffer* buf,
                    int buf_len,
                    net::CompletionOnceCallback callback);

    PendingReadTask(const PendingReadTask&) = delete;
    PendingReadTask& operator=(const PendingReadTask&) = delete;

    ~PendingReadTask();

    scoped_refptr<net::IOBuffer> buf;
    int buf_len;
    net::CompletionOnceCallback callback;
  };

  SharedDictionaryEncodingType ParseSharedDictionaryEncodingType(
      const net::HttpResponseHeaders& headers);

  void OnStartCompleted(net::CompletionOnceCallback callback, int result);

  void ModifyRequestHeaders(const GURL& request_url,
                            net::HttpRequestHeaders* request_headers);

  void OnReadSharedDictionary(base::Time read_start_time, int result);

  raw_ref<SharedDictionaryManager> shared_dictionary_manager_;
  scoped_refptr<SharedDictionaryStorage> shared_dictionary_storage_;
  std::unique_ptr<SharedDictionary> shared_dictionary_;

  DictionaryStatus dictionary_status_ = DictionaryStatus::kNoDictionary;

  SharedDictionaryEncodingType shared_dictionary_encoding_type_ =
      SharedDictionaryEncodingType::kNotUsed;

  std::unique_ptr<PendingReadTask> pending_read_task_;

  base::RepeatingCallback<bool()> is_shared_dictionary_read_allowed_callback_;

  // The network side transaction.
  std::unique_ptr<net::HttpTransaction> network_transaction_;

  std::unique_ptr<net::SourceStream> shared_compression_stream_;

  // This is set only when a shared dictionary is used for decoding the body.
  std::unique_ptr<net::HttpResponseInfo> shared_dictionary_used_response_info_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_H_
