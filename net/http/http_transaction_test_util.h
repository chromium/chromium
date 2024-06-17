// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_TEST_UTIL_H_
#define NET_HTTP_HTTP_TRANSACTION_TEST_UTIL_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/base/transport_info.h"
#include "net/cert/x509_certificate.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/log/net_log_source.h"
#include "net/socket/connection_attempts.h"

namespace net {

class IOBuffer;
class SSLPrivateKey;
class NetLogWithSource;
struct HttpRequestInfo;

//-----------------------------------------------------------------------------
// mock transaction data

// these flags may be combined to form the test_mode field
enum {
  TEST_MODE_NORMAL = 0,
  TEST_MODE_SYNC_NET_START = 1 << 0,
  TEST_MODE_SYNC_NET_READ  = 1 << 1,
  TEST_MODE_SYNC_CACHE_START = 1 << 2,
  TEST_MODE_SYNC_CACHE_READ  = 1 << 3,
  TEST_MODE_SYNC_CACHE_WRITE  = 1 << 4,
  TEST_MODE_SYNC_ALL = (TEST_MODE_SYNC_NET_START | TEST_MODE_SYNC_NET_READ |
                        TEST_MODE_SYNC_CACHE_START | TEST_MODE_SYNC_CACHE_READ |
                        TEST_MODE_SYNC_CACHE_WRITE),
  TEST_MODE_SLOW_READ = 1 << 5
};

using MockTransactionReadHandler = base::RepeatingCallback<
    int(int64_t content_length, int64_t offset, IOBuffer* buf, int buf_len)>;

using MockTransactionHandler =
    base::RepeatingCallback<void(const HttpRequestInfo* request,
                                 std::string* response_status,
                                 std::string* response_headers,
                                 std::string* response_data)>;

// Default TransportInfo suitable for most MockTransactions.
// Describes a direct connection to (127.0.0.1, 80).
TransportInfo DefaultTransportInfo();

struct MockTransaction {
  const char* url;
  const char* method;
  // If |request_time| is unspecified, the current time will be used.
  base::Time request_time;
  const char* request_headers;
  int load_flags;
  // Connection info passed to ConnectedCallback(), if any.
  TransportInfo transport_info = DefaultTransportInfo();
  const char* status;
  const char* response_headers;
  // If |response_time| is unspecified, the current time will be used.
  base::Time response_time;
  const char* data;
  // Any aliases for the requested URL, as read from DNS records. Includes all
  // known aliases, e.g. from A, AAAA, or HTTPS, not just from the address used
  // for the connection, in no particular order.
  std::set<std::string> dns_aliases;
  std::optional<int64_t> fps_cache_filter;
  std::optional<int64_t> browser_run_id;
  int test_mode;
  MockTransactionHandler handler;
  MockTransactionReadHandler read_handler;
  scoped_refptr<X509Certificate> cert;
  CertStatus cert_status;
  int ssl_connection_status;
  // Value returned by MockNetworkTransaction::Start (potentially
  // asynchronously if |!(test_mode & TEST_MODE_SYNC_NET_START)|.)
  Error start_return_code;
  // Value returned by MockNetworkTransaction::Read (potentially
  // asynchronously if |!(test_mode & TEST_MODE_SYNC_NET_START)|.)
  Error read_return_code;
};

extern const MockTransaction kSimpleGET_Transaction;
extern const MockTransaction kSimplePOST_Transaction;
extern const MockTransaction kTypicalGET_Transaction;
extern const MockTransaction kETagGET_Transaction;
extern const MockTransaction kRangeGET_Transaction;

// returns the mock transaction for the given URL
const MockTransaction* FindMockTransaction(const GURL& url);

// Register a mock transaction that can be accessed via
// FindMockTransaction. There can be only one MockTransaction associated
// with a given URL.
struct ScopedMockTransaction : MockTransaction {
  explicit ScopedMockTransaction(const char* url);
  explicit ScopedMockTransaction(const MockTransaction& t,
                                 const char* url = nullptr);
  ~ScopedMockTransaction();
};

//-----------------------------------------------------------------------------
// mock http request

class MockHttpRequest : public HttpRequestInfo {
 public:
  explicit MockHttpRequest(const MockTransaction& t);
  std::string CacheKey();
};

//-----------------------------------------------------------------------------
// use this class to test completely consuming a transaction

class TestTransactionConsumer {
 public:
  TestTransactionConsumer(RequestPriority priority,
                          HttpTransactionFactory* factory);
  virtual ~TestTransactionConsumer();

  void Start(const HttpRequestInfo* request, const NetLogWithSource& net_log);

  bool is_done() const { return state_ == State::kDone; }
  int error() const { return error_; }

