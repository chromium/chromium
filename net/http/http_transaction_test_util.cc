// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_transaction_test_util.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/schemeful_site.h"
#include "net/cert/x509_certificate.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {
using MockTransactionMap =
    std::unordered_map<std::string, const MockTransaction*>;
static MockTransactionMap mock_transactions;

void AddMockTransaction(const MockTransaction* trans) {
  auto result =
      mock_transactions.insert(std::make_pair(GURL(trans->url).spec(), trans));
  CHECK(result.second) << "Transaction already exists: " << trans->url;
}

void RemoveMockTransaction(const MockTransaction* trans) {
  mock_transactions.erase(GURL(trans->url).spec());
}

}  // namespace

TransportInfo DefaultTransportInfo() {
  return TransportInfo(TransportType::kDirect,
                       IPEndPoint(IPAddress::IPv4Localhost(), 80),
                       /*accept_ch_frame_arg=*/"",
                       /*cert_is_issued_by_known_root=*/false, kProtoUnknown);
}

//-----------------------------------------------------------------------------
// mock transaction data

const MockTransaction kSimpleGET_Transaction = {
    "http://www.google.com/",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n",
    base::Time(),
    "<html><body>Google Blah Blah</body></html>",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

const MockTransaction kSimplePOST_Transaction = {
    "http://bugdatabase.com/edit",
    "POST",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "",
    base::Time(),
    "<html><body>Google Blah Blah</body></html>",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

const MockTransaction kTypicalGET_Transaction = {
    "http://www.example.com/~foo/bar.html",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
    "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n",
    base::Time(),
    "<html><body>Google Blah Blah</body></html>",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

const MockTransaction kETagGET_Transaction = {
    "http://www.google.com/foopy",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Etag: \"foopy\"\n",
    base::Time(),
    "<html><body>Google Blah Blah</body></html>",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

const MockTransaction kRangeGET_Transaction = {
    "http://www.google.com/",
    "GET",
    base::Time(),
    "Range: 0-100\r\n",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n",
    base::Time(),
    "<html><body>Google Blah Blah</body></html>",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

static const MockTransaction* const kBuiltinMockTransactions[] = {
  &kSimpleGET_Transaction,
  &kSimplePOST_Transaction,
  &kTypicalGET_Transaction,
  &kETagGET_Transaction,
  &kRangeGET_Transaction
};

const MockTransaction* FindMockTransaction(const GURL& url) {
  // look for overrides:
  MockTransactionMap::const_iterator it = mock_transactions.find(url.spec());
  if (it != mock_transactions.end())
    return it->second;

  // look for builtins:
  for (const auto* transaction : kBuiltinMockTransactions) {
    if (url == GURL(transaction->url))
      return transaction;
  }
  return nullptr;
}

ScopedMockTransaction::ScopedMockTransaction(const char* url)
    : MockTransaction({nullptr}) {
  CHECK(url);
  this->url = url;
  AddMockTransaction(this);
}

ScopedMockTransaction::ScopedMockTransaction(const MockTransaction& t,
                                             const char* url)
    : MockTransaction(t) {
  if (url) {
    this->url = url;
  }
  AddMockTransaction(this);
}

ScopedMockTransaction::~ScopedMockTransaction() {
  RemoveMockTransaction(this);
}

MockHttpRequest::MockHttpRequest(const MockTransaction& t) {
  url = GURL(t.url);
  method = t.method;
  extra_headers.AddHeadersFromString(t.request_headers);
  load_flags = t.load_flags;
  SchemefulSite site(url);
  network_isolation_key = NetworkIsolationKey(site, site);
  network_anonymization_key = NetworkAnonymizationKey::CreateSameSite(site);
  frame_origin = url::Origin::Create(url);
  fps_cache_filter = t.fps_cache_filter;
  browser_run_id = t.browser_run_id;
}

std::string MockHttpRequest::CacheKey() {
  return *HttpCache::GenerateCacheKeyForRequest(this);
}

//-----------------------------------------------------------------------------

TestTransactionConsumer::TestTransactionConsumer(
    RequestPriority priority,
    HttpTransactionFactory* factory) {
  // Disregard the error code.
  factory->CreateTransaction(priority, &trans_);
}

TestTransactionConsumer::~TestTransactionConsumer() = default;

void TestTransactionConsumer::Start(const HttpRequestInfo* request,
                                    const NetLogWithSource& net_log) {
  state_ = State::kStarting;
  int result =
      trans_->Start(request,
                    base::BindOnce(&TestTransactionConsumer::OnIOComplete,
                                   base::Unretained(this)),
                    net_log);
  if (result != ERR_IO_PENDING)
    DidStart(result);

  base::RunLoop loop;
  quit_closure_ = loop.QuitClosure();
  loop.Run();
}

void TestTransactionConsumer::DidStart(int result) {
  if (result != OK) {
    DidFinish(result);
  } else {
    Read();
  }
}

void TestTransactionConsumer::DidRead(int result) {
  if (result <= 0) {
    DidFinish(result);
  } else {
    content_.append(read_buf_->data(), result);
    Read();
  }
}

void TestTransactionConsumer::DidFinish(int result) {
  state_ = State::kDone;
  error_ = result;
  if (!quit_closure_.is_null()) {
    std::move(quit_closure_).Run();
  }
}

void TestTransactionConsumer::Read() {
  state_ = State::kReading;
  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(1024);
  int result =
      trans_->Read(read_buf_.get(), 1024,
                   base::BindOnce(&TestTransactionConsumer::OnIOComplete,
                                  base::Unretained(this)));
  if (result != ERR_IO_PENDING)
    DidRead(result);
}

void TestTransactionConsumer::OnIOComplete(int result) {
  switch (state_) {
    case State::kStarting:
      DidStart(result);
      break;
    case State::kReading:
      DidRead(result);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

MockNetworkTransaction::MockNetworkTransaction(RequestPriority priority,
                                               MockNetworkLayer* factory)
    : priority_(priority), transaction_factory_(factory->AsWeakPtr()) {}

MockNetworkTransaction::~MockNetworkTransaction() {
  // Use `original_request_ptr_` as in ~HttpNetworkTransaction to make sure its
  // valid and not already freed by the consumer. Only check till Read is
  // invoked since HttpNetworkTransaction sets request_ to nullptr when Read is
  // invoked. See crbug.com/734037.
  if (original_request_ptr_ && !reading_) {
    DCHECK(original_request_ptr_->load_flags >= 0);
  }
}

int MockNetworkTransaction::Start(const HttpRequestInfo* request,
                                  CompletionOnceCallback callback,
                                  const NetLogWithSource& net_log) {
  net_log_ = net_log;
  CHECK(!original_request_ptr_);
  original_request_ptr_ = request;
  return StartInternal(*request, std::move(callback));
}

int MockNetworkTransaction::RestartIgnoringLastError(
    CompletionOnceCallback callback) {
  return ERR_FAILED;
}

int MockNetworkTransaction::RestartWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key,
    CompletionOnceCallback callback) {
  return ERR_FAILED;
}

int MockNetworkTransaction::RestartWithAuth(const AuthCredentials& credentials,
                                            CompletionOnceCallback callback) {
  if (!IsReadyToRestartForAuth())
    return ERR_FAILED;

  HttpRequestInfo auth_request_info = *original_request_ptr_;
  auth_request_info.extra_headers.SetHeader("Authorization", "Bar");

  // Let the MockTransactionHandler worry about this: the only way for this
  // test to succeed is by using an explicit handler for the transaction so
  // that server behavior can be simulated.
  return StartInternal(std::move(auth_request_info), std::move(callback));
}

void MockNetworkTransaction::PopulateNetErrorDetails(
    NetErrorDetails* /*details*/) const {
  NOTIMPLEMENTED();
}

bool MockNetworkTransaction::IsReadyToRestartForAuth() {
  CHECK(original_request_ptr_);
  if (!original_request_ptr_->extra_headers.HasHeader("X-Require-Mock-Auth")) {
    return false;
  }

  // Allow the mock server to decide whether authentication is required or not.
  std::string status_line = response_.headers->GetStatusLine();
  return status_line.find(" 401 ") != std::string::npos ||
      status_line.find(" 407 ") != std::string::npos;
}

int MockNetworkTransaction::Read(IOBuffer* buf,
                                 int buf_len,
                                 CompletionOnceCallback callback) {
  const MockTransaction* t = FindMockTransaction(current_request_.url);
  DCHECK(t);

  CHECK(!done_reading_called_);
  reading_ = true;

  int num = t->read_return_code;

  if (OK == num) {
    if (t->read_handler) {
      num = t->read_handler.Run(content_length_, data_cursor_, buf, buf_len);
      data_cursor_ += num;
    } else {
      int data_len = static_cast<int>(data_.size());
      num = std::min(static_cast<int64_t>(buf_len), data_len - data_cursor_);
      if (test_mode_ & TEST_MODE_SLOW_READ)
        num = std::min(num, 1);
      if (num) {
        memcpy(buf->data(), data_.data() + data_cursor_, num);
        data_cursor_ += num;
      }
    }
  }

  if (test_mode_ & TEST_MODE_SYNC_NET_READ)
    return num;

  CallbackLater(std::move(callback), num);
  return ERR_IO_PENDING;
}

void MockNetworkTransaction::StopCaching() {
  if (transaction_factory_.get())
    transaction_factory_->TransactionStopCaching();
}

int64_t MockNetworkTransaction::GetTotalReceivedBytes() const {
  return received_bytes_;
}

int64_t MockNetworkTransaction::GetTotalSentBytes() const {
  return sent_bytes_;
}

int64_t MockNetworkTransaction::GetReceivedBodyBytes() const {
  return received_body_bytes_;
}

void MockNetworkTransaction::DoneReading() {
  CHECK(!done_reading_called_);
  done_reading_called_ = true;
  if (transaction_factory_.get())
    transaction_factory_->TransactionDoneReading();
}

const HttpResponseInfo* MockNetworkTransaction::GetResponseInfo() const {
  return &response_;
}

LoadState MockNetworkTransaction::GetLoadState() const {
  if (data_cursor_)
    return LOAD_STATE_READING_RESPONSE;
  return LOAD_STATE_IDLE;
}

void MockNetworkTransaction::SetQuicServerInfo(
    QuicServerInfo* quic_server_info) {
}

bool MockNetworkTransaction::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  if (socket_log_id_ != NetLogSource::kInvalidId) {
    // The minimal set of times for a request that gets a response, assuming it
    // gets a new socket.
    load_timing_info->socket_reused = false;
    load_timing_info->socket_log_id = socket_log_id_;
    load_timing_info->connect_timing.connect_start = base::TimeTicks::Now();
    load_timing_info->connect_timing.connect_end = base::TimeTicks::Now();
    load_timing_info->send_start = base::TimeTicks::Now();
    load_timing_info->send_end = base::TimeTicks::Now();
  } else {
    // If there's no valid socket ID, just use the generic socket reused values.
    // No tests currently depend on this, just should not match the values set
    // by a cache hit.
    load_timing_info->socket_reused = true;
    load_timing_info->send_start = base::TimeTicks::Now();
    load_timing_info->send_end = base::TimeTicks::Now();
  }
  return true;
}

bool MockNetworkTransaction::GetRemoteEndpoint(IPEndPoint* endpoint) const {
  *endpoint = IPEndPoint(IPAddress(127, 0, 0, 1), 80);
  return true;
}

void MockNetworkTransaction::SetPriority(RequestPriority priority) {
  priority_ = priority;
}

void MockNetworkTransaction::SetWebSocketHandshakeStreamCreateHelper(
    WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  websocket_handshake_stream_create_helper_ = create_helper;
}

// static
const int64_t MockNetworkTransaction::kTotalReceivedBytes = 1000;

// static
const int64_t MockNetworkTransaction::kTotalSentBytes = 100;

// static
const int64_t MockNetworkTransaction::kReceivedBodyBytes = 500;

int MockNetworkTransaction::StartInternal(HttpRequestInfo request,
                                          CompletionOnceCallback callback) {
  current_request_ = std::move(request);
  const MockTransaction* t = FindMockTransaction(current_request_.url);
  if (!t) {
    return ERR_FAILED;
  }
  test_mode_ = t->test_mode;

  // Return immediately if we're returning an error.
  if (OK != t->start_return_code) {
    if (test_mode_ & TEST_MODE_SYNC_NET_START) {
      return t->start_return_code;
    }
    CallbackLater(std::move(callback), t->start_return_code);
    return ERR_IO_PENDING;
  }

  next_state_ = State::NOTIFY_BEFORE_CREATE_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }
  return rv;
}

int MockNetworkTransaction::DoNotifyBeforeCreateStream() {
  next_state_ = State::CREATE_STREAM;
  bool defer = false;
  if (!before_network_start_callback_.is_null()) {
    std::move(before_network_start_callback_).Run(&defer);
  }
  if (!defer) {
    return OK;
  }
  return ERR_IO_PENDING;
}

int MockNetworkTransaction::DoCreateStream() {
  next_state_ = State::CREATE_STREAM_COMPLETE;
  if (test_mode_ & TEST_MODE_SYNC_NET_START) {
    return OK;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MockNetworkTransaction::OnIOComplete,
                                weak_factory_.GetWeakPtr(), OK));
  return ERR_IO_PENDING;
}

int MockNetworkTransaction::DoCreateStreamComplete(int result) {
  // We don't have a logic which simulate stream creation
  CHECK_EQ(OK, result);
  next_state_ = State::CONNECTED_CALLBACK;
  return OK;
}

int MockNetworkTransaction::DoConnectedCallback() {
  next_state_ = State::CONNECTED_CALLBACK_COMPLETE;
  if (connected_callback_.is_null()) {
    return OK;
  }

  const MockTransaction* t = FindMockTransaction(current_request_.url);
  CHECK(t);
  return connected_callback_.Run(
      t->transport_info, base::BindOnce(&MockNetworkTransaction::OnIOComplete,
                                        weak_factory_.GetWeakPtr()));
}

int MockNetworkTransaction::DoConnectedCallbackComplete(int result) {
  if (result != OK) {
    return result;
  }
  next_state_ = State::BUILD_REQUEST;
  return OK;
}

int MockNetworkTransaction::DoBuildRequest() {
  next_state_ = State::BUILD_REQUEST_COMPLETE;
  if (modify_request_headers_callback_) {
    modify_request_headers_callback_.Run(&current_request_.extra_headers);
  }
  return OK;
}

int MockNetworkTransaction::DoBuildRequestComplete(int result) {
  CHECK_EQ(OK, result);
  next_state_ = State::SEND_REQUEST;
  return OK;
}

int MockNetworkTransaction::DoSendRequest() {
  next_state_ = State::SEND_REQUEST_COMPLETE;

  sent_bytes_ = kTotalSentBytes;
  received_bytes_ = kTotalReceivedBytes;
  received_body_bytes_ = kReceivedBodyBytes;

  const MockTransaction* t = FindMockTransaction(current_request_.url);
  CHECK(t);

  std::string resp_status = t->status;
  std::string resp_headers = t->response_headers;
  std::string resp_data = t->data;

  if (t->handler) {
    t->handler.Run(&current_request_, &resp_status, &resp_headers, &resp_data);
  }
  std::string header_data =
      base::StringPrintf("%s\n%s\n", resp_status.c_str(), resp_headers.c_str());
  std::replace(header_data.begin(), header_data.end(), '\n', '\0');

  response_.request_time = transaction_factory_->Now();
  if (!t->request_time.is_null())
    response_.request_time = t->request_time;

  response_.was_cached = false;
  response_.network_accessed = true;
  response_.remote_endpoint = t->transport_info.endpoint;
  if (t->transport_info.type == TransportType::kDirect) {
    response_.proxy_chain = ProxyChain::Direct();
  } else if (t->transport_info.type == TransportType::kProxied) {
    response_.proxy_chain = ProxyChain::FromSchemeHostAndPort(
        ProxyServer::SCHEME_HTTP,
        t->transport_info.endpoint.ToStringWithoutPort(),
        t->transport_info.endpoint.port());
  }

  response_.response_time = transaction_factory_->Now();
  if (!t->response_time.is_null())
    response_.response_time = t->response_time;

  response_.headers = base::MakeRefCounted<HttpResponseHeaders>(header_data);
  response_.ssl_info.cert = t->cert;
  response_.ssl_info.cert_status = t->cert_status;
  response_.ssl_info.connection_status = t->ssl_connection_status;
  response_.dns_aliases = t->dns_aliases;
  data_ = resp_data;
  content_length_ = response_.headers->GetContentLength();

  if (net_log_.net_log()) {
    socket_log_id_ = net_log_.net_log()->NextID();
  }

  if (current_request_.load_flags & LOAD_PREFETCH) {
    response_.unused_since_prefetch = true;
  }

  if (current_request_.load_flags & LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME) {
    DCHECK(response_.unused_since_prefetch);
    response_.restricted_prefetch = true;
  }
  return OK;
}

int MockNetworkTransaction::DoSendRequestComplete(int result) {
  CHECK_EQ(OK, result);
  next_state_ = State::READ_HEADERS;
  return OK;
}

int MockNetworkTransaction::DoReadHeaders() {
  next_state_ = State::READ_HEADERS_COMPLETE;
  return OK;
}

int MockNetworkTransaction::DoReadHeadersComplete(int result) {
  CHECK_EQ(OK, result);
  return OK;
}

int MockNetworkTransaction::DoLoop(int result) {
  CHECK(next_state_ != State::NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = State::NONE;
    switch (state) {
      case State::NOTIFY_BEFORE_CREATE_STREAM:
        CHECK_EQ(OK, rv);
        rv = DoNotifyBeforeCreateStream();
        break;
      case State::CREATE_STREAM:
        CHECK_EQ(OK, rv);
        rv = DoCreateStream();
        break;
      case State::CREATE_STREAM_COMPLETE:
        rv = DoCreateStreamComplete(rv);
        break;
      case State::CONNECTED_CALLBACK:
        rv = DoConnectedCallback();
        break;
      case State::CONNECTED_CALLBACK_COMPLETE:
        rv = DoConnectedCallbackComplete(rv);
        break;
      case State::BUILD_REQUEST:
        CHECK_EQ(OK, rv);
        rv = DoBuildRequest();
        break;
      case State::BUILD_REQUEST_COMPLETE:
        rv = DoBuildRequestComplete(rv);
        break;
      case State::SEND_REQUEST:
        CHECK_EQ(OK, rv);
        rv = DoSendRequest();
        break;
      case State::SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        break;
      case State::READ_HEADERS:
        CHECK_EQ(OK, rv);
        rv = DoReadHeaders();
        break;
      case State::READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != State::NONE);

  return rv;
}

void MockNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    CHECK(callback_);
    std::move(callback_).Run(rv);
  }
}

void MockNetworkTransaction::SetBeforeNetworkStartCallback(
    BeforeNetworkStartCallback callback) {
  before_network_start_callback_ = std::move(callback);
}

void MockNetworkTransaction::SetModifyRequestHeadersCallback(
    base::RepeatingCallback<void(HttpRequestHeaders*)> callback) {
  modify_request_headers_callback_ = std::move(callback);
}

void MockNetworkTransaction::SetConnectedCallback(
    const ConnectedCallback& callback) {
  connected_callback_ = callback;
}

int MockNetworkTransaction::ResumeNetworkStart() {
  CHECK_EQ(next_state_, State::CREATE_STREAM);
  return DoLoop(OK);
}

ConnectionAttempts MockNetworkTransaction::GetConnectionAttempts() const {
  // TODO(ricea): Replace this with a proper implementation if needed.
  return {};
}

void MockNetworkTransaction::CloseConnectionOnDestruction() {
  NOTIMPLEMENTED();
}

bool MockNetworkTransaction::IsMdlMatchForMetrics() const {
  return false;
}

void MockNetworkTransaction::CallbackLater(CompletionOnceCallback callback,
                                           int result) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockNetworkTransaction::RunCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback), result));
}

