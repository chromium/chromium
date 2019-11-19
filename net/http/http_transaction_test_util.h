// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_TEST_UTIL_H_
#define NET_HTTP_HTTP_TRANSACTION_TEST_UTIL_H_

#include "net/http/http_transaction.h"

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/socket/connection_attempts.h"

namespace net {

class IOBuffer;
class SSLPrivateKey;
class X509Certificate;
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

using MockTransactionReadHandler = int (*)(int64_t content_length,
                                           int64_t offset,
                                           IOBuffer* buf,
                                           int buf_len);
using MockTransactionHandler = void (*)(const HttpRequestInfo* request,
                                        std::string* response_status,
                                        std::string* response_headers,
                                        std::string* response_data);

struct MockTransaction {
  const char* url;
  const char* method;
  // If |request_time| is unspecified, the current time will be used.
  base::Time request_time;
  const char* request_headers;
  int load_flags;
  const char* status;
  const char* response_headers;
  // If |response_time| is unspecified, the current time will be used.
  base::Time response_time;
  const char* data;
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

// Add/Remove a mock transaction that can be accessed via FindMockTransaction.
// There can be only one MockTransaction associated with a given URL.
void AddMockTransaction(const MockTransaction* trans);
void RemoveMockTransaction(const MockTransaction* trans);

struct ScopedMockTransaction : MockTransaction {
  ScopedMockTransaction() {
    AddMockTransaction(this);
  }
  explicit ScopedMockTransaction(const MockTransaction& t)
      : MockTransaction(t) {
    AddMockTransaction(this);
  }
  ~ScopedMockTransaction() {
    RemoveMockTransaction(this);
  }
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

  bool is_done() const { return state_ == DONE; }
  int error() const { return error_; }

  const HttpResponseInfo* response_info() const {
    return trans_->GetResponseInfo();
  }
  const HttpTransaction* transaction() const { return trans_.get(); }
  const std::string& content() const { return content_; }

 private:
  enum State {
    IDLE,
    STARTING,
    READING,
    DONE
  };

  void DidStart(int result);
  void DidRead(int result);
  void DidFinish(int result);
  void Read();

  void OnIOComplete(int result);

  State state_;
  std::unique_ptr<HttpTransaction> trans_;
  std::string content_;
  scoped_refptr<IOBuffer> read_buf_;
  int error_;

  static int quit_counter_;
};

//-----------------------------------------------------------------------------
// mock network layer

class MockNetworkLayer;

// This transaction class inspects the available set of mock transactions to
// find data for the request URL.  It supports IO operations that complete
// synchronously or asynchronously to help exercise different code paths in the
// HttpCache implementation.
class MockNetworkTransaction
    : public HttpTransaction,
      public base::SupportsWeakPtr<MockNetworkTransaction> {
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
      const BeforeNetworkStartCallback& callback) override;

  void SetBeforeHeadersSentCallback(
      const BeforeHeadersSentCallback& callback) override;

  void SetRequestHeadersCallback(RequestHeadersCallback callback) override {}
  void SetResponseHeadersCallback(ResponseHeadersCallback) override {}

  int ResumeNetworkStart() override;

  void GetConnectionAttempts(ConnectionAttempts* out) const override;

  CreateHelper* websocket_handshake_stream_create_helper() {
    return websocket_handshake_stream_create_helper_;
  }

  RequestPriority priority() const { return priority_; }
  const HttpRequestInfo* request() const { return request_; }

  // Bogus value that will be returned by GetTotalReceivedBytes() if the
  // MockNetworkTransaction was started.
  static const int64_t kTotalReceivedBytes;
  // Bogus value that will be returned by GetTotalSentBytes() if the
  // MockNetworkTransaction was started.
  static const int64_t kTotalSentBytes;

 private:
  int StartInternal(const HttpRequestInfo* request,
                    CompletionOnceCallback callback,
                    const NetLogWithSource& net_log);
  void CallbackLater(CompletionOnceCallback callback, int result);
  void RunCallback(CompletionOnceCallback callback, int result);

  const HttpRequestInfo* request_;
  HttpResponseInfo response_;
  std::string data_;
  int64_t data_cursor_;
  int64_t content_length_;
  int test_mode_;
  RequestPriority priority_;
  MockTransactionReadHandler read_handler_;
  CreateHelper* websocket_handshake_stream_create_helper_;
  BeforeNetworkStartCallback before_network_start_callback_;
  base::WeakPtr<MockNetworkLayer> transaction_factory_;
  int64_t received_bytes_;
  int64_t sent_bytes_;

  // NetLog ID of the fake / non-existent underlying socket used by the
  // connection. Requires Start() be passed a NetLogWithSource with a real
  // NetLog to
  // be initialized.
  unsigned int socket_log_id_;

  bool done_reading_called_;
  bool reading_;

  CompletionOnceCallback resume_start_callback_;  // used for pause and restart.

  base::WeakPtrFactory<MockNetworkTransaction> weak_factory_{this};
};

class MockNetworkLayer : public HttpTransactionFactory,
                         public base::SupportsWeakPtr<MockNetworkLayer> {
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

 private:
  int transaction_count_;
  bool done_reading_called_;
  bool stop_caching_called_;
  RequestPriority last_create_transaction_priority_;

  // By default clock_ is NULL but it can be set to a custom clock by test
  // frameworks using SetClock.
  base::Clock* clock_;

  base::WeakPtr<MockNetworkTransaction> last_transaction_;
};

//-----------------------------------------------------------------------------
// helpers

// read the transaction completely
int ReadTransaction(HttpTransaction* trans, std::string* result);

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_TEST_UTIL_H_