  const HttpResponseInfo* response_info() const {
    return trans_->GetResponseInfo();
  }
  const HttpTransaction* transaction() const { return trans_.get(); }
  const std::string& content() const { return content_; }

 private:
  enum class State { kIdle, kStarting, kReading, kDone };

  void DidStart(int result);
  void DidRead(int result);
  void DidFinish(int result);
  void Read();

  void OnIOComplete(int result);

  State state_ = State::kIdle;
  std::unique_ptr<HttpTransaction> trans_;
  std::string content_;
  scoped_refptr<IOBuffer> read_buf_;
  int error_ = OK;
  base::OnceClosure quit_closure_;
};

//-----------------------------------------------------------------------------
// mock network layer

class MockNetworkLayer;

// This transaction class inspects the available set of mock transactions to
// find data for the request URL.  It supports IO operations that complete
// synchronously or asynchronously to help exercise different code paths in the
// HttpCache implementation.
class MockNetworkTransaction final : public HttpTransaction {
  typedef WebSocketHandshakeStreamBase::CreateHelper CreateHelper;

 public:
  MockNetworkTransaction(RequestPriority priority, MockNetworkLayer* factory);
  ~MockNetworkTransaction() override;

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
  void PopulateNetErrorDetails(NetErrorDetails* details) const override;

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

  void SetPriority(RequestPriority priority) override;

  void SetWebSocketHandshakeStreamCreateHelper(
      CreateHelper* create_helper) override;

  void SetBeforeNetworkStartCallback(
      BeforeNetworkStartCallback callback) override;

  void SetConnectedCallback(const ConnectedCallback& callback) override;

  void SetRequestHeadersCallback(RequestHeadersCallback callback) override {}
  void SetResponseHeadersCallback(ResponseHeadersCallback) override {}
  void SetEarlyResponseHeadersCallback(ResponseHeadersCallback) override {}

  void SetModifyRequestHeadersCallback(
      base::RepeatingCallback<void(HttpRequestHeaders*)> callback) override;

  void SetIsSharedDictionaryReadAllowedCallback(
      base::RepeatingCallback<bool()> callback) override {}

  int ResumeNetworkStart() override;

  ConnectionAttempts GetConnectionAttempts() const override;

  void CloseConnectionOnDestruction() override;
  bool IsMdlMatchForMetrics() const override;

  CreateHelper* websocket_handshake_stream_create_helper() {
    return websocket_handshake_stream_create_helper_;
  }

  RequestPriority priority() const { return priority_; }

  base::WeakPtr<MockNetworkTransaction> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Bogus value that will be returned by GetTotalReceivedBytes() if the
  // MockNetworkTransaction was started.
  static const int64_t kTotalReceivedBytes;
  // Bogus value that will be returned by GetTotalSentBytes() if the
  // MockNetworkTransaction was started.
  static const int64_t kTotalSentBytes;
  // Bogus value that will be returned by GetReceivedBodyBytes() if the
  // MockNetworkTransaction was started.
  static const int64_t kReceivedBodyBytes;

 private:
  enum class State {
    NOTIFY_BEFORE_CREATE_STREAM,
    CREATE_STREAM,
    CREATE_STREAM_COMPLETE,
    CONNECTED_CALLBACK,
    CONNECTED_CALLBACK_COMPLETE,
    BUILD_REQUEST,
    BUILD_REQUEST_COMPLETE,
    SEND_REQUEST,
    SEND_REQUEST_COMPLETE,
    READ_HEADERS,
    READ_HEADERS_COMPLETE,
    NONE
  };

  int StartInternal(HttpRequestInfo request, CompletionOnceCallback callback);
  int DoNotifyBeforeCreateStream();
  int DoCreateStream();
  int DoCreateStreamComplete(int result);
  int DoConnectedCallback();
  int DoConnectedCallbackComplete(int result);
  int DoBuildRequest();
  int DoBuildRequestComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  void OnIOComplete(int result);

  void CallbackLater(CompletionOnceCallback callback, int result);
  void RunCallback(CompletionOnceCallback callback, int result);

  raw_ptr<const HttpRequestInfo> original_request_ptr_ = nullptr;
  HttpRequestInfo current_request_;
  State next_state_ = State::NONE;
  NetLogWithSource net_log_;

  CompletionOnceCallback callback_;

  HttpResponseInfo response_;
  std::string data_;
  int64_t data_cursor_ = 0;
  int64_t content_length_ = 0;
  int test_mode_;
  RequestPriority priority_;
  raw_ptr<CreateHelper> websocket_handshake_stream_create_helper_ = nullptr;
  BeforeNetworkStartCallback before_network_start_callback_;
  ConnectedCallback connected_callback_;
  base::WeakPtr<MockNetworkLayer> transaction_factory_;
  int64_t received_bytes_ = 0;
  int64_t sent_bytes_ = 0;
  int64_t received_body_bytes_ = 0;