void MockNetworkTransaction::RunCallback(CompletionOnceCallback callback,
                                         int result) {
  std::move(callback).Run(result);
}

MockNetworkLayer::MockNetworkLayer() = default;

MockNetworkLayer::~MockNetworkLayer() = default;

void MockNetworkLayer::TransactionDoneReading() {
  CHECK(!done_reading_called_);
  done_reading_called_ = true;
}

void MockNetworkLayer::TransactionStopCaching() {
  stop_caching_called_ = true;
}

void MockNetworkLayer::ResetTransactionCount() {
  transaction_count_ = 0;
}

int MockNetworkLayer::CreateTransaction(
    RequestPriority priority,
    std::unique_ptr<HttpTransaction>* trans) {
  transaction_count_++;
  last_create_transaction_priority_ = priority;
  auto mock_transaction =
      std::make_unique<MockNetworkTransaction>(priority, this);
  last_transaction_ = mock_transaction->AsWeakPtr();
  *trans = std::move(mock_transaction);
  return OK;
}

HttpCache* MockNetworkLayer::GetCache() {
  return nullptr;
}

HttpNetworkSession* MockNetworkLayer::GetSession() {
  return nullptr;
}

void MockNetworkLayer::SetClock(base::Clock* clock) {
  DCHECK(!clock_);
  clock_ = clock;
}

