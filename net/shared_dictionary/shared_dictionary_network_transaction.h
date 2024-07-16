// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_H_
#define NET_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/http/http_transaction.h"
#include "net/shared_dictionary/shared_dictionary_getter.h"
#include "net/socket/next_proto.h"

class GURL;

namespace net {
class SharedDictionary;
class SourceStream;
struct TransportInfo;

// A `HttpTransaction` that decodes shared dictionary compression.
// If the `LOAD_CAN_USE_SHARED_DICTIONARY` flag is not set in the `request`'s
// `load_flags`, this class delegates all function calls to the underlying
// transaction.
// Otherwise, this class registers a callback with the underlying transaction
// that will be called just before the request is sent to the network. When this
// callback is called, this class tries to get a registered dictionary from the
// `shared_dictionary_manager`. If a matching dictionary is found, and the
// "content-encoding" header of the response from the server is "dcb" or "dcz",
// this class will decode the response body using a `BrotliSourceStream` or
// `ZstdSourceStream` with the dictionary.
class NET_EXPORT SharedDictionaryNetworkTransaction : public HttpTransaction {
 public:
  SharedDictionaryNetworkTransaction(
      std::unique_ptr<HttpTransaction> network_transaction,
      bool enable_shared_zstd);

  SharedDictionaryNetworkTransaction(
      const SharedDictionaryNetworkTransaction&) = delete;
  SharedDictionaryNetworkTransaction& operator=(
      const SharedDictionaryNetworkTransaction&) = delete;

  ~SharedDictionaryNetworkTransaction() override;

  // HttpTransaction methods:
  int Start(const HttpRequestInfo* request,
            CompletionOnceCallback callback,
            const NetLogWithSource& net_log) override;
  int RestartIgnoringLastError(CompletionOnceCallback callback) override;
  int RestartWithCertificate(scoped_refptr<X509Certificate> client_cert,
                             scoped_refptr<SSLPrivateKey> client_private_key,
                             CompletionOnceCallback callback) override;
  int RestartWithAuth(const AuthCredentials& credentials,
                      CompletionOnceCallback callback) override;
  bool IsReadyToRestartForAuth() override;

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  void StopCaching() override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  int64_t GetReceivedBodyBytes() const override;
  void DoneReading() override;
  const HttpResponseInfo* GetResponseInfo() const override;
  LoadState GetLoadState() const override;
  void SetQuicServerInfo(QuicServerInfo* quic_server_info) override;
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  bool GetRemoteEndpoint(IPEndPoint* endpoint) const override;
  void PopulateNetErrorDetails(NetErrorDetails* details) const override;
  void SetPriority(RequestPriority priority) override;
  void SetWebSocketHandshakeStreamCreateHelper(
      WebSocketHandshakeStreamBase::CreateHelper* create_helper) override;
  void SetBeforeNetworkStartCallback(
      BeforeNetworkStartCallback callback) override;
  void SetConnectedCallback(const ConnectedCallback& callback) override;
  void SetRequestHeadersCallback(RequestHeadersCallback callback) override;
  void SetResponseHeadersCallback(ResponseHeadersCallback callback) override;
  void SetEarlyResponseHeadersCallback(
      ResponseHeadersCallback callback) override;
  void SetModifyRequestHeadersCallback(
      base::RepeatingCallback<void(HttpRequestHeaders*)> callback) override;
  void SetIsSharedDictionaryReadAllowedCallback(
      base::RepeatingCallback<bool()> callback) override;
  int ResumeNetworkStart() override;
  ConnectionAttempts GetConnectionAttempts() const override;
  void CloseConnectionOnDestruction() override;
  bool IsMdlMatchForMetrics() const override;

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
    PendingReadTask(IOBuffer* buf,
                    int buf_len,
                    CompletionOnceCallback callback);

    PendingReadTask(const PendingReadTask&) = delete;
    PendingReadTask& operator=(const PendingReadTask&) = delete;

    ~PendingReadTask();

    scoped_refptr<IOBuffer> buf;
    int buf_len;
    CompletionOnceCallback callback;
  };

  SharedDictionaryEncodingType ParseSharedDictionaryEncodingType(
      const HttpResponseHeaders& headers);

  void OnStartCompleted(CompletionOnceCallback callback, int result);

  void ModifyRequestHeaders(const GURL& request_url,
                            HttpRequestHeaders* request_headers);

  void OnReadSharedDictionary(base::Time read_start_time, int result);

  int OnConnected(const TransportInfo& info, CompletionOnceCallback callback);

  const bool enable_shared_zstd_;

  scoped_refptr<SharedDictionary> shared_dictionary_;
  // The Structured Field sf-binary hash of sha256 of dictionary calculated when
  // sending a HTTP request.
  std::string dictionary_hash_base64_;

  DictionaryStatus dictionary_status_ = DictionaryStatus::kNoDictionary;

  SharedDictionaryEncodingType shared_dictionary_encoding_type_ =
      SharedDictionaryEncodingType::kNotUsed;

  std::unique_ptr<PendingReadTask> pending_read_task_;

  base::RepeatingCallback<bool()> is_shared_dictionary_read_allowed_callback_;

  // The network side transaction.
  std::unique_ptr<HttpTransaction> network_transaction_;

  std::unique_ptr<SourceStream> shared_compression_stream_;

  // This is set only when a shared dictionary is used for decoding the body.
  std::unique_ptr<HttpResponseInfo> shared_dictionary_used_response_info_;

  ConnectedCallback connected_callback_;

  bool cert_is_issued_by_known_root_ = false;
  NextProto negotiated_protocol_ = kProtoUnknown;

  base::RepeatingCallback<scoped_refptr<SharedDictionary>()>
      shared_dictionary_getter_;

  base::WeakPtrFactory<SharedDictionaryNetworkTransaction> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SHARED_DICTIONARY_SHARED_DICTIONARY_NETWORK_TRANSACTION_H_