  // NetLog ID of the fake / non-existent underlying socket used by the
  // connection. Requires Start() be passed a NetLogWithSource with a real
  // NetLog to
  // be initialized.
  unsigned int socket_log_id_ = NetLogSource::kInvalidId;

  bool done_reading_called_ = false;
  bool reading_ = false;

  CompletionOnceCallback resume_start_callback_;  // used for pause and restart.

  base::RepeatingCallback<void(HttpRequestHeaders*)>
      modify_request_headers_callback_;

  base::WeakPtrFactory<MockNetworkTransaction> weak_factory_{this};
};

class MockNetworkLayer final : public HttpTransactionFactory {
 public:
  MockNetworkLayer();
  ~MockNetworkLayer() override;

  int transaction_count() const { return transaction_count_; }
  bool done_reading_called() const { return done_reading_called_; }
  bool stop_caching_called() const { return stop_caching_called_; }
  void TransactionDoneReading();
  void TransactionStopCaching();

  // Resets the transaction count. Can be called after test setup in order to
  // make test expectations independent of how test setup is performed.
  void ResetTransactionCount();

  // Returns the last priority passed to CreateTransaction, or
  // DEFAULT_PRIORITY if it hasn't been called yet.
  RequestPriority last_create_transaction_priority() const {
    return last_create_transaction_priority_;
  }

  // Returns the last transaction created by
  // CreateTransaction. Returns a NULL WeakPtr if one has not been
  // created yet, or the last transaction has been destroyed, or
  // ClearLastTransaction() has been called and a new transaction
  // hasn't been created yet.
  base::WeakPtr<MockNetworkTransaction> last_transaction() {
    return last_transaction_;
  }

  // Makes last_transaction() return NULL until the next transaction
  // is created.
  void ClearLastTransaction() {
    last_transaction_.reset();
  }

  // HttpTransactionFactory:
  int CreateTransaction(RequestPriority priority,
                        std::unique_ptr<HttpTransaction>* trans) override;
  HttpCache* GetCache() override;
  HttpNetworkSession* GetSession() override;

  // The caller must guarantee that |clock| will outlive this object.
  void SetClock(base::Clock* clock);
  base::Clock* clock() const { return clock_; }

  // The current time (will use clock_ if it is non NULL).
  base::Time Now();

  base::WeakPtr<MockNetworkLayer> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  int transaction_count_ = 0;
  bool done_reading_called_ = false;
  bool stop_caching_called_ = false;
  RequestPriority last_create_transaction_priority_ = DEFAULT_PRIORITY;

  // By default clock_ is NULL but it can be set to a custom clock by test
  // frameworks using SetClock.
  raw_ptr<base::Clock> clock_ = nullptr;

  base::WeakPtr<MockNetworkTransaction> last_transaction_;

  base::WeakPtrFactory<MockNetworkLayer> weak_factory_{this};
};

//-----------------------------------------------------------------------------
// helpers

// read the transaction completely
int ReadTransaction(HttpTransaction* trans, std::string* result);

//-----------------------------------------------------------------------------
// connected callback handler

// Used for injecting ConnectedCallback instances in HttpTransaction.
class ConnectedHandler {
 public:
  ConnectedHandler();
  ~ConnectedHandler();

  // Instances of this class are copyable and efficiently movable.
  // WARNING: Do not move an instance to which a callback is bound.
  ConnectedHandler(const ConnectedHandler&);
  ConnectedHandler& operator=(const ConnectedHandler&);
  ConnectedHandler(ConnectedHandler&&);
  ConnectedHandler& operator=(ConnectedHandler&&);

  // Returns a callback bound to this->OnConnected().
  // The returned callback must not outlive this instance.
  HttpTransaction::ConnectedCallback Callback() {
    return base::BindRepeating(&ConnectedHandler::OnConnected,
                               base::Unretained(this));
  }

  // Compatible with HttpTransaction::ConnectedCallback.
  // Returns the last value passed to set_result(), if any, OK otherwise.
  int OnConnected(const TransportInfo& info, CompletionOnceCallback callback);

  // Returns the list of arguments with which OnConnected() was called.
  // The arguments are listed in the same order as the calls were received.
  const std::vector<TransportInfo>& transports() const { return transports_; }

  // Sets the value to be returned by subsequent calls to OnConnected().
  void set_result(int result) { result_ = result; }

  // If true, runs the callback supplied to OnConnected asynchronously with
  // `result_`. Otherwise, the callback is skipped and `result_` is returned
  // directly.
  void set_run_callback(bool run_callback) { run_callback_ = run_callback; }

 private:
  std::vector<TransportInfo> transports_;
  int result_ = OK;
  bool run_callback_ = false;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_TEST_UTIL_H_