base::Time MockNetworkLayer::Now() {
  if (clock_)
    return clock_->Now();
  return base::Time::Now();
}

//-----------------------------------------------------------------------------
// helpers

int ReadTransaction(HttpTransaction* trans, std::string* result) {
  int rv;

  std::string content;
  do {
    TestCompletionCallback callback;
    auto buf = base::MakeRefCounted<IOBufferWithSize>(256);
    rv = trans->Read(buf.get(), 256, callback.callback());
    if (rv == ERR_IO_PENDING) {
      rv = callback.WaitForResult();
      base::RunLoop().RunUntilIdle();
    }

    if (rv > 0)
      content.append(buf->data(), rv);
    else if (rv < 0)
      return rv;
  } while (rv > 0);

  result->swap(content);
  return OK;
}

//-----------------------------------------------------------------------------
// connected callback handler

ConnectedHandler::ConnectedHandler() = default;
ConnectedHandler::~ConnectedHandler() = default;

ConnectedHandler::ConnectedHandler(const ConnectedHandler&) = default;
ConnectedHandler& ConnectedHandler::operator=(const ConnectedHandler&) =
    default;
ConnectedHandler::ConnectedHandler(ConnectedHandler&&) = default;
ConnectedHandler& ConnectedHandler::operator=(ConnectedHandler&&) = default;

int ConnectedHandler::OnConnected(const TransportInfo& info,
                                  CompletionOnceCallback callback) {
  transports_.push_back(info);
  if (run_callback_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result_));
    return ERR_IO_PENDING;
  }
  return result_;
}

}  // namespace net
