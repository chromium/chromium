// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "net/base/cache_type.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/base/tracing.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_cache_transaction.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_headers_test_util.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/http_util.h"
#include "net/http/mock_http_cache.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/client_socket_handle.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/scoped_mutually_exclusive_feature_list.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using net::test::IsError;
using net::test::IsOk;
using testing::AllOf;
using testing::ByRef;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::Gt;
using testing::IsEmpty;
using testing::NotNull;

using base::Time;

namespace net {

using CacheEntryStatus = HttpResponseInfo::CacheEntryStatus;

class WebSocketEndpointLockManager;

namespace {

constexpr auto ToSimpleString = test::HttpResponseHeadersToSimpleString;

// Tests the load timing values of a request that goes through a
// MockNetworkTransaction.
void TestLoadTimingNetworkRequest(const LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());

  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);
  EXPECT_LE(load_timing_info.connect_timing.connect_end,
            load_timing_info.send_start);

  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);

  // Set by URLRequest / URLRequestHttpJob, at a higher level.
  EXPECT_TRUE(load_timing_info.request_start_time.is_null());
  EXPECT_TRUE(load_timing_info.request_start.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

// Tests the load timing values of a request that receives a cached response.
void TestLoadTimingCachedResponse(const LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_EQ(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);

  // Only the send start / end times should be sent, and they should have the
  // same value.
  EXPECT_FALSE(load_timing_info.send_start.is_null());
  EXPECT_EQ(load_timing_info.send_start, load_timing_info.send_end);

  // Set by URLRequest / URLRequestHttpJob, at a higher level.
  EXPECT_TRUE(load_timing_info.request_start_time.is_null());
  EXPECT_TRUE(load_timing_info.request_start.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

void DeferCallback(bool* defer) {
  *defer = true;
}

class DeleteCacheCompletionCallback
    : public TestGetBackendCompletionCallbackBase {
 public:
  explicit DeleteCacheCompletionCallback(std::unique_ptr<MockHttpCache> cache)
      : cache_(std::move(cache)) {}

  DeleteCacheCompletionCallback(const DeleteCacheCompletionCallback&) = delete;
  DeleteCacheCompletionCallback& operator=(
      const DeleteCacheCompletionCallback&) = delete;

  HttpCache::GetBackendCallback callback() {
    return base::BindOnce(&DeleteCacheCompletionCallback::OnComplete,
                          base::Unretained(this));
  }

 private:
  void OnComplete(HttpCache::GetBackendResult result) {
    result.second = nullptr;  // would dangle on next line otherwise.
    cache_.reset();
    SetResult(result);
  }

  std::unique_ptr<MockHttpCache> cache_;
};

//-----------------------------------------------------------------------------
// helpers

void ReadAndVerifyTransaction(HttpTransaction* trans,
                              const MockTransaction& trans_info) {
  std::string content;
  int rv = ReadTransaction(trans, &content);

  EXPECT_THAT(rv, IsOk());
  std::string expected(trans_info.data);
  EXPECT_EQ(expected, content);
}

void ReadRemainingAndVerifyTransaction(HttpTransaction* trans,
                                       const std::string& already_read,
                                       const MockTransaction& trans_info) {
  std::string content;
  int rv = ReadTransaction(trans, &content);
  EXPECT_THAT(rv, IsOk());

  std::string expected(trans_info.data);
  EXPECT_EQ(expected, already_read + content);
}

void RunTransactionTestBase(HttpCache* cache,
                            const MockTransaction& trans_info,
                            const MockHttpRequest& request,
                            HttpResponseInfo* response_info,
                            const NetLogWithSource& net_log,
                            LoadTimingInfo* load_timing_info,
                            int64_t* sent_bytes,
                            int64_t* received_bytes,
                            IPEndPoint* remote_endpoint) {
  TestCompletionCallback callback;

  // write to the cache

  std::unique_ptr<HttpTransaction> trans;
  int rv = cache->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, callback.callback(), net_log);
  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
  }
  ASSERT_EQ(trans_info.start_return_code, rv);

  if (OK != rv) {
    return;
  }

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response);

  if (response_info) {
    *response_info = *response;
  }

  if (load_timing_info) {
    // If a fake network connection is used, need a NetLog to get a fake socket
    // ID.
    EXPECT_TRUE(net_log.net_log());
    *load_timing_info = LoadTimingInfo();
    trans->GetLoadTimingInfo(load_timing_info);
  }

  if (remote_endpoint) {
    ASSERT_TRUE(trans->GetRemoteEndpoint(remote_endpoint));
  }

  ReadAndVerifyTransaction(trans.get(), trans_info);

  if (sent_bytes) {
    *sent_bytes = trans->GetTotalSentBytes();
  }
  if (received_bytes) {
    *received_bytes = trans->GetTotalReceivedBytes();
  }
}

void RunTransactionTestWithRequest(HttpCache* cache,
                                   const MockTransaction& trans_info,
                                   const MockHttpRequest& request,
                                   HttpResponseInfo* response_info) {
  RunTransactionTestBase(cache, trans_info, request, response_info,
                         NetLogWithSource(), nullptr, nullptr, nullptr,
                         nullptr);
}

void RunTransactionTestAndGetTiming(HttpCache* cache,
                                    const MockTransaction& trans_info,
                                    const NetLogWithSource& log,
                                    LoadTimingInfo* load_timing_info) {
  RunTransactionTestBase(cache, trans_info, MockHttpRequest(trans_info),
                         nullptr, log, load_timing_info, nullptr, nullptr,
                         nullptr);
}

void RunTransactionTestAndGetTimingAndConnectedSocketAddress(
    HttpCache* cache,
    const MockTransaction& trans_info,
    const NetLogWithSource& log,
    LoadTimingInfo* load_timing_info,
    IPEndPoint* remote_endpoint) {
  RunTransactionTestBase(cache, trans_info, MockHttpRequest(trans_info),
                         nullptr, log, load_timing_info, nullptr, nullptr,
                         remote_endpoint);
}

void RunTransactionTest(HttpCache* cache, const MockTransaction& trans_info) {
  RunTransactionTestAndGetTiming(cache, trans_info, NetLogWithSource(),
                                 nullptr);
}

void RunTransactionTestWithLog(HttpCache* cache,
                               const MockTransaction& trans_info,
                               const NetLogWithSource& log) {
  RunTransactionTestAndGetTiming(cache, trans_info, log, nullptr);
}

void RunTransactionTestWithResponseInfo(HttpCache* cache,
                                        const MockTransaction& trans_info,
                                        HttpResponseInfo* response) {
  RunTransactionTestWithRequest(cache, trans_info, MockHttpRequest(trans_info),
                                response);
}

void RunTransactionTestWithResponseInfoAndGetTiming(
    HttpCache* cache,
    const MockTransaction& trans_info,
    HttpResponseInfo* response,
    const NetLogWithSource& log,
    LoadTimingInfo* load_timing_info) {
  RunTransactionTestBase(cache, trans_info, MockHttpRequest(trans_info),
                         response, log, load_timing_info, nullptr, nullptr,
                         nullptr);
}

void RunTransactionTestWithResponse(HttpCache* cache,
                                    const MockTransaction& trans_info,
                                    std::string* response_headers) {
  HttpResponseInfo response;
  RunTransactionTestWithResponseInfo(cache, trans_info, &response);
  *response_headers = ToSimpleString(response.headers);
}

void RunTransactionTestWithResponseAndGetTiming(
    HttpCache* cache,
    const MockTransaction& trans_info,
    std::string* response_headers,
    const NetLogWithSource& log,
    LoadTimingInfo* load_timing_info) {
  HttpResponseInfo response;
  RunTransactionTestBase(cache, trans_info, MockHttpRequest(trans_info),
                         &response, log, load_timing_info, nullptr, nullptr,
                         nullptr);
  *response_headers = ToSimpleString(response.headers);
}

// This class provides a handler for kFastNoStoreGET_Transaction so that the
// no-store header can be included on demand.
class FastTransactionServer {
 public:
  FastTransactionServer() { no_store = false; }

  FastTransactionServer(const FastTransactionServer&) = delete;
  FastTransactionServer& operator=(const FastTransactionServer&) = delete;

  ~FastTransactionServer() = default;

  void set_no_store(bool value) { no_store = value; }

  static void FastNoStoreHandler(const HttpRequestInfo* request,
                                 std::string* response_status,
                                 std::string* response_headers,
                                 std::string* response_data) {
    if (no_store) {
      *response_headers = "Cache-Control: no-store\n";
    }
  }

 private:
  static bool no_store;
};
bool FastTransactionServer::no_store;

const MockTransaction kFastNoStoreGET_Transaction = {
    "http://www.google.com/nostore",
    "GET",
    base::Time(),
    "",
    LOAD_VALIDATE_CACHE,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n",
    base::Time(),
    "<html><body>Google Blah Blah</body></html>",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_SYNC_NET_START,
    base::BindRepeating(&FastTransactionServer::FastNoStoreHandler),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
};

// This class provides a handler for kRangeGET_TransactionOK so that the range
// request can be served on demand.
class RangeTransactionServer {
 public:
  RangeTransactionServer() {
    not_modified_ = false;
    modified_ = false;
    bad_200_ = false;
    redirect_ = false;
    length_ = 80;
  }

  RangeTransactionServer(const RangeTransactionServer&) = delete;
  RangeTransactionServer& operator=(const RangeTransactionServer&) = delete;

  ~RangeTransactionServer() {
    not_modified_ = false;
    modified_ = false;
    bad_200_ = false;
    redirect_ = false;
    length_ = 80;
  }

  // Returns only 416 or 304 when set.
  void set_not_modified(bool value) { not_modified_ = value; }

  // Returns 206 when revalidating a range (instead of 304).
  void set_modified(bool value) { modified_ = value; }

  // Returns 200 instead of 206 (a malformed response overall).
  void set_bad_200(bool value) { bad_200_ = value; }

  // Sets how long the resource is. (Default is 80)
  void set_length(int64_t length) { length_ = length; }

  // Sets whether to return a 301 instead of normal return.
  void set_redirect(bool redirect) { redirect_ = redirect; }

  // Other than regular range related behavior (and the flags mentioned above),
  // the server reacts to requests headers like so:
  //   X-Require-Mock-Auth -> return 401.
  //   X-Require-Mock-Auth-Alt -> return 401.
  //   X-Return-Default-Range -> assume 40-49 was requested.
  // The -Alt variant doesn't cause the MockNetworkTransaction to
  // report that it IsReadyToRestartForAuth().
  static void RangeHandler(const HttpRequestInfo* request,
                           std::string* response_status,
                           std::string* response_headers,
                           std::string* response_data);

 private:
  static bool not_modified_;
  static bool modified_;
  static bool bad_200_;
  static bool redirect_;
  static int64_t length_;
};
bool RangeTransactionServer::not_modified_ = false;
bool RangeTransactionServer::modified_ = false;
bool RangeTransactionServer::bad_200_ = false;
bool RangeTransactionServer::redirect_ = false;
int64_t RangeTransactionServer::length_ = 80;

// A dummy extra header that must be preserved on a given request.

// EXTRA_HEADER_LINE doesn't include a line terminator because it
// will be passed to AddHeaderFromString() which doesn't accept them.
#define EXTRA_HEADER_LINE "Extra: header"

// EXTRA_HEADER contains a line terminator, as expected by
// AddHeadersFromString() (_not_ AddHeaderFromString()).
#define EXTRA_HEADER EXTRA_HEADER_LINE "\r\n"

static const char kExtraHeaderKey[] = "Extra";

// Static.
void RangeTransactionServer::RangeHandler(const HttpRequestInfo* request,
                                          std::string* response_status,
                                          std::string* response_headers,
                                          std::string* response_data) {
  if (request->extra_headers.IsEmpty()) {
    response_status->assign("HTTP/1.1 416 Requested Range Not Satisfiable");
    response_data->clear();
    return;
  }

  // We want to make sure we don't delete extra headers.
  EXPECT_TRUE(request->extra_headers.HasHeader(kExtraHeaderKey));

  bool require_auth =
      request->extra_headers.HasHeader("X-Require-Mock-Auth") ||
      request->extra_headers.HasHeader("X-Require-Mock-Auth-Alt");

  if (require_auth && !request->extra_headers.HasHeader("Authorization")) {
    response_status->assign("HTTP/1.1 401 Unauthorized");
    response_data->assign("WWW-Authenticate: Foo\n");
    return;
  }

  if (redirect_) {
    response_status->assign("HTTP/1.1 301 Moved Permanently");
    response_headers->assign("Location: /elsewhere\nContent-Length: 5");
    response_data->assign("12345");
    return;
  }

  if (not_modified_) {
    response_status->assign("HTTP/1.1 304 Not Modified");
    response_data->clear();
    return;
  }

  std::vector<HttpByteRange> ranges;
  std::optional<std::string> range_header =
      request->extra_headers.GetHeader(HttpRequestHeaders::kRange);
  if (!range_header || !HttpUtil::ParseRangeHeader(*range_header, &ranges) ||
      bad_200_ || ranges.size() != 1 ||
      (modified_ && request->extra_headers.HasHeader("If-Range"))) {
    // This is not a byte range request, or a failed If-Range. We return 200.
    response_status->assign("HTTP/1.1 200 OK");
    response_headers->assign("Date: Wed, 28 Nov 2007 09:40:09 GMT");
    response_data->assign("Not a range");
    return;
  }

  // We can handle this range request.
  HttpByteRange byte_range = ranges[0];

  if (request->extra_headers.HasHeader("X-Return-Default-Range")) {
    byte_range.set_first_byte_position(40);
    byte_range.set_last_byte_position(49);
  }

  if (byte_range.first_byte_position() >= length_) {
    response_status->assign("HTTP/1.1 416 Requested Range Not Satisfiable");
    response_data->clear();
    return;
  }

  EXPECT_TRUE(byte_range.ComputeBounds(length_));
  int64_t start = byte_range.first_byte_position();
  int64_t end = byte_range.last_byte_position();

  EXPECT_LT(end, length_);

  std::string content_range = base::StringPrintf("Content-Range: bytes %" PRId64
                                                 "-%" PRId64 "/%" PRId64 "\n",
                                                 start, end, length_);
  response_headers->append(content_range);

  if (!request->extra_headers.HasHeader("If-None-Match") || modified_) {
    std::string data;
    if (end == start) {
      EXPECT_EQ(0, end % 10);
      data = "r";
    } else {
      EXPECT_EQ(9, (end - start) % 10);
      for (int64_t block_start = start; block_start < end; block_start += 10) {
        base::StringAppendF(&data, "rg: %02" PRId64 "-%02" PRId64 " ",
                            block_start % 100, (block_start + 9) % 100);
      }
    }
    *response_data = data;

    if (end - start != 9) {
      // We also have to fix content-length.
      int64_t len = end - start + 1;
      std::string content_length =
          base::StringPrintf("Content-Length: %" PRId64 "\n", len);
      response_headers->replace(response_headers->find("Content-Length:"),
                                content_length.size(), content_length);
    }
  } else {
    response_status->assign("HTTP/1.1 304 Not Modified");
    response_data->clear();
  }
}

const MockTransaction kRangeGET_TransactionOK = {
    "http://www.google.com/range",
    "GET",
    base::Time(),
    "Range: bytes = 40-49\r\n" EXTRA_HEADER,
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 206 Partial Content",
    "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
    "ETag: \"foo\"\n"
    "Accept-Ranges: bytes\n"
    "Content-Length: 10\n",
    base::Time(),
    "rg: 40-49 ",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    base::BindRepeating(&RangeTransactionServer::RangeHandler),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
};

const char kFullRangeData[] =
    "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 "
    "rg: 40-49 rg: 50-59 rg: 60-69 rg: 70-79 ";

// Verifies the response headers (|response|) match a partial content
// response for the range starting at |start| and ending at |end|.
void Verify206Response(const std::string& response, int start, int end) {
  auto headers = base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(response));

  ASSERT_EQ(206, headers->response_code());

  int64_t range_start, range_end, object_size;
  ASSERT_TRUE(
      headers->GetContentRangeFor206(&range_start, &range_end, &object_size));
  int64_t content_length = headers->GetContentLength();

  int length = end - start + 1;
  ASSERT_EQ(length, content_length);
  ASSERT_EQ(start, range_start);
  ASSERT_EQ(end, range_end);
}

// Creates a truncated entry that can be resumed using byte ranges.
void CreateTruncatedEntry(std::string raw_headers, MockHttpCache* cache) {
  // Create a disk cache entry that stores an incomplete resource.
  disk_cache::Entry* entry;
  MockHttpRequest request(kRangeGET_TransactionOK);
  ASSERT_TRUE(cache->CreateBackendEntry(request.CacheKey(), &entry, nullptr));

  HttpResponseInfo response;
  response.response_time = base::Time::Now();
  response.request_time = base::Time::Now();
  response.headers = base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(raw_headers));
  // Set the last argument for this to be an incomplete request.
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, true));

  auto buf = base::MakeRefCounted<IOBufferWithSize>(100);
  int len =
      static_cast<int>(base::strlcpy(buf->data(), "rg: 00-09 rg: 10-19 ", 100));
  TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  EXPECT_EQ(len, cb.GetResult(rv));
  entry->Close();
}

// Verifies that there's an entry with this |key| with the truncated flag set to
// |flag_value|, and with an optional |data_size| (if not zero).
void VerifyTruncatedFlag(MockHttpCache* cache,
                         const std::string& key,
                         bool flag_value,
                         int data_size) {
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache->OpenBackendEntry(key, &entry));
  disk_cache::ScopedEntryPtr closer(entry);

  HttpResponseInfo response;
  bool truncated = !flag_value;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_EQ(flag_value, truncated);
  if (data_size) {
    EXPECT_EQ(data_size, entry->GetDataSize(1));
  }
}

// Helper to represent a network HTTP response.
struct Response {
  // Set this response into |trans|.
  void AssignTo(MockTransaction* trans) const {
    trans->status = status;
    trans->response_headers = headers;
    trans->data = body;
  }

  std::string status_and_headers() const {
    return std::string(status) + "\n" + std::string(headers);
  }

  const char* status;
  const char* headers;
  const char* body;
};

struct Context {
  Context() = default;

  int result = ERR_IO_PENDING;
  TestCompletionCallback callback;
  std::unique_ptr<HttpTransaction> trans;
};

class FakeWebSocketHandshakeStreamCreateHelper
    : public WebSocketHandshakeStreamBase::CreateHelper {
 public:
  ~FakeWebSocketHandshakeStreamCreateHelper() override = default;
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateBasicStream(
      std::unique_ptr<ClientSocketHandle> connect,
      bool using_proxy,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager) override {
    return nullptr;
  }
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp2Stream(
      base::WeakPtr<SpdySession> session,
      std::set<std::string> dns_aliases) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp3Stream(
      std::unique_ptr<QuicChromiumClientSession::Handle> session,
      std::set<std::string> dns_aliases) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
};

// Returns true if |entry| is not one of the log types paid attention to in this
// test. Note that HTTP_CACHE_WRITE_INFO and HTTP_CACHE_*_DATA are
// ignored.
bool ShouldIgnoreLogEntry(const NetLogEntry& entry) {
  switch (entry.type) {
    case NetLogEventType::HTTP_CACHE_GET_BACKEND:
    case NetLogEventType::HTTP_CACHE_OPEN_OR_CREATE_ENTRY:
    case NetLogEventType::HTTP_CACHE_OPEN_ENTRY:
    case NetLogEventType::HTTP_CACHE_CREATE_ENTRY:
    case NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY:
    case NetLogEventType::HTTP_CACHE_DOOM_ENTRY:
    case NetLogEventType::HTTP_CACHE_READ_INFO:
      return false;
    default:
      return true;
  }
}

// Gets the entries from |net_log| created by the cache layer and asserted on in
// these tests.
std::vector<NetLogEntry> GetFilteredNetLogEntries(
    const RecordingNetLogObserver& net_log_observer) {
  auto entries = net_log_observer.GetEntries();
  std::erase_if(entries, ShouldIgnoreLogEntry);
  return entries;
}

bool LogContainsEventType(const RecordingNetLogObserver& net_log_observer,
                          NetLogEventType expected) {
  return !net_log_observer.GetEntriesWithType(expected).empty();
}

// Returns a TransportInfo distinct from the default for mock transactions,
// with the given port number.
TransportInfo TestTransportInfoWithPort(uint16_t port) {
  TransportInfo result;
  result.endpoint = IPEndPoint(IPAddress(42, 0, 1, 2), port);
  return result;
}

// Returns a TransportInfo distinct from the default for mock transactions.
TransportInfo TestTransportInfo() {
  return TestTransportInfoWithPort(1337);
}

TransportInfo CachedTestTransportInfo() {
  TransportInfo result = TestTransportInfo();
  result.type = TransportType::kCached;
  return result;
}

// Helper function, generating valid HTTP cache key from `url`.
// See also: HttpCache::GenerateCacheKey(..)
std::string GenerateCacheKey(const std::string& url) {
  return "1/0/" + url;
}

}  // namespace

using HttpCacheTest = TestWithTaskEnvironment;

class HttpCacheIOCallbackTest : public HttpCacheTest {
 public:
  HttpCacheIOCallbackTest() = default;
  ~HttpCacheIOCallbackTest() override = default;

  // HttpCache::ActiveEntry is private, doing this allows tests to use it
  using ActiveEntry = HttpCache::ActiveEntry;
  using Transaction = HttpCache::Transaction;

  // The below functions are forwarding calls to the HttpCache class.
  int OpenEntry(HttpCache* cache,
                const std::string& url,
                scoped_refptr<ActiveEntry>* entry,
                HttpCache::Transaction* trans) {
    return cache->OpenEntry(GenerateCacheKey(url), entry, trans);
  }

  int OpenOrCreateEntry(HttpCache* cache,
                        const std::string& url,
                        scoped_refptr<ActiveEntry>* entry,
                        HttpCache::Transaction* trans) {
    return cache->OpenOrCreateEntry(GenerateCacheKey(url), entry, trans);
  }

  int CreateEntry(HttpCache* cache,
                  const std::string& url,
                  scoped_refptr<ActiveEntry>* entry,
                  HttpCache::Transaction* trans) {
    return cache->CreateEntry(GenerateCacheKey(url), entry, trans);
  }

  int DoomEntry(HttpCache* cache,
                const std::string& url,
                HttpCache::Transaction* trans) {
    return cache->DoomEntry(GenerateCacheKey(url), trans);
  }
};

class HttpSplitCacheKeyTest : public HttpCacheTest {
 public:
  HttpSplitCacheKeyTest() = default;
  ~HttpSplitCacheKeyTest() override = default;

  std::string ComputeCacheKey(const std::string& url_string) {
    GURL url(url_string);
    SchemefulSite site(url);
    HttpRequestInfo request_info;
    request_info.url = url;
    request_info.method = "GET";
    request_info.network_isolation_key = NetworkIsolationKey(site, site);
    request_info.network_anonymization_key =
        NetworkAnonymizationKey::CreateSameSite(site);
    MockHttpCache cache;
    return *HttpCache::GenerateCacheKeyForRequest(&request_info);
  }
};

//-----------------------------------------------------------------------------
// Tests.

TEST_F(HttpCacheTest, CreateThenDestroy) {
  MockHttpCache cache;

  std::unique_ptr<HttpTransaction> trans;
  EXPECT_THAT(cache.CreateTransaction(&trans), IsOk());
  ASSERT_TRUE(trans.get());
}

TEST_F(HttpCacheTest, GetBackend) {
  MockHttpCache cache(HttpCache::DefaultBackend::InMemory(0));

  TestGetBackendCompletionCallback cb;
  // This will lazily initialize the backend.
  HttpCache::GetBackendResult result =
      cache.http_cache()->GetBackend(cb.callback());
  EXPECT_THAT(cb.GetResult(result).first, IsOk());
}

using HttpCacheSimpleGetTest = HttpCacheTest;

TEST_F(HttpCacheSimpleGetTest, Basic) {
  MockHttpCache cache;
  LoadTimingInfo load_timing_info;

  // Write to the cache.
  RunTransactionTestAndGetTiming(cache.http_cache(), kSimpleGET_Transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// This test verifies that the callback passed to SetConnectedCallback() is
// called once for simple GET calls that traverse the cache.
TEST_F(HttpCacheSimpleGetTest, ConnectedCallback) {
  MockHttpCache cache;

  ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
  mock_transaction.transport_info = TestTransportInfo();
  MockHttpRequest request(mock_transaction);

  ConnectedHandler connected_handler;

  std::unique_ptr<HttpTransaction> transaction;
  EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
  ASSERT_THAT(transaction, NotNull());

  transaction->SetConnectedCallback(connected_handler.Callback());

  TestCompletionCallback callback;
  ASSERT_THAT(
      transaction->Start(&request, callback.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_THAT(connected_handler.transports(), ElementsAre(TestTransportInfo()));
}

// This test verifies that when the callback passed to SetConnectedCallback()
// returns an error, the transaction fails with that error.
TEST_F(HttpCacheSimpleGetTest, ConnectedCallbackReturnError) {
  MockHttpCache cache;
  MockHttpRequest request(kSimpleGET_Transaction);
  ConnectedHandler connected_handler;

  std::unique_ptr<HttpTransaction> transaction;
  EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
  ASSERT_THAT(transaction, NotNull());

  // The exact error code does not matter. We only care that it is passed to
  // the transaction's completion callback unmodified.
  connected_handler.set_result(ERR_NOT_IMPLEMENTED);
  transaction->SetConnectedCallback(connected_handler.Callback());

  TestCompletionCallback callback;
  ASSERT_THAT(
      transaction->Start(&request, callback.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_NOT_IMPLEMENTED));
}

// This test verifies that the callback passed to SetConnectedCallback() is
// called once for requests that hit the cache.
TEST_F(HttpCacheSimpleGetTest, ConnectedCallbackOnCacheHit) {
  MockHttpCache cache;

  {
    // Populate the cache.
    ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
    mock_transaction.transport_info = TestTransportInfo();
    RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);
  }

  // Establish a baseline.
  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Load from the cache (only), observe the callback being called.

  ConnectedHandler connected_handler;
  MockHttpRequest request(kSimpleGET_Transaction);

  std::unique_ptr<HttpTransaction> transaction;
  EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
  ASSERT_THAT(transaction, NotNull());

  transaction->SetConnectedCallback(connected_handler.Callback());

  TestCompletionCallback callback;
  ASSERT_THAT(
      transaction->Start(&request, callback.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Still only 1 transaction for the previous request. The connected callback
  // was not called by a second network transaction.
  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  EXPECT_THAT(connected_handler.transports(),
              ElementsAre(CachedTestTransportInfo()));
}

// This test verifies that when the callback passed to SetConnectedCallback()
// is called for a request that hit the cache and returns an error, the cache
// entry is reusable.
TEST_F(HttpCacheSimpleGetTest, ConnectedCallbackOnCacheHitReturnError) {
  MockHttpCache cache;

  {
    // Populate the cache.
    ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
    mock_transaction.transport_info = TestTransportInfo();
    RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);
  }

  MockHttpRequest request(kSimpleGET_Transaction);

  {
    // Attempt to read from cache entry, but abort transaction due to a
    // connected callback error.
    ConnectedHandler connected_handler;
    connected_handler.set_result(ERR_FAILED);

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsError(ERR_FAILED));

    // Used the cache entry only.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(CachedTestTransportInfo()));
  }

  {
    // Request the same resource once more, observe that it is read from cache.
    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    // Used the cache entry only.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(CachedTestTransportInfo()));
  }
}

// This test verifies that when the callback passed to SetConnectedCallback()
// returns `ERR_INCONSISTENT_IP_ADDRESS_SPACE`, the cache entry is invalidated.
TEST_F(HttpCacheSimpleGetTest,
       ConnectedCallbackOnCacheHitReturnInconsistentIpError) {
  MockHttpCache cache;

  ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
  mock_transaction.transport_info = TestTransportInfo();

  // Populate the cache.
  RunTransactionTest(cache.http_cache(), mock_transaction);

  MockHttpRequest request(kSimpleGET_Transaction);

  {
    // Attempt to read from cache entry, but abort transaction due to a
    // connected callback error.
    ConnectedHandler connected_handler;
    connected_handler.set_result(ERR_INCONSISTENT_IP_ADDRESS_SPACE);

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(),
                IsError(ERR_INCONSISTENT_IP_ADDRESS_SPACE));

    // Used the cache entry only.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(CachedTestTransportInfo()));
  }

  {
    // Request the same resource once more, observe that it is not read from
    // cache.
    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    // Used the network only.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo()));
  }
}

// This test verifies that when the callback passed to SetConnectedCallback()
// returns
// `ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_POLICY`, the
// cache entry is invalidated, and we'll retry the connection from the network.
TEST_F(HttpCacheSimpleGetTest,
       ConnectedCallbackOnCacheHitReturnPrivateNetworkAccessBlockedError) {
  MockHttpCache cache;

  ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
  mock_transaction.transport_info = TestTransportInfo();

  // Populate the cache.
  RunTransactionTest(cache.http_cache(), mock_transaction);

  MockHttpRequest request(kSimpleGET_Transaction);

  {
    // Attempt to read from cache entry, but abort transaction due to a
    // connected callback error.
    ConnectedHandler connected_handler;
    connected_handler.set_result(
        ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_POLICY);

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(
        callback.WaitForResult(),
        IsError(
            ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_POLICY));

    // Used the cache entry only.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(CachedTestTransportInfo(), TestTransportInfo()));
  }

  {
    // Request the same resource once more, observe that it is not read from
    // cache.
    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    // Used the network only.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo()));
  }
}

// This test verifies that the callback passed to SetConnectedCallback() is
// called with the right transport type when the cached entry was originally
// fetched via proxy.
TEST_F(HttpCacheSimpleGetTest, ConnectedCallbackOnCacheHitFromProxy) {
  MockHttpCache cache;

  TransportInfo proxied_transport_info = TestTransportInfo();
  proxied_transport_info.type = TransportType::kProxied;

  {
    // Populate the cache.
    ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
    mock_transaction.transport_info = proxied_transport_info;
    RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);
  }

  // Establish a baseline.
  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Load from the cache (only), observe the callback being called.

  ConnectedHandler connected_handler;
  MockHttpRequest request(kSimpleGET_Transaction);

  std::unique_ptr<HttpTransaction> transaction;
  EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
  ASSERT_THAT(transaction, NotNull());

  transaction->SetConnectedCallback(connected_handler.Callback());

  TestCompletionCallback callback;
  ASSERT_THAT(
      transaction->Start(&request, callback.callback(), NetLogWithSource()),
      IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Still only 1 transaction for the previous request. The connected callback
  // was not called by a second network transaction.
  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // The transport info mentions both the cache and the original proxy.
  TransportInfo expected_transport_info = TestTransportInfo();
  expected_transport_info.type = TransportType::kCachedFromProxy;

  EXPECT_THAT(connected_handler.transports(),
              ElementsAre(expected_transport_info));
}

TEST_F(HttpCacheSimpleGetTest, DelayedCacheLock) {
  MockHttpCache cache;
  LoadTimingInfo load_timing_info;

  // Configure the cache to delay the response for AddTransactionToEntry so it
  // gets sequenced behind any other tasks that get generated when starting the
  // transaction (i.e. network activity when run in parallel with the cache
  // lock).
  cache.http_cache()->DelayAddTransactionToEntryForTesting();

  // Write to the cache.
  RunTransactionTestAndGetTiming(cache.http_cache(), kSimpleGET_Transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

TEST_F(HttpCacheTest, GetExperimentMode) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean,
             net::features::kSplitCacheByMainFrameNavigationInitiator,
             net::features::kSplitCacheByNavigationInitiator});

    EXPECT_EQ(HttpCache::ExperimentMode::kStandard,
              HttpCache::GetExperimentMode());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean},
        {net::features::kSplitCacheByMainFrameNavigationInitiator,
         net::features::kSplitCacheByNavigationInitiator});

    EXPECT_EQ(HttpCache::ExperimentMode::kCrossSiteInitiatorBoolean,
              HttpCache::GetExperimentMode());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {net::features::kSplitCacheByMainFrameNavigationInitiator},
        {net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean,
         net::features::kSplitCacheByNavigationInitiator});

    EXPECT_EQ(HttpCache::ExperimentMode::kMainFrameNavigationInitiator,
              HttpCache::GetExperimentMode());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {net::features::kSplitCacheByNavigationInitiator},
        {net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean,
         net::features::kSplitCacheByMainFrameNavigationInitiator});

    EXPECT_EQ(HttpCache::ExperimentMode::kNavigationInitiator,
              HttpCache::GetExperimentMode());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean,
         net::features::kSplitCacheByMainFrameNavigationInitiator},
        {net::features::kSplitCacheByNavigationInitiator});

    EXPECT_EQ(HttpCache::ExperimentMode::kStandard,
              HttpCache::GetExperimentMode());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {net::features::kSplitCacheByMainFrameNavigationInitiator,
         net::features::kSplitCacheByNavigationInitiator},
        {net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean});

    EXPECT_EQ(HttpCache::ExperimentMode::kStandard,
              HttpCache::GetExperimentMode());
  }
}

enum class SplitCacheTestCase {
  kDisabled,
  kEnabledTripleKeyed,
  kEnabledTriplePlusCrossSiteMainFrameNavBool,
  kEnabledTriplePlusMainFrameNavInitiator,
  kEnabledTriplePlusNavInitiator
};

const struct {
  const SplitCacheTestCase test_case;
  base::test::FeatureRef feature;
} kTestCaseToFeatureMapping[] = {
    {SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
     net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean},
    {SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
     net::features::kSplitCacheByMainFrameNavigationInitiator},
    {SplitCacheTestCase::kEnabledTriplePlusNavInitiator,
     net::features::kSplitCacheByNavigationInitiator}};

class HttpCacheTestSplitCacheFeature
    : public HttpCacheTest,
      public ::testing::WithParamInterface<SplitCacheTestCase> {
 public:
  HttpCacheTestSplitCacheFeature()
      : split_cache_experiment_feature_list_(GetParam(),
                                             kTestCaseToFeatureMapping) {
    if (IsSplitCacheEnabled()) {
      split_cache_enabled_feature_list_.InitAndEnableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    } else {
      split_cache_enabled_feature_list_.InitAndDisableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    }
  }

  bool IsSplitCacheEnabled() const {
    return GetParam() != SplitCacheTestCase::kDisabled;
  }

 private:
  net::test::ScopedMutuallyExclusiveFeatureList
      split_cache_experiment_feature_list_;
  base::test::ScopedFeatureList split_cache_enabled_feature_list_;
};

TEST_P(HttpCacheTestSplitCacheFeature, SimpleGetVerifyGoogleFontMetrics) {
  SchemefulSite site_a(GURL("http://www.a.com"));

  MockHttpCache cache;

  ScopedMockTransaction transaction(
      kSimpleGET_Transaction,
      "http://themes.googleusercontent.com/static/fonts/roboto");
  MockHttpRequest request(transaction);
  request.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  request.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                nullptr);

  RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HttpCacheTestSplitCacheFeature,
    testing::ValuesIn(
        {SplitCacheTestCase::kDisabled, SplitCacheTestCase::kEnabledTripleKeyed,
         SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
         SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
         SplitCacheTestCase::kEnabledTriplePlusNavInitiator}),
    [](const testing::TestParamInfo<SplitCacheTestCase>& info) {
      switch (info.param) {
        case SplitCacheTestCase::kDisabled:
          return "SplitCacheDisabled";
        case SplitCacheTestCase::kEnabledTripleKeyed:
          return "SplitCacheNikFrameSiteEnabled";
        case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
          return "SplitCacheEnabledTriplePlusCrossSiteMainFrameNavigationBool";
        case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
          return "SplitCacheEnabledTriplePlusMainFrameNavigationInitiator";
        case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
          return "SplitCacheEnabledTriplePlusNavigationInitiator";
      }
    });

class HttpCacheTestSplitCacheFeatureEnabled : public HttpCacheTest {
 public:
  HttpCacheTestSplitCacheFeatureEnabled() {
    split_cache_always_enabled_feature_list_.InitAndEnableFeature(
        features::kSplitCacheByNetworkIsolationKey);
  }

 private:
  base::test::ScopedFeatureList split_cache_always_enabled_feature_list_;
};

TEST_F(HttpCacheSimpleGetTest, NoDiskCache) {
  MockHttpCache cache;

  cache.disk_cache()->set_fail_requests(true);

  RecordingNetLogObserver net_log_observer;
  LoadTimingInfo load_timing_info;

  // Read from the network, and don't use the cache.
  RunTransactionTestAndGetTiming(cache.http_cache(), kSimpleGET_Transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  // Check that the NetLog was filled as expected.
  // (We attempted to OpenOrCreate entries, but fail).
  auto entries = GetFilteredNetLogEntries(net_log_observer);

  EXPECT_EQ(4u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(entries, 0,
                                    NetLogEventType::HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(
      LogContainsEndEvent(entries, 1, NetLogEventType::HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 2, NetLogEventType::HTTP_CACHE_OPEN_OR_CREATE_ENTRY));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 3, NetLogEventType::HTTP_CACHE_OPEN_OR_CREATE_ENTRY));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

TEST_F(HttpCacheSimpleGetTest, NoDiskCache2) {
  // This will initialize a cache object with NULL backend.
  auto factory = std::make_unique<MockBlockingBackendFactory>();
  factory->set_fail(true);
  factory->FinishCreation();  // We'll complete synchronously.
  MockHttpCache cache(std::move(factory));

  // Read from the network, and don't use the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_FALSE(cache.http_cache()->GetCurrentBackend());
}

// Tests that IOBuffers are not referenced after IO completes.
TEST_F(HttpCacheTest, ReleaseBuffer) {
  MockHttpCache cache;

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  MockHttpRequest request(kSimpleGET_Transaction);
  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

  const int kBufferSize = 10;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
  ReleaseBufferCompletionCallback cb(buffer.get());

  int rv = trans->Start(&request, cb.callback(), NetLogWithSource());
  EXPECT_THAT(cb.GetResult(rv), IsOk());

  rv = trans->Read(buffer.get(), kBufferSize, cb.callback());
  EXPECT_EQ(kBufferSize, cb.GetResult(rv));
}

TEST_F(HttpCacheSimpleGetTest, WithDiskFailures) {
  MockHttpCache cache;

  cache.disk_cache()->set_soft_failures_mask(MockDiskEntry::FAIL_ALL);

  // Read from the network, and fail to write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This one should see an empty cache again.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that disk failures after the transaction has started don't cause the
// request to fail.
TEST_F(HttpCacheSimpleGetTest, WithDiskFailures2) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = c->callback.WaitForResult();

  // Start failing request now.
  cache.disk_cache()->set_soft_failures_mask(MockDiskEntry::FAIL_ALL);

  // We have to open the entry again to propagate the failure flag.
  disk_cache::Entry* en;
  ASSERT_TRUE(cache.OpenBackendEntry(request.CacheKey(), &en));
  en->Close();

  ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  c.reset();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This one should see an empty cache again.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we handle failures to read from the cache.
TEST_F(HttpCacheSimpleGetTest, WithDiskFailures3) {
  MockHttpCache cache;

  // Read from the network, and write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  cache.disk_cache()->set_soft_failures_mask(MockDiskEntry::FAIL_ALL);

  MockHttpRequest request(kSimpleGET_Transaction);

  // Now fail to read from the cache.
  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_THAT(c->callback.GetResult(rv), IsOk());

  // Now verify that the entry was removed from the cache.
  cache.disk_cache()->set_soft_failures_mask(0);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheSimpleGetTest, LoadOnlyFromCacheHit) {
  MockHttpCache cache;

  RecordingNetLogObserver net_log_observer;
  NetLogWithSource net_log_with_source =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  LoadTimingInfo load_timing_info;

  // Write to the cache.
  RunTransactionTestAndGetTiming(cache.http_cache(), kSimpleGET_Transaction,
                                 net_log_with_source, &load_timing_info);

  // Check that the NetLog was filled as expected.
  auto entries = GetFilteredNetLogEntries(net_log_observer);

  EXPECT_EQ(6u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(entries, 0,
                                    NetLogEventType::HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(
      LogContainsEndEvent(entries, 1, NetLogEventType::HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 2, NetLogEventType::HTTP_CACHE_OPEN_OR_CREATE_ENTRY));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 3, NetLogEventType::HTTP_CACHE_OPEN_OR_CREATE_ENTRY));
  EXPECT_TRUE(LogContainsBeginEvent(entries, 4,
                                    NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY));
  EXPECT_TRUE(LogContainsEndEvent(entries, 5,
                                  NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY));

  TestLoadTimingNetworkRequest(load_timing_info);

  // Force this transaction to read from the cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;

  net_log_observer.Clear();

  RunTransactionTestAndGetTiming(cache.http_cache(), transaction,
                                 net_log_with_source, &load_timing_info);

  // Check that the NetLog was filled as expected.
  entries = GetFilteredNetLogEntries(net_log_observer);

  EXPECT_EQ(8u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(entries, 0,
                                    NetLogEventType::HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(
      LogContainsEndEvent(entries, 1, NetLogEventType::HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 2, NetLogEventType::HTTP_CACHE_OPEN_OR_CREATE_ENTRY));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 3, NetLogEventType::HTTP_CACHE_OPEN_OR_CREATE_ENTRY));
  EXPECT_TRUE(LogContainsBeginEvent(entries, 4,
                                    NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY));
  EXPECT_TRUE(LogContainsEndEvent(entries, 5,
                                  NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY));
  EXPECT_TRUE(
      LogContainsBeginEvent(entries, 6, NetLogEventType::HTTP_CACHE_READ_INFO));
  EXPECT_TRUE(
      LogContainsEndEvent(entries, 7, NetLogEventType::HTTP_CACHE_READ_INFO));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingCachedResponse(load_timing_info);
}

TEST_F(HttpCacheSimpleGetTest, LoadOnlyFromCacheMiss) {
  MockHttpCache cache;

  // force this transaction to read from the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

  int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
  }
  ASSERT_THAT(rv, IsError(ERR_CACHE_MISS));

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheSimpleGetTest, LoadPreferringCacheHit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_SKIP_CACHE_VALIDATION;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheSimpleGetTest, LoadPreferringCacheMiss) {
  MockHttpCache cache;

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_SKIP_CACHE_VALIDATION;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests LOAD_SKIP_CACHE_VALIDATION in the presence of vary headers.
TEST_F(HttpCacheSimpleGetTest, LoadPreferringCacheVaryMatch) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Cache-Control: max-age=10000\n"
      "Vary: Foo\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Read from the cache.
  transaction.load_flags |= LOAD_SKIP_CACHE_VALIDATION;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests LOAD_SKIP_CACHE_VALIDATION in the presence of vary headers.
TEST_F(HttpCacheSimpleGetTest, LoadPreferringCacheVaryMismatch) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Cache-Control: max-age=10000\n"
      "Vary: Foo\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Attempt to read from the cache... this is a vary mismatch that must reach
  // the network again.
  transaction.load_flags |= LOAD_SKIP_CACHE_VALIDATION;
  transaction.request_headers = "Foo: none\r\n";
  LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests that we honor Vary: * with LOAD_SKIP_CACHE_VALIDATION (crbug/778681)
TEST_F(HttpCacheSimpleGetTest, LoadSkipCacheValidationVaryStar) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Cache-Control: max-age=10000\n"
      "Vary: *\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Attempt to read from the cache... we will still load it from network,
  // since Vary: * doesn't match.
  transaction.load_flags |= LOAD_SKIP_CACHE_VALIDATION;
  LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that was_cached was set properly on a failure, even if the cached
// response wasn't returned.
TEST_F(HttpCacheSimpleGetTest, CacheSignalFailure) {
  for (bool use_memory_entry_data : {false, true}) {
    MockHttpCache cache;
    cache.disk_cache()->set_support_in_memory_entry_data(use_memory_entry_data);

    // Prime cache.
    ScopedMockTransaction transaction(kSimpleGET_Transaction);
    transaction.response_headers = "Cache-Control: no-cache\n";

    RunTransactionTest(cache.http_cache(), transaction);
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());

    // Network failure with error; should fail but have was_cached set.
    transaction.start_return_code = ERR_FAILED;

    MockHttpRequest request(transaction);
    TestCompletionCallback callback;
    std::unique_ptr<HttpTransaction> trans;
    int rv = cache.http_cache()->CreateTransaction(DEFAULT_PRIORITY, &trans);
    EXPECT_THAT(rv, IsOk());
    ASSERT_TRUE(trans.get());
    rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsError(ERR_FAILED));

    const HttpResponseInfo* response_info = trans->GetResponseInfo();
    ASSERT_TRUE(response_info);
    // If use_memory_entry_data is true, we will not bother opening the entry,
    // and just kick it out, so was_cached will end up false.
    EXPECT_EQ(2, cache.network_layer()->transaction_count());
    if (use_memory_entry_data) {
      EXPECT_EQ(false, response_info->was_cached);
      EXPECT_EQ(2, cache.disk_cache()->create_count());
      EXPECT_EQ(0, cache.disk_cache()->open_count());
    } else {
      EXPECT_EQ(true, response_info->was_cached);
      EXPECT_EQ(1, cache.disk_cache()->create_count());
      EXPECT_EQ(1, cache.disk_cache()->open_count());
    }
  }
}

// Tests that if the transaction is destroyed right after setting the
// cache_entry_status_ as CANT_CONDITIONALIZE, then RecordHistograms should not
// hit a dcheck.
TEST_F(HttpCacheTest, RecordHistogramsCantConditionalize) {
  MockHttpCache cache;
  cache.disk_cache()->set_support_in_memory_entry_data(true);

  {
    // Prime cache.
    ScopedMockTransaction transaction(kSimpleGET_Transaction);
    transaction.response_headers = "Cache-Control: no-cache\n";
    RunTransactionTest(cache.http_cache(), transaction);
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
  }

  {
    ScopedMockTransaction transaction(kSimpleGET_Transaction);
    MockHttpRequest request(transaction);
    TestCompletionCallback callback;
    std::unique_ptr<HttpTransaction> trans;
    int rv = cache.http_cache()->CreateTransaction(DEFAULT_PRIORITY, &trans);
    EXPECT_THAT(rv, IsOk());
    ASSERT_TRUE(trans.get());
    rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    // Now destroy the transaction so that RecordHistograms gets invoked.
    trans.reset();
  }
}

// Confirm if we have an empty cache, a read is marked as network verified.
TEST_F(HttpCacheSimpleGetTest, NetworkAccessedNetwork) {
  MockHttpCache cache;

  // write to the cache
  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), kSimpleGET_Transaction,
                                     &response_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  EXPECT_TRUE(response_info.network_accessed);
  EXPECT_EQ(CacheEntryStatus::ENTRY_NOT_IN_CACHE,
            response_info.cache_entry_status);
}

// Confirm if we have a fresh entry in cache, it isn't marked as
// network verified.
TEST_F(HttpCacheSimpleGetTest, NetworkAccessedCache) {
  MockHttpCache cache;

  // Prime cache.
  MockTransaction transaction(kSimpleGET_Transaction);

  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Re-run transaction; make sure we don't mark the network as accessed.
  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_FALSE(response_info.network_accessed);
  EXPECT_EQ(CacheEntryStatus::ENTRY_USED, response_info.cache_entry_status);
}

TEST_F(HttpCacheSimpleGetTest, LoadBypassCache) {
  MockHttpCache cache;

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // Force this transaction to write to the cache again.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_BYPASS_CACHE;

  RecordingNetLogObserver net_log_observer;
  LoadTimingInfo load_timing_info;

  // Write to the cache.
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  // Check that the NetLog was filled as expected.
  auto entries = GetFilteredNetLogEntries(net_log_observer);

  EXPECT_EQ(8u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(entries, 0,
                                    NetLogEventType::HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(
      LogContainsEndEvent(entries, 1, NetLogEventType::HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(LogContainsBeginEvent(entries, 2,
                                    NetLogEventType::HTTP_CACHE_DOOM_ENTRY));
  EXPECT_TRUE(
      LogContainsEndEvent(entries, 3, NetLogEventType::HTTP_CACHE_DOOM_ENTRY));
  EXPECT_TRUE(LogContainsBeginEvent(entries, 4,
                                    NetLogEventType::HTTP_CACHE_CREATE_ENTRY));
  EXPECT_TRUE(LogContainsEndEvent(entries, 5,
                                  NetLogEventType::HTTP_CACHE_CREATE_ENTRY));
  EXPECT_TRUE(LogContainsBeginEvent(entries, 6,
                                    NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY));
  EXPECT_TRUE(LogContainsEndEvent(entries, 7,
                                  NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY));

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

TEST_F(HttpCacheSimpleGetTest, LoadBypassCacheImplicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "pragma: no-cache\r\n";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheSimpleGetTest, LoadBypassCacheImplicit2) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: no-cache\r\n";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheSimpleGetTest, LoadValidateCache) {
  MockHttpCache cache;

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // Read from the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // Force this transaction to validate the cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_VALIDATE_CACHE;

  HttpResponseInfo response_info;
  LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), &load_timing_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  EXPECT_TRUE(response_info.network_accessed);
  TestLoadTimingNetworkRequest(load_timing_info);
}

TEST_F(HttpCacheSimpleGetTest, LoadValidateCacheImplicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: max-age=0\r\n";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that |unused_since_prefetch| is updated accordingly (e.g. it is set to
// true after a prefetch and set back to false when the prefetch is used).
TEST_F(HttpCacheSimpleGetTest, UnusedSincePrefetch) {
  MockHttpCache cache;
  HttpResponseInfo response_info;

  // A normal load does not have |unused_since_prefetch| set.
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), kSimpleGET_Transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_FALSE(response_info.unused_since_prefetch);
  EXPECT_FALSE(response_info.was_cached);

  // The prefetch itself does not have |unused_since_prefetch| set.
  MockTransaction prefetch_transaction(kSimpleGET_Transaction);
  prefetch_transaction.load_flags |= LOAD_PREFETCH;
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), prefetch_transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_FALSE(response_info.unused_since_prefetch);
  EXPECT_TRUE(response_info.was_cached);

  // A duplicated prefetch has |unused_since_prefetch| set.
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), prefetch_transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_TRUE(response_info.unused_since_prefetch);
  EXPECT_TRUE(response_info.was_cached);

  // |unused_since_prefetch| is still true after two prefetches in a row.
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), kSimpleGET_Transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_TRUE(response_info.unused_since_prefetch);
  EXPECT_TRUE(response_info.was_cached);

  // The resource has now been used, back to normal behavior.
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), kSimpleGET_Transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_FALSE(response_info.unused_since_prefetch);
  EXPECT_TRUE(response_info.was_cached);
}

// Tests that requests made with the LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME
// load flag result in HttpResponseInfo entries with the |restricted_prefetch|
// flag set. Also tests that responses with |restricted_prefetch| flag set can
// only be used by requests that have the
// LOAD_CAN_USE_RESTRICTED_PREFETCH_FOR_MAIN_FRAME load flag.
TEST_F(HttpCacheSimpleGetTest, RestrictedPrefetchIsRestrictedUntilReuse) {
  MockHttpCache cache;
  HttpResponseInfo response_info;

  // A normal load does not have |restricted_prefetch| set.
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), kTypicalGET_Transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_FALSE(response_info.restricted_prefetch);
  EXPECT_FALSE(response_info.was_cached);
  EXPECT_TRUE(response_info.network_accessed);

  // A restricted prefetch is marked as |restricted_prefetch|.
  MockTransaction prefetch_transaction(kSimpleGET_Transaction);
  prefetch_transaction.load_flags |= LOAD_PREFETCH;
  prefetch_transaction.load_flags |= LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), prefetch_transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_TRUE(response_info.restricted_prefetch);
  EXPECT_FALSE(response_info.was_cached);
  EXPECT_TRUE(response_info.network_accessed);

  // Requests that are marked as able to reuse restricted prefetches can do so
  // correctly. Once it is reused, it is no longer considered as or marked
  // restricted.
  MockTransaction can_use_restricted_prefetch_transaction(
      kSimpleGET_Transaction);
  can_use_restricted_prefetch_transaction.load_flags |=
      LOAD_CAN_USE_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), can_use_restricted_prefetch_transaction,
      &response_info, NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_TRUE(response_info.restricted_prefetch);
  EXPECT_TRUE(response_info.was_cached);
  EXPECT_FALSE(response_info.network_accessed);

  // Later reuse is still no longer marked restricted.
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), kSimpleGET_Transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_FALSE(response_info.restricted_prefetch);
  EXPECT_TRUE(response_info.was_cached);
  EXPECT_FALSE(response_info.network_accessed);
}

TEST_F(HttpCacheSimpleGetTest, RestrictedPrefetchReuseIsLimited) {
  MockHttpCache cache;
  HttpResponseInfo response_info;

  // A restricted prefetch is marked as |restricted_prefetch|.
  MockTransaction prefetch_transaction(kSimpleGET_Transaction);
  prefetch_transaction.load_flags |= LOAD_PREFETCH;
  prefetch_transaction.load_flags |= LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), prefetch_transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_TRUE(response_info.restricted_prefetch);
  EXPECT_FALSE(response_info.was_cached);
  EXPECT_TRUE(response_info.network_accessed);

  // Requests that cannot reuse restricted prefetches fail to do so. The network
  // is accessed and the resulting response is not marked as
  // |restricted_prefetch|.
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), kSimpleGET_Transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_FALSE(response_info.restricted_prefetch);
  EXPECT_FALSE(response_info.was_cached);
  EXPECT_TRUE(response_info.network_accessed);

  // Future requests that are not marked as able to reuse restricted prefetches
  // can use the entry in the cache now, since it has been evicted in favor of
  // an unrestricted one.
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), kSimpleGET_Transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_FALSE(response_info.restricted_prefetch);
  EXPECT_TRUE(response_info.was_cached);
  EXPECT_FALSE(response_info.network_accessed);
}

TEST_F(HttpCacheSimpleGetTest, UnusedSincePrefetchWriteError) {
  MockHttpCache cache;
  HttpResponseInfo response_info;

  // Do a prefetch.
  MockTransaction prefetch_transaction(kSimpleGET_Transaction);
  prefetch_transaction.load_flags |= LOAD_PREFETCH;
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), prefetch_transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
  EXPECT_TRUE(response_info.unused_since_prefetch);
  EXPECT_FALSE(response_info.was_cached);

  // Try to use it while injecting a failure on write.
  cache.disk_cache()->set_soft_failures_mask(MockDiskEntry::FAIL_WRITE);
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), kSimpleGET_Transaction, &response_info,
      NetLogWithSource::Make(NetLogSourceType::NONE), nullptr);
}

// Make sure that if a prefetch entry is truncated, then an attempt to re-use it
// gets aborted in connected handler that truncated bit is not lost.
TEST_F(HttpCacheTest, PrefetchTruncateCancelInConnectedCallback) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 20\n"
      "Etag: \"foopy\"\n";
  transaction.data = "01234567890123456789";
  transaction.load_flags |=
      LOAD_PREFETCH | LOAD_CAN_USE_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;

  // Do a truncated read of a prefetch request.
  {
    MockHttpRequest request(transaction);
    Context c;

    int rv = cache.CreateTransaction(&c.trans);
    ASSERT_THAT(rv, IsOk());

    rv = c.callback.GetResult(
        c.trans->Start(&request, c.callback.callback(), NetLogWithSource()));
    ASSERT_THAT(rv, IsOk());

    // Read less than the whole thing.
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(10);
    rv = c.callback.GetResult(
        c.trans->Read(buf.get(), buf->size(), c.callback.callback()));
    EXPECT_EQ(buf->size(), rv);

    // Destroy the transaction.
    c.trans.reset();
    base::RunLoop().RunUntilIdle();

    VerifyTruncatedFlag(&cache, request.CacheKey(), /*flag_value=*/true,
                        /*data_size=*/10);
  }

  // Do a fetch that can use prefetch that aborts in connected handler.
  transaction.load_flags &= ~LOAD_PREFETCH;
  {
    MockHttpRequest request(transaction);
    Context c;

    int rv = cache.CreateTransaction(&c.trans);
    ASSERT_THAT(rv, IsOk());
    c.trans->SetConnectedCallback(base::BindRepeating(
        [](const TransportInfo& info, CompletionOnceCallback callback) -> int {
          return ERR_ABORTED;
        }));
    rv = c.callback.GetResult(
        c.trans->Start(&request, c.callback.callback(), NetLogWithSource()));
    EXPECT_EQ(ERR_ABORTED, rv);

    // Destroy the transaction.
    c.trans.reset();
    base::RunLoop().RunUntilIdle();

    VerifyTruncatedFlag(&cache, request.CacheKey(), /*flag_value=*/true,
                        /*data_size=*/10);
  }

  // Now try again without abort.
  {
    MockHttpRequest request(transaction);
    RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                  /*response_info=*/nullptr);
    base::RunLoop().RunUntilIdle();

    VerifyTruncatedFlag(&cache, request.CacheKey(), /*flag_value=*/false,
                        /*data_size=*/20);
  }
}

// Make sure that if a stale-while-revalidate entry is truncated, then an
// attempt to re-use it gets aborted in connected handler that truncated bit is
// not lost.
TEST_F(HttpCacheTest, StaleWhiteRevalidateTruncateCancelInConnectedCallback) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 20\n"
      "Cache-Control: max-age=0, stale-while-revalidate=60\n"
      "Etag: \"foopy\"\n";
  transaction.data = "01234567890123456789";
  transaction.load_flags |= LOAD_SUPPORT_ASYNC_REVALIDATION;

  // Do a truncated read of a stale-while-revalidate resource.
  {
    MockHttpRequest request(transaction);
    Context c;

    int rv = cache.CreateTransaction(&c.trans);
    ASSERT_THAT(rv, IsOk());

    rv = c.callback.GetResult(
        c.trans->Start(&request, c.callback.callback(), NetLogWithSource()));
    ASSERT_THAT(rv, IsOk());

    // Read less than the whole thing.
    scoped_refptr<IOBufferWithSize> buf =
        base::MakeRefCounted<IOBufferWithSize>(10);
    rv = c.callback.GetResult(
        c.trans->Read(buf.get(), buf->size(), c.callback.callback()));
    EXPECT_EQ(buf->size(), rv);

    // Destroy the transaction.
    c.trans.reset();
    base::RunLoop().RunUntilIdle();

    VerifyTruncatedFlag(&cache, request.CacheKey(), /*flag_value=*/true,
                        /*data_size=*/10);
  }

  // Do a fetch that uses that resource that aborts in connected handler.
  {
    MockHttpRequest request(transaction);
    Context c;

    int rv = cache.CreateTransaction(&c.trans);
    ASSERT_THAT(rv, IsOk());
    c.trans->SetConnectedCallback(base::BindRepeating(
        [](const TransportInfo& info, CompletionOnceCallback callback) -> int {
          return ERR_ABORTED;
        }));
    rv = c.callback.GetResult(
        c.trans->Start(&request, c.callback.callback(), NetLogWithSource()));
    EXPECT_EQ(ERR_ABORTED, rv);

    // Destroy the transaction.
    c.trans.reset();
    base::RunLoop().RunUntilIdle();

    VerifyTruncatedFlag(&cache, request.CacheKey(), /*flag_value=*/true,
                        /*data_size=*/10);
  }

  // Now try again without abort.
  {
    MockHttpRequest request(transaction);
    RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                  /*response_info=*/nullptr);
    base::RunLoop().RunUntilIdle();

    VerifyTruncatedFlag(&cache, request.CacheKey(), /*flag_value=*/false,
                        /*data_size=*/20);
  }
}

static const auto kPreserveRequestHeaders =
    base::BindRepeating([](const HttpRequestInfo* request,
                           std::string* response_status,
                           std::string* response_headers,
                           std::string* response_data) {
      EXPECT_TRUE(request->extra_headers.HasHeader(kExtraHeaderKey));
    });

// Tests that we don't remove extra headers for simple requests.
TEST_F(HttpCacheSimpleGetTest, PreserveRequestHeaders) {
  for (bool use_memory_entry_data : {false, true}) {
    MockHttpCache cache;
    cache.disk_cache()->set_support_in_memory_entry_data(use_memory_entry_data);

    ScopedMockTransaction transaction(kSimpleGET_Transaction);
    transaction.handler = kPreserveRequestHeaders;
    transaction.request_headers = EXTRA_HEADER;
    transaction.response_headers = "Cache-Control: max-age=0\n";

    // Write, then revalidate the entry.
    RunTransactionTest(cache.http_cache(), transaction);
    RunTransactionTest(cache.http_cache(), transaction);

    EXPECT_EQ(2, cache.network_layer()->transaction_count());

    // If the backend supports memory entry data, we can figure out that the
    // entry has caching-hostile headers w/o opening it.
    if (use_memory_entry_data) {
      EXPECT_EQ(0, cache.disk_cache()->open_count());
      EXPECT_EQ(2, cache.disk_cache()->create_count());
    } else {
      EXPECT_EQ(1, cache.disk_cache()->open_count());
      EXPECT_EQ(1, cache.disk_cache()->create_count());
    }
  }
}

// Tests that we don't remove extra headers for conditionalized requests.
TEST_F(HttpCacheTest, ConditionalizedGetPreserveRequestHeaders) {
  for (bool use_memory_entry_data : {false, true}) {
    MockHttpCache cache;
    // Unlike in SimpleGET_PreserveRequestHeaders, this entry can be
    // conditionalized, so memory hints don't affect behavior.
    cache.disk_cache()->set_support_in_memory_entry_data(use_memory_entry_data);

    // Write to the cache.
    RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

    ScopedMockTransaction transaction(kETagGET_Transaction);
    transaction.handler = kPreserveRequestHeaders;
    transaction.request_headers = "If-None-Match: \"foopy\"\r\n" EXTRA_HEADER;

    RunTransactionTest(cache.http_cache(), transaction);

    EXPECT_EQ(2, cache.network_layer()->transaction_count());
    EXPECT_EQ(1, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }
}

TEST_F(HttpCacheSimpleGetTest, ManyReaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // All requests are waiting for the active entry.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, context->trans->GetLoadState());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  // All requests are added to writers.
  std::string cache_key = request.CacheKey();
  EXPECT_EQ(kNumTransactions, cache.GetCountWriterTransactions(cache_key));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // All requests are between Start and Read, i.e. idle.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_IDLE, context->trans->GetLoadState());
  }

  for (int i = 0; i < kNumTransactions; ++i) {
    auto& c = context_list[i];
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }

    // After the 1st transaction has completed the response, all transactions
    // get added to readers.
    if (i > 0) {
      EXPECT_FALSE(cache.IsWriterPresent(cache_key));
      EXPECT_EQ(kNumTransactions - i, cache.GetCountReaders(cache_key));
    }

    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // We should not have had to re-open the disk entry
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

using HttpCacheRangeGetTest = HttpCacheTest;

TEST_F(HttpCacheRangeGetTest, FullAfterPartial) {
  MockHttpCache cache;

  // Request a prefix.
  {
    ScopedMockTransaction transaction_pre(kRangeGET_TransactionOK);
    transaction_pre.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
    transaction_pre.data = "rg: 00-09 ";
    MockHttpRequest request_pre(transaction_pre);

    HttpResponseInfo response_pre;
    RunTransactionTestWithRequest(cache.http_cache(), transaction_pre,
                                  request_pre, &response_pre);
    ASSERT_TRUE(response_pre.headers != nullptr);
    EXPECT_EQ(206, response_pre.headers->response_code());
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }

  {
    // Now request the full thing, but set validation to fail. This would
    // previously fail in the middle of data and truncate it; current behavior
    // restarts it, somewhat wastefully but gets the data back.
    RangeTransactionServer handler;
    handler.set_modified(true);

    ScopedMockTransaction transaction_all(kRangeGET_TransactionOK);
    transaction_all.request_headers = EXTRA_HEADER;
    transaction_all.data = "Not a range";
    MockHttpRequest request_all(transaction_all);

    HttpResponseInfo response_all;
    RunTransactionTestWithRequest(cache.http_cache(), transaction_all,
                                  request_all, &response_all);
    ASSERT_TRUE(response_all.headers != nullptr);
    EXPECT_EQ(200, response_all.headers->response_code());
    // 1 from previous test, failed validation, and re-try.
    EXPECT_EQ(3, cache.network_layer()->transaction_count());
    EXPECT_EQ(1, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }
}

// Tests that when a range request transaction becomes a writer for the first
// range and then fails conditionalization for the next range and decides to
// doom the entry, then there should not be a dcheck assertion hit.
TEST_F(HttpCacheRangeGetTest, OverlappingRangesCouldntConditionalize) {
  MockHttpCache cache;

  {
    ScopedMockTransaction transaction_pre(kRangeGET_TransactionOK);
    transaction_pre.request_headers = "Range: bytes = 10-19\r\n" EXTRA_HEADER;
    transaction_pre.data = "rg: 10-19 ";
    MockHttpRequest request_pre(transaction_pre);

    HttpResponseInfo response_pre;
    RunTransactionTestWithRequest(cache.http_cache(), transaction_pre,
                                  request_pre, &response_pre);
    ASSERT_TRUE(response_pre.headers != nullptr);
    EXPECT_EQ(206, response_pre.headers->response_code());
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }

  {
    // First range skips validation because the response is fresh while the
    // second range requires validation since that range is not present in the
    // cache and during validation it fails conditionalization.
    cache.FailConditionalizations();
    ScopedMockTransaction transaction_pre(kRangeGET_TransactionOK);
    transaction_pre.request_headers = "Range: bytes = 10-29\r\n" EXTRA_HEADER;

    // TODO(crbug.com/40639784): Fix this scenario to not return the cached
    // bytes repeatedly.
    transaction_pre.data = "rg: 10-19 rg: 10-19 rg: 20-29 ";
    MockHttpRequest request_pre(transaction_pre);
    HttpResponseInfo response_pre;
    RunTransactionTestWithRequest(cache.http_cache(), transaction_pre,
                                  request_pre, &response_pre);
    ASSERT_TRUE(response_pre.headers != nullptr);
    EXPECT_EQ(2, cache.network_layer()->transaction_count());
    EXPECT_EQ(1, cache.disk_cache()->open_count());
    EXPECT_EQ(2, cache.disk_cache()->create_count());
  }
}

TEST_F(HttpCacheRangeGetTest, FullAfterPartialReuse) {
  MockHttpCache cache;

  // Request a prefix.
  {
    ScopedMockTransaction transaction_pre(kRangeGET_TransactionOK);
    transaction_pre.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
    transaction_pre.data = "rg: 00-09 ";
    MockHttpRequest request_pre(transaction_pre);

    HttpResponseInfo response_pre;
    RunTransactionTestWithRequest(cache.http_cache(), transaction_pre,
                                  request_pre, &response_pre);
    ASSERT_TRUE(response_pre.headers != nullptr);
    EXPECT_EQ(206, response_pre.headers->response_code());
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }

  {
    // Now request the full thing, revalidating successfully, so the full
    // file gets stored via a sparse-entry.
    ScopedMockTransaction transaction_all(kRangeGET_TransactionOK);
    transaction_all.request_headers = EXTRA_HEADER;
    transaction_all.data =
        "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49"
        " rg: 50-59 rg: 60-69 rg: 70-79 ";
    MockHttpRequest request_all(transaction_all);

    HttpResponseInfo response_all;
    RunTransactionTestWithRequest(cache.http_cache(), transaction_all,
                                  request_all, &response_all);
    ASSERT_TRUE(response_all.headers != nullptr);
    EXPECT_EQ(200, response_all.headers->response_code());
    // 1 from previous test, validation, and second chunk
    EXPECT_EQ(3, cache.network_layer()->transaction_count());
    EXPECT_EQ(1, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }

  {
    // Grab it again, should not need re-validation.
    ScopedMockTransaction transaction_all2(kRangeGET_TransactionOK);
    transaction_all2.request_headers = EXTRA_HEADER;
    transaction_all2.data =
        "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49"
        " rg: 50-59 rg: 60-69 rg: 70-79 ";
    MockHttpRequest request_all2(transaction_all2);

    HttpResponseInfo response_all2;
    RunTransactionTestWithRequest(cache.http_cache(), transaction_all2,
                                  request_all2, &response_all2);
    ASSERT_TRUE(response_all2.headers != nullptr);
    EXPECT_EQ(200, response_all2.headers->response_code());

    // Only one more cache open, no new network traffic.
    EXPECT_EQ(3, cache.network_layer()->transaction_count());
    EXPECT_EQ(2, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }
}

// This test verifies that the ConnectedCallback passed to a cache transaction
// is called once per subrange in the case of a range request with a partial
// cache hit.
TEST_F(HttpCacheRangeGetTest, ConnectedCallbackCalledForEachRange) {
  MockHttpCache cache;

  // Request an infix range and populate the cache with it.
  {
    ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
    mock_transaction.request_headers = "Range: bytes = 20-29\r\n" EXTRA_HEADER;
    mock_transaction.data = "rg: 20-29 ";
    mock_transaction.transport_info = TestTransportInfo();

    RunTransactionTest(cache.http_cache(), mock_transaction);
  }

  // Request a surrounding range and observe that the callback is called once
  // per subrange, as split up by cache hits.
  {
    ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
    mock_transaction.request_headers = "Range: bytes = 10-39\r\n" EXTRA_HEADER;
    mock_transaction.data = "rg: 10-19 rg: 20-29 rg: 30-39 ";
    mock_transaction.transport_info = TestTransportInfo();
    MockHttpRequest request(mock_transaction);

    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    // 1 call for the first range's network transaction.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo()));

    // Switch the endpoint for the next network transaction to observe.
    // For ease, we just switch the port number.
    //
    // NOTE: This works because only the mock transaction struct's address is
    // registered with the mocking framework - the pointee data is consulted
    // each time it is read.
    mock_transaction.transport_info = TestTransportInfoWithPort(123);

    ReadAndVerifyTransaction(transaction.get(), mock_transaction);

    // A second call for the cached range, reported as coming from the original
    // endpoint it was cached from. A third call for the last range's network
    // transaction.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo(), CachedTestTransportInfo(),
                            TestTransportInfoWithPort(123)));
  }
}

// This test verifies that when the ConnectedCallback passed to a cache range
// transaction returns an `ERR_INCONSISTENT_IP_ADDRESS_SPACE` error during a
// partial read from cache, then the cache entry is invalidated.
TEST_F(HttpCacheRangeGetTest, ConnectedCallbackReturnInconsistentIpError) {
  MockHttpCache cache;

  // Request an infix range and populate the cache with it.
  {
    ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
    mock_transaction.request_headers = "Range: bytes = 20-29\r\n" EXTRA_HEADER;
    mock_transaction.data = "rg: 20-29 ";
    mock_transaction.transport_info = TestTransportInfo();

    RunTransactionTest(cache.http_cache(), mock_transaction);
  }

  ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
  mock_transaction.request_headers = "Range: bytes = 10-39\r\n" EXTRA_HEADER;
  mock_transaction.data = "rg: 10-19 rg: 20-29 rg: 30-39 ";
  mock_transaction.transport_info = TestTransportInfo();
  MockHttpRequest request(mock_transaction);

  // Request a surrounding range. This *should* be read in three parts:
  //
  // 1. for the prefix: from the network
  // 2. for the cached infix: from the cache
  // 3. for the suffix: from the network
  //
  // The connected callback returns OK for 1), but fails during 2). As a result,
  // the transaction fails partway and 3) is never created. The cache entry is
  // invalidated as a result of the specific error code.
  {
    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    // 1 call for the first range's network transaction.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo()));

    // Set the callback to return an error the next time it is called.
    connected_handler.set_result(ERR_INCONSISTENT_IP_ADDRESS_SPACE);

    std::string content;
    EXPECT_THAT(ReadTransaction(transaction.get(), &content),
                IsError(ERR_INCONSISTENT_IP_ADDRESS_SPACE));

    // A second call that failed.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo(), CachedTestTransportInfo()));
  }

  // Request the same range again, observe that nothing is read from cache.
  {
    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    std::string content;
    EXPECT_THAT(ReadTransaction(transaction.get(), &content), IsOk());
    EXPECT_EQ(content, mock_transaction.data);

    // 1 call for the network transaction from which the whole response was
    // read. The first 20 bytes were cached by the previous two requests, but
    // the cache entry was doomed during the last transaction so they are not
    // used here.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo()));
  }
}

// This test verifies that when the ConnectedCallback passed to a cache range
// transaction returns an `ERR_INCONSISTENT_IP_ADDRESS_SPACE` error during a
// network transaction, then the cache entry is invalidated.
TEST_F(HttpCacheRangeGetTest,
       ConnectedCallbackReturnInconsistentIpErrorForNetwork) {
  MockHttpCache cache;

  // Request a prefix range and populate the cache with it.
  {
    ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
    mock_transaction.request_headers = "Range: bytes = 10-19\r\n" EXTRA_HEADER;
    mock_transaction.data = "rg: 10-19 ";
    mock_transaction.transport_info = TestTransportInfo();

    RunTransactionTest(cache.http_cache(), mock_transaction);
  }

  ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
  mock_transaction.request_headers = "Range: bytes = 10-29\r\n" EXTRA_HEADER;
  mock_transaction.data = "rg: 10-19 rg: 20-29 ";
  mock_transaction.transport_info = TestTransportInfo();
  MockHttpRequest request(mock_transaction);

  // Request a longer range. This *should* be read in two parts:
  //
  // 1. for the prefix: from the cache
  // 2. for the suffix: from the network
  {
    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    // 1 call for the first range's network transaction.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(CachedTestTransportInfo()));

    // Set the callback to return an error the next time it is called.
    connected_handler.set_result(ERR_INCONSISTENT_IP_ADDRESS_SPACE);

    std::string content;
    EXPECT_THAT(ReadTransaction(transaction.get(), &content),
                IsError(ERR_INCONSISTENT_IP_ADDRESS_SPACE));

    // A second call that failed.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(CachedTestTransportInfo(), TestTransportInfo()));
  }

  // Request the same range again, observe that nothing is read from cache.
  {
    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    std::string content;
    EXPECT_THAT(ReadTransaction(transaction.get(), &content), IsOk());
    EXPECT_EQ(content, mock_transaction.data);

    // 1 call for the network transaction from which the whole response was
    // read. The first 20 bytes were cached by the previous two requests, but
    // the cache entry was doomed during the last transaction so they are not
    // used here.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo()));
  }
}

// This test verifies that when the ConnectedCallback passed to a cache
// transaction returns an error for the second (or third) subrange transaction,
// the overall cache transaction fails with that error. The cache entry is still
// usable after that.
TEST_F(HttpCacheRangeGetTest, ConnectedCallbackReturnErrorSecondTime) {
  MockHttpCache cache;

  // Request an infix range and populate the cache with it.
  {
    ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
    mock_transaction.request_headers = "Range: bytes = 20-29\r\n" EXTRA_HEADER;
    mock_transaction.data = "rg: 20-29 ";
    mock_transaction.transport_info = TestTransportInfo();

    RunTransactionTest(cache.http_cache(), mock_transaction);
  }

  ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
  mock_transaction.request_headers = "Range: bytes = 10-39\r\n" EXTRA_HEADER;
  mock_transaction.data = "rg: 10-19 rg: 20-29 rg: 30-39 ";
  mock_transaction.transport_info = TestTransportInfo();
  MockHttpRequest request(mock_transaction);

  // Request a surrounding range. This *should* be read in three parts:
  //
  // 1. for the prefix: from the network
  // 2. for the cached infix: from the cache
  // 3. for the suffix: from the network
  //
  // The connected callback returns OK for 1), but fails during 2). As a result,
  // the transaction fails partway and 3) is never created. The prefix is still
  // cached, such that the cache entry ends up with both the prefix and infix.
  {
    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    // 1 call for the first range's network transaction.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo()));

    // Set the callback to return an error the next time it is called. The exact
    // error code is irrelevant, what matters is that it is reflected in the
    // overall status of the transaction.
    connected_handler.set_result(ERR_NOT_IMPLEMENTED);

    std::string content;
    EXPECT_THAT(ReadTransaction(transaction.get(), &content),
                IsError(ERR_NOT_IMPLEMENTED));

    // A second call that failed.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(TestTransportInfo(), CachedTestTransportInfo()));
  }

  // Request the same range again, observe that the prefix and infix are both
  // read from cache. Only the suffix is fetched from the network.
  {
    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    // 1 call for the first range's cache transaction: the first 20 bytes were
    // cached by the previous two requests.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(CachedTestTransportInfo()));

    std::string content;
    EXPECT_THAT(ReadTransaction(transaction.get(), &content), IsOk());
    EXPECT_EQ(content, mock_transaction.data);

    // A second call from the network transaction for the last 10 bytes.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(CachedTestTransportInfo(), TestTransportInfo()));
  }
}

// This test verifies that the ConnectedCallback passed to a cache transaction
// is called once per subrange in the case of a range request with a partial
// cache hit, even when a prefix of the range is cached.
TEST_F(HttpCacheRangeGetTest, ConnectedCallbackCalledForEachRangeWithPrefix) {
  MockHttpCache cache;

  // Request a prefix range and populate the cache with it.
  {
    ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
    mock_transaction.request_headers = "Range: bytes = 10-19\r\n" EXTRA_HEADER;
    mock_transaction.data = "rg: 10-19 ";
    mock_transaction.transport_info = TestTransportInfo();

    RunTransactionTest(cache.http_cache(), mock_transaction);
  }

  // Request a surrounding range and observe that the callback is called once
  // per subrange, as split up by cache hits.
  {
    ScopedMockTransaction mock_transaction(kRangeGET_TransactionOK);
    mock_transaction.request_headers = "Range: bytes = 10-39\r\n" EXTRA_HEADER;
    mock_transaction.data = "rg: 10-19 rg: 20-29 rg: 30-39 ";
    mock_transaction.transport_info = TestTransportInfoWithPort(123);
    MockHttpRequest request(mock_transaction);

    ConnectedHandler connected_handler;

    std::unique_ptr<HttpTransaction> transaction;
    EXPECT_THAT(cache.CreateTransaction(&transaction), IsOk());
    ASSERT_THAT(transaction, NotNull());

    transaction->SetConnectedCallback(connected_handler.Callback());

    TestCompletionCallback callback;
    ASSERT_THAT(
        transaction->Start(&request, callback.callback(), NetLogWithSource()),
        IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());

    // 1 call for the first range from the cache, reported as coming from the
    // endpoint which initially served the cached range.
    EXPECT_THAT(connected_handler.transports(),
                ElementsAre(CachedTestTransportInfo()));

    ReadAndVerifyTransaction(transaction.get(), mock_transaction);

    // A second call for the last range's network transaction.
    EXPECT_THAT(
        connected_handler.transports(),
        ElementsAre(CachedTestTransportInfo(), TestTransportInfoWithPort(123)));
  }
}

// Tests that a range transaction is still usable even if it's unable to access
// the cache.
TEST_F(HttpCacheRangeGetTest, FailedCacheAccess) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  MockHttpRequest request(transaction);

  auto c = std::make_unique<Context>();
  c->result = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(c->result, IsOk());
  EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

  cache.disk_cache()->set_fail_requests(true);

  c->result =
      c->trans->Start(&request, c->callback.callback(), NetLogWithSource());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cache.IsWriterPresent(kRangeGET_TransactionOK.url));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  c->result = c->callback.WaitForResult();

  ReadAndVerifyTransaction(c->trans.get(), kRangeGET_TransactionOK);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that we can have parallel validation on range requests.
TEST_F(HttpCacheRangeGetTest, ParallelValidationNoMatch) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  MockHttpRequest request(transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // All requests are waiting for the active entry.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, context->trans->GetLoadState());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  // First entry created is doomed due to 2nd transaction's validation leading
  // to restarting of the queued transactions.
  EXPECT_TRUE(cache.IsWriterPresent(request.CacheKey()));

  // TODO(shivanisha): The restarted transactions race for creating the entry
  // and thus instead of all 4 succeeding, 2 of them succeed. This is very
  // implementation specific and happens because the queued transactions get
  // restarted synchronously and get to the queue of creating the entry before
  // the transaction that is restarting them. Fix the test to make it less
  // vulnerable to any scheduling changes in the code.
  EXPECT_EQ(5, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());

  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_IDLE, context->trans->GetLoadState());
  }

  for (int i = 0; i < kNumTransactions; ++i) {
    auto& c = context_list[i];
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }

    ReadAndVerifyTransaction(c->trans.get(), kRangeGET_TransactionOK);
  }

  EXPECT_EQ(5, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());
}

// Tests that if a transaction is dooming the entry and the entry was doomed by
// another transaction that was not part of the entry and created a new entry,
// the new entry should not be incorrectly doomed. (crbug.com/736993)
TEST_F(HttpCacheRangeGetTest, ParallelValidationNoMatchDoomEntry) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  MockHttpRequest request(transaction);

  MockTransaction dooming_transaction(kRangeGET_TransactionOK);
  dooming_transaction.load_flags |= LOAD_BYPASS_CACHE;
  MockHttpRequest dooming_request(dooming_transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 3;

  scoped_refptr<MockDiskEntry> first_entry;
  scoped_refptr<MockDiskEntry> second_entry;
  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    MockHttpRequest* this_request = &request;

    if (i == 2) {
      this_request = &dooming_request;
    }

    if (i == 1) {
      ASSERT_TRUE(first_entry);
      first_entry->SetDefer(MockDiskEntry::DEFER_READ);
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());

    // Continue the transactions. 2nd will pause at the cache reading state and
    // 3rd transaction will doom the entry.
    base::RunLoop().RunUntilIdle();

    std::string cache_key = request.CacheKey();
    // Check status of the first and second entries after every transaction.
    switch (i) {
      case 0:
        first_entry = cache.disk_cache()->GetDiskEntryRef(cache_key);
        break;
      case 1:
        EXPECT_FALSE(first_entry->is_doomed());
        break;
      case 2:
        EXPECT_TRUE(first_entry->is_doomed());
        second_entry = cache.disk_cache()->GetDiskEntryRef(cache_key);
        EXPECT_FALSE(second_entry->is_doomed());
        break;
    }
  }
  // Resume cache read by 1st transaction which will lead to dooming the entry
  // as well since the entry cannot be validated. This double dooming should not
  // lead to an assertion.
  first_entry->ResumeDiskEntryOperation();
  base::RunLoop().RunUntilIdle();

  // Since second_entry is already created, when 1st transaction goes on to
  // create an entry, it will get ERR_CACHE_RACE leading to dooming of
  // second_entry and creation of a third entry.
  EXPECT_TRUE(second_entry->is_doomed());

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());

  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_IDLE, context->trans->GetLoadState());
  }

  for (auto& c : context_list) {
    ReadAndVerifyTransaction(c->trans.get(), kRangeGET_TransactionOK);
  }

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());
}

// Same as above but tests that the 2nd transaction does not do anything if
// there is nothing to doom. (crbug.com/736993)
TEST_F(HttpCacheRangeGetTest, ParallelValidationNoMatchDoomEntry1) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  MockHttpRequest request(transaction);

  MockTransaction dooming_transaction(kRangeGET_TransactionOK);
  dooming_transaction.load_flags |= LOAD_BYPASS_CACHE;
  MockHttpRequest dooming_request(dooming_transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 3;

  scoped_refptr<MockDiskEntry> first_entry;
  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    MockHttpRequest* this_request = &request;

    if (i == 2) {
      this_request = &dooming_request;
      cache.disk_cache()->SetDefer(MockDiskEntry::DEFER_CREATE);
    }

    if (i == 1) {
      ASSERT_TRUE(first_entry);
      first_entry->SetDefer(MockDiskEntry::DEFER_READ);
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());

    // Continue the transactions. 2nd will pause at the cache reading state and
    // 3rd transaction will doom the entry and pause before creating a new
    // entry.
    base::RunLoop().RunUntilIdle();

    // Check status of the entry after every transaction.
    switch (i) {
      case 0:
        first_entry = cache.disk_cache()->GetDiskEntryRef(request.CacheKey());
        break;
      case 1:
        EXPECT_FALSE(first_entry->is_doomed());
        break;
      case 2:
        EXPECT_TRUE(first_entry->is_doomed());
        break;
    }
  }
  // Resume cache read by 2nd transaction which will lead to dooming the entry
  // as well since the entry cannot be validated. This double dooming should not
  // lead to an assertion.
  first_entry->ResumeDiskEntryOperation();
  base::RunLoop().RunUntilIdle();

  // Resume creation of entry by 3rd transaction.
  cache.disk_cache()->ResumeCacheOperation();
  base::RunLoop().RunUntilIdle();

  // Note that since 3rd transaction's entry is already created but its
  // callback is deferred, MockDiskCache's implementation returns
  // ERR_CACHE_CREATE_FAILURE when 2nd transaction tries to create an entry
  // during that time, leading to it switching over to pass-through mode.
  // Thus the number of entries is 2 below.
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_IDLE, context->trans->GetLoadState());
  }

  for (auto& c : context_list) {
    ReadAndVerifyTransaction(c->trans.get(), kRangeGET_TransactionOK);
  }

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests parallel validation on range requests with non-overlapping ranges.
TEST_F(HttpCacheRangeGetTest, ParallelValidationDifferentRanges) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
  }

  // Let 1st transaction complete headers phase for ranges 40-49.
  std::string first_read;
  MockHttpRequest request1(transaction);
  {
    auto& c = context_list[0];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request1, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    // Start writing to the cache so that MockDiskEntry::CouldBeSparse() returns
    // true.
    const int kBufferSize = 5;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    ReleaseBufferCompletionCallback cb(buffer.get());
    c->result = c->trans->Read(buffer.get(), kBufferSize, cb.callback());
    EXPECT_EQ(kBufferSize, cb.GetResult(c->result));

    std::string data_read(buffer->data(), kBufferSize);
    first_read = data_read;

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  // 2nd transaction requests ranges 30-39.
  transaction.request_headers = "Range: bytes = 30-39\r\n" EXTRA_HEADER;
  MockHttpRequest request2(transaction);
  {
    auto& c = context_list[1];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request2, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  std::string cache_key = request2.CacheKey();
  EXPECT_TRUE(cache.IsWriterPresent(cache_key));
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    auto& c = context_list[i];
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }

    if (i == 0) {
      ReadRemainingAndVerifyTransaction(c->trans.get(), first_read,
                                        transaction);
      continue;
    }

    transaction.data = "rg: 30-39 ";
    ReadAndVerifyTransaction(c->trans.get(), transaction);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Fetch from the cache to check that ranges 30-49 have been successfully
  // cached.
  {
    MockTransaction range_transaction(kRangeGET_TransactionOK);
    range_transaction.request_headers = "Range: bytes = 30-49\r\n" EXTRA_HEADER;
    range_transaction.data = "rg: 30-39 rg: 40-49 ";
    std::string headers;
    RunTransactionTestWithResponse(cache.http_cache(), range_transaction,
                                   &headers);
    Verify206Response(headers, 30, 49);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  context_list.clear();
}

// Tests that a request does not create Writers when readers is not empty.
TEST_F(HttpCacheRangeGetTest, DoNotCreateWritersWhenReaderExists) {
  MockHttpCache cache;

  // Save a request in the cache so that the next request can become a
  // reader.
  ScopedMockTransaction transaction(kRangeGET_Transaction);
  transaction.request_headers = EXTRA_HEADER;
  RunTransactionTest(cache.http_cache(), transaction);

  // Let this request be a reader since it doesn't need validation as per its
  // load flag.
  transaction.load_flags |= LOAD_SKIP_CACHE_VALIDATION;
  MockHttpRequest request(transaction);
  Context context;
  context.result = cache.CreateTransaction(&context.trans);
  ASSERT_THAT(context.result, IsOk());
  context.result = context.trans->Start(&request, context.callback.callback(),
                                        NetLogWithSource());
  base::RunLoop().RunUntilIdle();
  std::string cache_key = request.CacheKey();
  EXPECT_EQ(1, cache.GetCountReaders(cache_key));

  // A range request should now "not" create Writers while readers is still
  // non-empty.
  transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
  MockHttpRequest range_request(transaction);
  Context range_context;
  range_context.result = cache.CreateTransaction(&range_context.trans);
  ASSERT_THAT(range_context.result, IsOk());
  range_context.result = range_context.trans->Start(
      &range_request, range_context.callback.callback(), NetLogWithSource());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.GetCountReaders(cache_key));
  EXPECT_FALSE(cache.IsWriterPresent(cache_key));
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));
}

// Tests parallel validation on range requests can be successfully restarted
// when there is a cache lock timeout.
TEST_F(HttpCacheRangeGetTest, ParallelValidationCacheLockTimeout) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
  }

  // Let 1st transaction complete headers phase for ranges 40-49.
  std::string first_read;
  MockHttpRequest request1(transaction);
  {
    auto& c = context_list[0];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request1, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    // Start writing to the cache so that MockDiskEntry::CouldBeSparse() returns
    // true.
    const int kBufferSize = 5;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    ReleaseBufferCompletionCallback cb(buffer.get());
    c->result = c->trans->Read(buffer.get(), kBufferSize, cb.callback());
    EXPECT_EQ(kBufferSize, cb.GetResult(c->result));

    std::string data_read(buffer->data(), kBufferSize);
    first_read = data_read;

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  // Cache lock timeout will lead to dooming the entry since the transaction may
  // have already written the headers.
  cache.SimulateCacheLockTimeoutAfterHeaders();

  // 2nd transaction requests ranges 30-39.
  transaction.request_headers = "Range: bytes = 30-39\r\n" EXTRA_HEADER;
  MockHttpRequest request2(transaction);
  {
    auto& c = context_list[1];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request2, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  EXPECT_EQ(0, cache.GetCountDoneHeadersQueue(request1.CacheKey()));

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    auto& c = context_list[i];
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }

    if (i == 0) {
      ReadRemainingAndVerifyTransaction(c->trans.get(), first_read,
                                        transaction);
      continue;
    }

    transaction.data = "rg: 30-39 ";
    ReadAndVerifyTransaction(c->trans.get(), transaction);
  }

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests a full request and a simultaneous range request and the range request
// dooms the entry created by the full request due to not being able to
// conditionalize.
TEST_F(HttpCacheRangeGetTest, ParallelValidationCouldntConditionalize) {
  MockHttpCache cache;

  MockTransaction mock_transaction(kSimpleGET_Transaction);
  mock_transaction.url = kRangeGET_TransactionOK.url;
  // Remove the cache-control and other headers so that the response cannot be
  // conditionalized.
  mock_transaction.response_headers = "";
  MockHttpRequest request1(mock_transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
  }

  // Let 1st transaction complete headers phase for no range and read some part
  // of the response and write in the cache.
  std::string first_read;
  {
    ScopedMockTransaction transaction(mock_transaction);
    request1.url = GURL(kRangeGET_TransactionOK.url);
    auto& c = context_list[0];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request1, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    const int kBufferSize = 5;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    ReleaseBufferCompletionCallback cb(buffer.get());
    c->result = c->trans->Read(buffer.get(), kBufferSize, cb.callback());
    EXPECT_EQ(kBufferSize, cb.GetResult(c->result));

    std::string data_read(buffer->data(), kBufferSize);
    first_read = data_read;

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  // 2nd transaction requests a range.
  ScopedMockTransaction range_transaction(kRangeGET_TransactionOK);
  range_transaction.request_headers = "Range: bytes = 0-29\r\n" EXTRA_HEADER;
  MockHttpRequest request2(range_transaction);
  {
    auto& c = context_list[1];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request2, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  // The second request would have doomed the 1st entry and created a new entry.
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    auto& c = context_list[i];
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }

    if (i == 0) {
      ReadRemainingAndVerifyTransaction(c->trans.get(), first_read,
                                        mock_transaction);
      continue;
    }
    range_transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 ";
    ReadAndVerifyTransaction(c->trans.get(), range_transaction);
  }
  context_list.clear();
}

// Tests a 200 request and a simultaneous range request where conditionalization
// is possible.
TEST_F(HttpCacheRangeGetTest, ParallelValidationCouldConditionalize) {
  MockHttpCache cache;

  MockTransaction mock_transaction(kSimpleGET_Transaction);
  mock_transaction.url = kRangeGET_TransactionOK.url;
  mock_transaction.data = kFullRangeData;
  std::string response_headers_str = base::StrCat(
      {"ETag: StrongOne\n",
       "Content-Length:", base::NumberToString(strlen(kFullRangeData)), "\n"});
  mock_transaction.response_headers = response_headers_str.c_str();

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
  }

  // Let 1st transaction complete headers phase for no range and read some part
  // of the response and write in the cache.
  std::string first_read;
  MockHttpRequest request1(mock_transaction);
  {
    ScopedMockTransaction transaction(mock_transaction);
    request1.url = GURL(kRangeGET_TransactionOK.url);
    auto& c = context_list[0];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request1, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    const int kBufferSize = 5;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    ReleaseBufferCompletionCallback cb(buffer.get());
    c->result = c->trans->Read(buffer.get(), kBufferSize, cb.callback());
    EXPECT_EQ(kBufferSize, cb.GetResult(c->result));

    std::string data_read(buffer->data(), kBufferSize);
    first_read = data_read;

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  // 2nd transaction requests a range.
  ScopedMockTransaction range_transaction(kRangeGET_TransactionOK);
  range_transaction.request_headers = "Range: bytes = 0-29\r\n" EXTRA_HEADER;
  MockHttpRequest request2(range_transaction);
  {
    auto& c = context_list[1];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request2, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Finish and verify the first request.
  auto& c0 = context_list[0];
  c0->result = c0->callback.WaitForResult();
  ReadRemainingAndVerifyTransaction(c0->trans.get(), first_read,
                                    mock_transaction);

  // And the second.
  auto& c1 = context_list[1];
  c1->result = c1->callback.WaitForResult();

  range_transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 ";
  ReadAndVerifyTransaction(c1->trans.get(), range_transaction);
  context_list.clear();
}

// Tests parallel validation on range requests with overlapping ranges.
TEST_F(HttpCacheRangeGetTest, ParallelValidationOverlappingRanges) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
  }

  // Let 1st transaction complete headers phase for ranges 40-49.
  std::string first_read;
  MockHttpRequest request1(transaction);
  {
    auto& c = context_list[0];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request1, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    // Start writing to the cache so that MockDiskEntry::CouldBeSparse() returns
    // true.
    const int kBufferSize = 5;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    ReleaseBufferCompletionCallback cb(buffer.get());
    c->result = c->trans->Read(buffer.get(), kBufferSize, cb.callback());
    EXPECT_EQ(kBufferSize, cb.GetResult(c->result));

    std::string data_read(buffer->data(), kBufferSize);
    first_read = data_read;

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  // 2nd transaction requests ranges 30-49.
  transaction.request_headers = "Range: bytes = 30-49\r\n" EXTRA_HEADER;
  MockHttpRequest request2(transaction);
  {
    auto& c = context_list[1];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request2, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  std::string cache_key = request1.CacheKey();
  EXPECT_TRUE(cache.IsWriterPresent(cache_key));
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));

  // Should have created another transaction for the uncached range.
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    auto& c = context_list[i];
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }

    if (i == 0) {
      ReadRemainingAndVerifyTransaction(c->trans.get(), first_read,
                                        transaction);
      continue;
    }

    transaction.data = "rg: 30-39 rg: 40-49 ";
    ReadAndVerifyTransaction(c->trans.get(), transaction);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Fetch from the cache to check that ranges 30-49 have been successfully
  // cached.
  {
    MockTransaction range_transaction(kRangeGET_TransactionOK);
    range_transaction.request_headers = "Range: bytes = 30-49\r\n" EXTRA_HEADER;
    range_transaction.data = "rg: 30-39 rg: 40-49 ";
    std::string headers;
    RunTransactionTestWithResponse(cache.http_cache(), range_transaction,
                                   &headers);
    Verify206Response(headers, 30, 49);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests parallel validation on range requests with overlapping ranges and the
// impact of deleting the writer on transactions that have validated.
TEST_F(HttpCacheRangeGetTest, ParallelValidationRestartDoneHeaders) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
  }

  // Let 1st transaction complete headers phase for ranges 40-59.
  std::string first_read;
  transaction.request_headers = "Range: bytes = 40-59\r\n" EXTRA_HEADER;
  MockHttpRequest request1(transaction);
  {
    auto& c = context_list[0];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request1, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    // Start writing to the cache so that MockDiskEntry::CouldBeSparse() returns
    // true.
    const int kBufferSize = 10;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    ReleaseBufferCompletionCallback cb(buffer.get());
    c->result = c->trans->Read(buffer.get(), kBufferSize, cb.callback());
    EXPECT_EQ(kBufferSize, cb.GetResult(c->result));

    std::string data_read(buffer->data(), kBufferSize);
    first_read = data_read;

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  // 2nd transaction requests ranges 30-59.
  transaction.request_headers = "Range: bytes = 30-59\r\n" EXTRA_HEADER;
  MockHttpRequest request2(transaction);
  {
    auto& c = context_list[1];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request2, c->callback.callback(), NetLogWithSource());
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  std::string cache_key = request1.CacheKey();
  EXPECT_TRUE(cache.IsWriterPresent(cache_key));
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Delete the writer transaction.
  context_list[0].reset();

  base::RunLoop().RunUntilIdle();

  transaction.data = "rg: 30-39 rg: 40-49 rg: 50-59 ";
  ReadAndVerifyTransaction(context_list[1]->trans.get(), transaction);

  // Create another network transaction since the 2nd transaction is restarted.
  // 30-39 will be read from network, 40-49 from the cache and 50-59 from the
  // network.
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Fetch from the cache to check that ranges 30-49 have been successfully
  // cached.
  {
    MockTransaction range_transaction(kRangeGET_TransactionOK);
    range_transaction.request_headers = "Range: bytes = 30-49\r\n" EXTRA_HEADER;
    range_transaction.data = "rg: 30-39 rg: 40-49 ";
    std::string headers;
    RunTransactionTestWithResponse(cache.http_cache(), range_transaction,
                                   &headers);
    Verify206Response(headers, 30, 49);
  }

  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// A test of doing a range request to a cached 301 response
TEST_F(HttpCacheRangeGetTest, CachedRedirect) {
  RangeTransactionServer handler;
  handler.set_redirect(true);

  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 0-\r\n" EXTRA_HEADER;
  transaction.status = "HTTP/1.1 301 Moved Permanently";
  transaction.response_headers = "Location: /elsewhere\nContent-Length:5";
  transaction.data = "12345";
  MockHttpRequest request(transaction);

  TestCompletionCallback callback;

  // Write to the cache.
  {
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    if (rv == ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    ASSERT_THAT(rv, IsOk());

    const HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(nullptr, "Location", &location);
    EXPECT_EQ(location, "/elsewhere");

    ReadAndVerifyTransaction(trans.get(), transaction);
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Active entries in the cache are not retired synchronously. Make
  // sure the next run hits the MockHttpCache and open_count is
  // correct.
  base::RunLoop().RunUntilIdle();

  // Read from the cache.
  {
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    if (rv == ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    ASSERT_THAT(rv, IsOk());

    const HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(nullptr, "Location", &location);
    EXPECT_EQ(location, "/elsewhere");

    trans->DoneReading();
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now read the full body. This normally would not be done for a 301 by
  // higher layers, but e.g. a 500 could hit a further bug here.
  {
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    if (rv == ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    ASSERT_THAT(rv, IsOk());

    const HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(nullptr, "Location", &location);
    EXPECT_EQ(location, "/elsewhere");

    ReadAndVerifyTransaction(trans.get(), transaction);
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  // No extra open since it picks up a previous ActiveEntry.
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// A transaction that fails to validate an entry, while attempting to write
// the response, should still get data to its consumer even if the attempt to
// create a new entry fails.
TEST_F(HttpCacheSimpleGetTest, ValidationFailureWithCreateFailure) {
  MockHttpCache cache;
  MockHttpRequest request(kSimpleGET_Transaction);
  request.load_flags |= LOAD_VALIDATE_CACHE;
  std::vector<std::unique_ptr<Context>> context_list;

  // Create and run the first, successful, transaction to prime the cache.
  context_list.push_back(std::make_unique<Context>());
  auto& c1 = context_list.back();
  c1->result = cache.CreateTransaction(&c1->trans);
  ASSERT_THAT(c1->result, IsOk());
  EXPECT_EQ(LOAD_STATE_IDLE, c1->trans->GetLoadState());
  c1->result =
      c1->trans->Start(&request, c1->callback.callback(), NetLogWithSource());
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, c1->trans->GetLoadState());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(cache.IsWriterPresent(request.CacheKey()));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Create and start the second transaction, which will fail its validation
  // during the call to RunUntilIdle().
  context_list.push_back(std::make_unique<Context>());
  auto& c2 = context_list.back();
  c2->result = cache.CreateTransaction(&c2->trans);
  ASSERT_THAT(c2->result, IsOk());
  EXPECT_EQ(LOAD_STATE_IDLE, c2->trans->GetLoadState());
  c2->result =
      c2->trans->Start(&request, c2->callback.callback(), NetLogWithSource());
  // Expect idle at this point because we should be able to find and use the
  // Active Entry that c1 created instead of waiting on the cache to open the
  // entry.
  EXPECT_EQ(LOAD_STATE_IDLE, c2->trans->GetLoadState());

  cache.disk_cache()->set_fail_requests(true);
  // The transaction, c2, should now attempt to validate the entry, fail when it
  // receives a 200 OK response, attempt to create a new entry, fail to create,
  // and then continue onward without an entry.
  base::RunLoop().RunUntilIdle();

  // All requests depend on the writer, and the writer is between Start and
  // Read, i.e. idle.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_IDLE, context->trans->GetLoadState());
  }

  // Confirm that both transactions correctly Read() the data.
  for (auto& context : context_list) {
    if (context->result == ERR_IO_PENDING) {
      context->result = context->callback.WaitForResult();
    }
    ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Parallel validation results in 200.
TEST_F(HttpCacheSimpleGetTest, ParallelValidationNoMatch) {
  MockHttpCache cache;
  MockHttpRequest request(kSimpleGET_Transaction);
  request.load_flags |= LOAD_VALIDATE_CACHE;
  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 5;
  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());
    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // All requests are waiting for the active entry.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, context->trans->GetLoadState());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  // The first request should be a writer at this point, and the subsequent
  // requests should have passed the validation phase and created their own
  // entries since none of them matched the headers of the earlier one.
  EXPECT_TRUE(cache.IsWriterPresent(request.CacheKey()));

  EXPECT_EQ(5, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(5, cache.disk_cache()->create_count());

  // All requests depend on the writer, and the writer is between Start and
  // Read, i.e. idle.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_IDLE, context->trans->GetLoadState());
  }

  for (auto& context : context_list) {
    if (context->result == ERR_IO_PENDING) {
      context->result = context->callback.WaitForResult();
    }
    ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
  }

  EXPECT_EQ(5, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(5, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheRangeGetTest, Enormous) {
  // Test for how blockfile's limit on range namespace interacts with
  // HttpCache::Transaction.
  // See https://crbug.com/770694
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  auto backend_factory = std::make_unique<HttpCache::DefaultBackend>(
      DISK_CACHE, CACHE_BACKEND_BLOCKFILE,
      /*file_operations_factory=*/nullptr, temp_dir.GetPath(), 1024 * 1024,
      false);
  MockHttpCache cache(std::move(backend_factory));

  RangeTransactionServer handler;
  handler.set_length(2305843009213693962);

  // Prime with a range it can store.
  {
    ScopedMockTransaction transaction(kRangeGET_TransactionOK);
    transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
    transaction.data = "rg: 00-09 ";
    MockHttpRequest request(transaction);

    HttpResponseInfo response;
    RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                  &response);
    ASSERT_TRUE(response.headers != nullptr);
    EXPECT_EQ(206, response.headers->response_code());
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
  }

  // Try with a range it can't. Should still work.
  {
    ScopedMockTransaction transaction(kRangeGET_TransactionOK);
    transaction.request_headers =
        "Range: bytes = "
        "2305843009213693952-2305843009213693961\r\n" EXTRA_HEADER;
    transaction.data = "rg: 52-61 ";
    MockHttpRequest request(transaction);

    HttpResponseInfo response;
    RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                  &response);
    ASSERT_TRUE(response.headers != nullptr);
    EXPECT_EQ(206, response.headers->response_code());
    EXPECT_EQ(2, cache.network_layer()->transaction_count());
  }

  // Can't actually cache it due to backend limitations. If the network
  // transaction count is 2, this test isn't covering what it needs to.
  {
    ScopedMockTransaction transaction(kRangeGET_TransactionOK);
    transaction.request_headers =
        "Range: bytes = "
        "2305843009213693952-2305843009213693961\r\n" EXTRA_HEADER;
    transaction.data = "rg: 52-61 ";
    MockHttpRequest request(transaction);

    HttpResponseInfo response;
    RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                  &response);
    ASSERT_TRUE(response.headers != nullptr);
    EXPECT_EQ(206, response.headers->response_code());
    EXPECT_EQ(3, cache.network_layer()->transaction_count());
  }
}

// Parallel validation results in 200 for 1 transaction and validation matches
// for subsequent transactions.
TEST_F(HttpCacheSimpleGetTest, ParallelValidationNoMatch1) {
  MockHttpCache cache;
  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_VALIDATE_CACHE;
  MockHttpRequest validate_request(transaction);
  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 5;
  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];
    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    MockHttpRequest* this_request = &request;
    if (i == 1) {
      this_request = &validate_request;
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // All requests are waiting for the active entry.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, context->trans->GetLoadState());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  // The new entry will have all the transactions except the first one which
  // will continue in the doomed entry.
  EXPECT_EQ(kNumTransactions - 1,
            cache.GetCountWriterTransactions(validate_request.CacheKey()));

  EXPECT_EQ(1, cache.disk_cache()->doomed_count());

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_IDLE, context->trans->GetLoadState());
  }

  for (auto& context : context_list) {
    if (context->result == ERR_IO_PENDING) {
      context->result = context->callback.WaitForResult();
    }

    ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that a GET followed by a DELETE results in DELETE immediately starting
// the headers phase and the entry is doomed.
TEST_F(HttpCacheSimpleGetTest, ParallelValidationDelete) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);
  request.load_flags |= LOAD_VALIDATE_CACHE;

  MockHttpRequest delete_request(kSimpleGET_Transaction);
  delete_request.method = "DELETE";

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    MockHttpRequest* this_request = &request;
    if (i == 1) {
      this_request = &delete_request;
    }

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // All requests are waiting for the active entry.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, context->trans->GetLoadState());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  // The first request should be a writer at this point, and the subsequent
  // request should have passed the validation phase and doomed the existing
  // entry.
  EXPECT_TRUE(cache.disk_cache()->IsDiskEntryDoomed(request.CacheKey()));

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // All requests depend on the writer, and the writer is between Start and
  // Read, i.e. idle.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_IDLE, context->trans->GetLoadState());
  }

  for (auto& context : context_list) {
    if (context->result == ERR_IO_PENDING) {
      context->result = context->callback.WaitForResult();
    }
    ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that a transaction which is in validated queue can be destroyed without
// any impact to other transactions.
TEST_F(HttpCacheSimpleGetTest, ParallelValidationCancelValidated) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE;
  MockHttpRequest read_only_request(transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* current_request = i == 1 ? &read_only_request : &request;

    c->result = c->trans->Start(current_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();
  EXPECT_EQ(1, cache.GetCountWriterTransactions(cache_key));
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));

  context_list[1].reset();

  EXPECT_EQ(0, cache.GetCountDoneHeadersQueue(cache_key));

  // Complete the rest of the transactions.
  for (auto& context : context_list) {
    if (!context) {
      continue;
    }
    ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that an idle writer transaction can be deleted without impacting the
// existing writers.
TEST_F(HttpCacheSimpleGetTest, ParallelWritingCancelIdleTransaction) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Both transactions would be added to writers.
  std::string cache_key = request.CacheKey();
  EXPECT_EQ(kNumTransactions, cache.GetCountWriterTransactions(cache_key));

  context_list[1].reset();

  EXPECT_EQ(kNumTransactions - 1, cache.GetCountWriterTransactions(cache_key));

  // Complete the rest of the transactions.
  for (auto& context : context_list) {
    if (!context) {
      continue;
    }
    ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that a transaction which is in validated queue can timeout and start
// the headers phase again.
TEST_F(HttpCacheSimpleGetTest, ParallelValidationValidatedTimeout) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE;
  MockHttpRequest read_only_request(transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    MockHttpRequest* this_request = &request;
    if (i == 1) {
      this_request = &read_only_request;
      cache.SimulateCacheLockTimeoutAfterHeaders();
    }

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  // The first request should be a writer at this point, and the subsequent
  // requests should have completed validation, timed out and restarted.
  // Since it is a read only request, it will error out.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();
  EXPECT_TRUE(cache.IsWriterPresent(cache_key));
  EXPECT_EQ(0, cache.GetCountDoneHeadersQueue(cache_key));

  base::RunLoop().RunUntilIdle();

  int rv = context_list[1]->callback.WaitForResult();
  EXPECT_EQ(ERR_CACHE_MISS, rv);

  ReadAndVerifyTransaction(context_list[0]->trans.get(),
                           kSimpleGET_Transaction);
}

// Tests that a transaction which is in readers can be destroyed without
// any impact to other transactions.
TEST_F(HttpCacheSimpleGetTest, ParallelValidationCancelReader) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_VALIDATE_CACHE;
  MockHttpRequest validate_request(transaction);

  int kNumTransactions = 4;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    if (i == 3) {
      this_request = &validate_request;
      c->trans->SetBeforeNetworkStartCallback(base::BindOnce(&DeferCallback));
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();

  EXPECT_EQ(kNumTransactions - 1, cache.GetCountWriterTransactions(cache_key));
  EXPECT_TRUE(cache.IsHeadersTransactionPresent(cache_key));

  // Complete the response body.
  ReadAndVerifyTransaction(context_list[0]->trans.get(),
                           kSimpleGET_Transaction);

  // Rest of the transactions should move to readers.
  EXPECT_FALSE(cache.IsWriterPresent(cache_key));
  EXPECT_EQ(kNumTransactions - 2, cache.GetCountReaders(cache_key));
  EXPECT_EQ(0, cache.GetCountDoneHeadersQueue(cache_key));
  EXPECT_TRUE(cache.IsHeadersTransactionPresent(cache_key));

  // Add 2 new transactions.
  kNumTransactions = 6;

  for (int i = 4; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  EXPECT_EQ(2, cache.GetCountAddToEntryQueue(cache_key));

  // Delete a reader.
  context_list[1].reset();

  // Deleting the reader did not impact any other transaction.
  EXPECT_EQ(1, cache.GetCountReaders(cache_key));
  EXPECT_EQ(2, cache.GetCountAddToEntryQueue(cache_key));
  EXPECT_TRUE(cache.IsHeadersTransactionPresent(cache_key));

  // Resume network start for headers_transaction. It will doom the entry as it
  // will be a 200 and will go to network for the response body.
  context_list[3]->trans->ResumeNetworkStart();

  // The pending transactions will be added to a new entry as writers.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3, cache.GetCountWriterTransactions(cache_key));

  // Complete the rest of the transactions.
  for (int i = 2; i < kNumTransactions; ++i) {
    ReadAndVerifyTransaction(context_list[i]->trans.get(),
                             kSimpleGET_Transaction);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that when the only writer goes away, it immediately cleans up rather
// than wait for the network request to finish. See https://crbug.com/804868.
TEST_F(HttpCacheSimpleGetTest, HangingCacheWriteCleanup) {
  MockHttpCache mock_cache;
  MockHttpRequest request(kSimpleGET_Transaction);

  std::unique_ptr<HttpTransaction> transaction;
  mock_cache.CreateTransaction(&transaction);
  TestCompletionCallback callback;
  int result =
      transaction->Start(&request, callback.callback(), NetLogWithSource());

  // Get the transaction ready to read.
  result = callback.GetResult(result);

  // Read the first byte.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1);
  ReleaseBufferCompletionCallback buffer_callback(buffer.get());
  result = transaction->Read(buffer.get(), 1, buffer_callback.callback());
  EXPECT_EQ(1, buffer_callback.GetResult(result));

  // Read the second byte, but leave the cache write hanging.
  std::string cache_key = request.CacheKey();
  scoped_refptr<MockDiskEntry> entry =
      mock_cache.disk_cache()->GetDiskEntryRef(cache_key);
  entry->SetDefer(MockDiskEntry::DEFER_WRITE);

  auto buffer2 = base::MakeRefCounted<IOBufferWithSize>(1);
  ReleaseBufferCompletionCallback buffer_callback2(buffer2.get());
  result = transaction->Read(buffer2.get(), 1, buffer_callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, result);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_cache.IsWriterPresent(cache_key));

  // At this point the next byte should have been read from the network but is
  // waiting to be written to the cache. Destroy the transaction and make sure
  // that everything has been cleaned up.
  transaction = nullptr;
  EXPECT_FALSE(mock_cache.IsWriterPresent(cache_key));
  EXPECT_FALSE(mock_cache.network_layer()->last_transaction());
}

// Tests that a transaction writer can be destroyed mid-read.
// A waiting for read transaction should be able to read the data that was
// driven by the Read started by the cancelled writer.
TEST_F(HttpCacheSimpleGetTest, ParallelWritingCancelWriter) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_VALIDATE_CACHE;
  MockHttpRequest validate_request(transaction);

  const int kNumTransactions = 3;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    if (i == 2) {
      this_request = &validate_request;
      c->trans->SetBeforeNetworkStartCallback(base::BindOnce(&DeferCallback));
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = validate_request.CacheKey();
  EXPECT_TRUE(cache.IsHeadersTransactionPresent(cache_key));
  EXPECT_EQ(2, cache.GetCountWriterTransactions(cache_key));

  // Initiate Read from both writers and kill 1 of them mid-read.
  std::string first_read;
  for (int i = 0; i < 2; i++) {
    auto& c = context_list[i];
    const int kBufferSize = 5;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    ReleaseBufferCompletionCallback cb(buffer.get());
    c->result = c->trans->Read(buffer.get(), kBufferSize, cb.callback());
    EXPECT_EQ(ERR_IO_PENDING, c->result);
    // Deleting one writer at this point will not impact other transactions
    // since writers contain more transactions.
    if (i == 1) {
      context_list[0].reset();
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(kBufferSize, cb.GetResult(c->result));
      std::string data_read(buffer->data(), kBufferSize);
      first_read = data_read;
    }
  }

  // Resume network start for headers_transaction. It will doom the existing
  // entry and create a new entry due to validation returning a 200.
  auto& c = context_list[2];
  c->trans->ResumeNetworkStart();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.GetCountWriterTransactions(cache_key));

  // Complete the rest of the transactions.
  for (int i = 0; i < kNumTransactions; i++) {
    auto& context = context_list[i];
    if (!context) {
      continue;
    }
    if (i == 1) {
      ReadRemainingAndVerifyTransaction(context->trans.get(), first_read,
                                        kSimpleGET_Transaction);
    } else {
      ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
    }
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests the case when network read failure happens. Idle and waiting
// transactions should fail and headers transaction should be restarted.
TEST_F(HttpCacheSimpleGetTest, ParallelWritingNetworkReadFailed) {
  MockHttpCache cache;

  ScopedMockTransaction fail_transaction(kSimpleGET_Transaction);
  fail_transaction.read_return_code = ERR_INTERNET_DISCONNECTED;
  MockHttpRequest failing_request(fail_transaction);

  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE;
  MockHttpRequest read_request(transaction);

  const int kNumTransactions = 4;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    if (i == 0) {
      this_request = &failing_request;
    }
    if (i == 3) {
      this_request = &read_request;
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = read_request.CacheKey();
  EXPECT_EQ(3, cache.GetCountWriterTransactions(cache_key));
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));

  // Initiate Read from two writers and let the first get a network failure.
  for (int i = 0; i < 2; i++) {
    auto& c = context_list[i];
    const int kBufferSize = 5;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    c->result =
        c->trans->Read(buffer.get(), kBufferSize, c->callback.callback());
    EXPECT_EQ(ERR_IO_PENDING, c->result);
  }

  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < 2; i++) {
    auto& c = context_list[i];
    c->result = c->callback.WaitForResult();
    EXPECT_EQ(ERR_INTERNET_DISCONNECTED, c->result);
  }

  // The entry should have been doomed and destroyed and the headers transaction
  // restarted. Since headers transaction is read-only it will error out.
  auto& read_only = context_list[3];
  read_only->result = read_only->callback.WaitForResult();
  EXPECT_EQ(ERR_CACHE_MISS, read_only->result);

  EXPECT_FALSE(cache.IsWriterPresent(cache_key));

  // Invoke Read on the 3rd transaction and it should get the error code back.
  auto& c = context_list[2];
  const int kBufferSize = 5;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
  c->result = c->trans->Read(buffer.get(), kBufferSize, c->callback.callback());
  EXPECT_EQ(ERR_INTERNET_DISCONNECTED, c->result);
}

// Tests the case when cache write failure happens. Idle and waiting
// transactions should fail and headers transaction should be restarted.
TEST_F(HttpCacheSimpleGetTest, ParallelWritingCacheWriteFailed) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE;
  MockHttpRequest read_request(transaction);

  const int kNumTransactions = 4;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    if (i == 3) {
      this_request = &read_request;
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = read_request.CacheKey();
  EXPECT_EQ(3, cache.GetCountWriterTransactions(cache_key));
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));

  // Initiate Read from two writers and let the first get a cache write failure.
  cache.disk_cache()->set_soft_failures_mask(MockDiskEntry::FAIL_ALL);
  // We have to open the entry again to propagate the failure flag.
  disk_cache::Entry* en;
  cache.OpenBackendEntry(cache_key, &en);
  en->Close();
  const int kBufferSize = 5;
  std::vector<scoped_refptr<IOBuffer>> buffer(
      3, base::MakeRefCounted<IOBufferWithSize>(kBufferSize));
  for (int i = 0; i < 2; i++) {
    auto& c = context_list[i];
    c->result =
        c->trans->Read(buffer[i].get(), kBufferSize, c->callback.callback());
    EXPECT_EQ(ERR_IO_PENDING, c->result);
  }

  std::string first_read;
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < 2; i++) {
    auto& c = context_list[i];
    c->result = c->callback.WaitForResult();
    if (i == 0) {
      EXPECT_EQ(5, c->result);
      std::string data_read(buffer[i]->data(), kBufferSize);
      first_read = data_read;
    } else {
      EXPECT_EQ(ERR_CACHE_WRITE_FAILURE, c->result);
    }
  }

  // The entry should have been doomed and destroyed and the headers transaction
  // restarted. Since headers transaction is read-only it will error out.
  auto& read_only = context_list[3];
  read_only->result = read_only->callback.WaitForResult();
  EXPECT_EQ(ERR_CACHE_MISS, read_only->result);

  EXPECT_FALSE(cache.IsWriterPresent(cache_key));

  // Invoke Read on the 3rd transaction and it should get the error code back.
  auto& c = context_list[2];
  c->result =
      c->trans->Read(buffer[2].get(), kBufferSize, c->callback.callback());
  EXPECT_EQ(ERR_CACHE_WRITE_FAILURE, c->result);

  // The first transaction should be able to continue to read from the network
  // without writing to the cache.
  auto& succ_read = context_list[0];
  ReadRemainingAndVerifyTransaction(succ_read->trans.get(), first_read,
                                    kSimpleGET_Transaction);
}

using HttpCacheSimplePostTest = HttpCacheTest;

// Tests that POST requests do not join existing transactions for parallel
// writing to the cache. Note that two POSTs only map to the same entry if their
// upload data identifier is same and that should happen for back-forward case
// (LOAD_ONLY_FROM_CACHE). But this test tests without LOAD_ONLY_FROM_CACHE
// because read-only transactions anyways do not join parallel writing.
// TODO(shivanisha) Testing this because it is allowed by the code but looks
// like the code should disallow two POSTs without LOAD_ONLY_FROM_CACHE with the
// same upload data identifier to map to the same entry.
TEST_F(HttpCacheSimplePostTest, ParallelWritingDisallowed) {
  MockHttpCache cache;

  MockTransaction transaction(kSimplePOST_Transaction);

  const int64_t kUploadId = 1;  // Just a dummy value.

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers),
                                              kUploadId);

  // Note that both transactions should have the same upload_data_stream
  // identifier to map to the same entry.
  transaction.load_flags = LOAD_SKIP_CACHE_VALIDATION;
  MockHttpRequest request(transaction);
  request.upload_data_stream = &upload_data_stream;

  const int kNumTransactions = 2;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());

    // Complete the headers phase request.
    base::RunLoop().RunUntilIdle();
  }

  std::string cache_key = request.CacheKey();
  // Only the 1st transaction gets added to writers.
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));
  EXPECT_EQ(1, cache.GetCountWriterTransactions(cache_key));

  // Read the 1st transaction.
  ReadAndVerifyTransaction(context_list[0]->trans.get(),
                           kSimplePOST_Transaction);

  // 2nd transaction should now become a reader.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, cache.GetCountReaders(cache_key));
  EXPECT_EQ(0, cache.GetCountDoneHeadersQueue(cache_key));
  ReadAndVerifyTransaction(context_list[1]->trans.get(),
                           kSimplePOST_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  context_list.clear();
}

// Tests the case when parallel writing succeeds. Tests both idle and waiting
// transactions.
TEST_F(HttpCacheSimpleGetTest, ParallelWritingSuccess) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE;
  MockHttpRequest read_request(transaction);

  const int kNumTransactions = 4;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    if (i == 3) {
      this_request = &read_request;
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();
  EXPECT_EQ(3, cache.GetCountWriterTransactions(cache_key));
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));

  // Initiate Read from two writers.
  const int kBufferSize = 5;
  std::vector<scoped_refptr<IOBuffer>> buffer(
      3, base::MakeRefCounted<IOBufferWithSize>(kBufferSize));
  for (int i = 0; i < 2; i++) {
    auto& c = context_list[i];
    c->result =
        c->trans->Read(buffer[i].get(), kBufferSize, c->callback.callback());
    EXPECT_EQ(ERR_IO_PENDING, c->result);
  }

  std::vector<std::string> first_read(2);
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < 2; i++) {
    auto& c = context_list[i];
    c->result = c->callback.WaitForResult();
    EXPECT_EQ(5, c->result);
    std::string data_read(buffer[i]->data(), kBufferSize);
    first_read[i] = data_read;
  }
  EXPECT_EQ(first_read[0], first_read[1]);

  // The first transaction should be able to continue to read from the network
  // without writing to the cache.
  for (int i = 0; i < 2; i++) {
    auto& c = context_list[i];
    ReadRemainingAndVerifyTransaction(c->trans.get(), first_read[i],
                                      kSimpleGET_Transaction);
    if (i == 0) {
      // Remaining transactions should now be readers.
      EXPECT_EQ(3, cache.GetCountReaders(cache_key));
    }
  }

  // Verify the rest of the transactions.
  for (int i = 2; i < kNumTransactions; i++) {
    auto& c = context_list[i];
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  context_list.clear();
}

// Tests the case when parallel writing involves things bigger than what cache
// can store. In this case, the best we can do is re-fetch it.
TEST_F(HttpCacheSimpleGetTest, ParallelWritingHuge) {
  MockHttpCache cache;
  cache.disk_cache()->set_max_file_size(10);

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  std::string response_headers = base::StrCat(
      {kSimpleGET_Transaction.response_headers, "Content-Length: ",
       base::NumberToString(strlen(kSimpleGET_Transaction.data)), "\n"});
  transaction.response_headers = response_headers.c_str();
  MockHttpRequest request(transaction);

  const int kNumTransactions = 4;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Start them up.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();
  EXPECT_EQ(1, cache.GetCountWriterTransactions(cache_key));
  EXPECT_EQ(kNumTransactions - 1, cache.GetCountDoneHeadersQueue(cache_key));

  // Initiate Read from first transaction.
  const int kBufferSize = 5;
  std::vector<scoped_refptr<IOBuffer>> buffer(
      kNumTransactions, base::MakeRefCounted<IOBufferWithSize>(kBufferSize));
  auto& c = context_list[0];
  c->result =
      c->trans->Read(buffer[0].get(), kBufferSize, c->callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, c->result);

  // ... and complete it.
  std::vector<std::string> first_read(kNumTransactions);
  base::RunLoop().RunUntilIdle();
  c->result = c->callback.WaitForResult();
  EXPECT_EQ(kBufferSize, c->result);
  std::string data_read(buffer[0]->data(), kBufferSize);
  first_read[0] = data_read;
  EXPECT_EQ("<html", first_read[0]);

  // Complete all of them.
  for (int i = 0; i < kNumTransactions; i++) {
    ReadRemainingAndVerifyTransaction(context_list[i]->trans.get(),
                                      first_read[i], kSimpleGET_Transaction);
  }

  // Sadly all of them have to hit the network
  EXPECT_EQ(kNumTransactions, cache.network_layer()->transaction_count());

  context_list.clear();
}

// Tests that network transaction's info is saved correctly when a writer
// transaction that created the network transaction becomes a reader. Also
// verifies that the network bytes are only attributed to the transaction that
// created the network transaction.
TEST_F(HttpCacheSimpleGetTest, ParallelWritingVerifyNetworkBytes) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  const int kNumTransactions = 2;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();
  EXPECT_EQ(2, cache.GetCountWriterTransactions(cache_key));
  EXPECT_EQ(0, cache.GetCountDoneHeadersQueue(cache_key));

  // Get the network bytes read by the first transaction.
  int total_received_bytes = context_list[0]->trans->GetTotalReceivedBytes();
  EXPECT_GT(total_received_bytes, 0);

  // Complete Read by the 2nd transaction so that the 1st transaction that
  // created the network transaction is now a reader.
  ReadAndVerifyTransaction(context_list[1]->trans.get(),
                           kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.GetCountReaders(cache_key));

  // Verify that the network bytes read are not attributed to the 2nd
  // transaction but to the 1st.
  EXPECT_EQ(0, context_list[1]->trans->GetTotalReceivedBytes());

  EXPECT_GE(total_received_bytes,
            context_list[0]->trans->GetTotalReceivedBytes());

  ReadAndVerifyTransaction(context_list[0]->trans.get(),
                           kSimpleGET_Transaction);
}

// Tests than extra Read from the consumer should not hang/crash the browser.
TEST_F(HttpCacheSimpleGetTest, ExtraRead) {
  MockHttpCache cache;
  MockHttpRequest request(kSimpleGET_Transaction);
  Context c;

  c.result = cache.CreateTransaction(&c.trans);
  ASSERT_THAT(c.result, IsOk());

  c.result =
      c.trans->Start(&request, c.callback.callback(), NetLogWithSource());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();
  EXPECT_EQ(1, cache.GetCountWriterTransactions(cache_key));
  EXPECT_EQ(0, cache.GetCountDoneHeadersQueue(cache_key));

  ReadAndVerifyTransaction(c.trans.get(), kSimpleGET_Transaction);

  // Perform an extra Read.
  const int kBufferSize = 10;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
  c.result = c.trans->Read(buffer.get(), kBufferSize, c.callback.callback());
  EXPECT_EQ(0, c.result);
}

// Tests when a writer is destroyed mid-read, all the other writer transactions
// can continue writing to the entry.
TEST_F(HttpCacheSimpleGetTest, ParallelValidationCancelWriter) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 22\n"
      "Etag: \"foopy\"\n";
  MockHttpRequest request(transaction);

  const int kNumTransactions = 3;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();
  EXPECT_EQ(kNumTransactions, cache.GetCountWriterTransactions(cache_key));

  // Let first transaction read some bytes.
  {
    auto& c = context_list[0];
    const int kBufferSize = 5;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    ReleaseBufferCompletionCallback cb(buffer.get());
    c->result = c->trans->Read(buffer.get(), kBufferSize, cb.callback());
    EXPECT_EQ(kBufferSize, cb.GetResult(c->result));
  }

  // Deleting the active transaction at this point will not impact the other
  // transactions since there are other transactions in writers.
  context_list[0].reset();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Complete the rest of the transactions.
  for (auto& context : context_list) {
    if (!context) {
      continue;
    }
    ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
  }
}

// Tests that when StopCaching is invoked on a writer, dependent transactions
// are restarted.
TEST_F(HttpCacheSimpleGetTest, ParallelValidationStopCaching) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE;
  MockHttpRequest read_only_request(transaction);

  const int kNumTransactions = 2;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    if (i == 1) {
      this_request = &read_only_request;
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();
  EXPECT_EQ(kNumTransactions - 1, cache.GetCountWriterTransactions(cache_key));
  EXPECT_EQ(1, cache.GetCountDoneHeadersQueue(cache_key));

  // Invoking StopCaching on the writer will lead to dooming the entry and
  // restarting the validated transactions. Since it is a read-only transaction
  // it will error out.
  context_list[0]->trans->StopCaching();

  base::RunLoop().RunUntilIdle();

  int rv = context_list[1]->callback.WaitForResult();
  EXPECT_EQ(ERR_CACHE_MISS, rv);

  ReadAndVerifyTransaction(context_list[0]->trans.get(),
                           kSimpleGET_Transaction);
}

// Tests that when StopCaching is invoked on a writer transaction, it is a
// no-op if there are other writer transactions.
TEST_F(HttpCacheSimpleGetTest, ParallelWritersStopCachingNoOp) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= LOAD_VALIDATE_CACHE;
  MockHttpRequest validate_request(transaction);

  const int kNumTransactions = 3;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    if (i == 2) {
      this_request = &validate_request;
      c->trans->SetBeforeNetworkStartCallback(base::BindOnce(&DeferCallback));
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::string cache_key = request.CacheKey();
  EXPECT_TRUE(cache.IsHeadersTransactionPresent(cache_key));
  EXPECT_EQ(kNumTransactions - 1, cache.GetCountWriterTransactions(cache_key));

  // Invoking StopCaching on the writer will be a no-op since there are multiple
  // transaction in writers.
  context_list[0]->trans->StopCaching();

  // Resume network start for headers_transaction.
  auto& c = context_list[2];
  c->trans->ResumeNetworkStart();
  base::RunLoop().RunUntilIdle();
  // After validation old entry will be doomed and headers_transaction will be
  // added to the new entry.
  EXPECT_EQ(1, cache.GetCountWriterTransactions(cache_key));

  // Complete the rest of the transactions.
  for (auto& context : context_list) {
    if (!context) {
      continue;
    }
    ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that a transaction is currently in headers phase and is destroyed
// leading to destroying the entry.
TEST_F(HttpCacheSimpleGetTest, ParallelValidationCancelHeaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  const int kNumTransactions = 2;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    if (i == 0) {
      c->trans->SetBeforeNetworkStartCallback(base::BindOnce(&DeferCallback));
    }

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  base::RunLoop().RunUntilIdle();

  std::string cache_key = request.CacheKey();
  EXPECT_TRUE(cache.IsHeadersTransactionPresent(cache_key));
  EXPECT_EQ(1, cache.GetCountAddToEntryQueue(cache_key));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Delete the headers transaction.
  context_list[0].reset();

  base::RunLoop().RunUntilIdle();

  // Complete the rest of the transactions.
  for (auto& context : context_list) {
    if (!context) {
      continue;
    }
    ReadAndVerifyTransaction(context->trans.get(), kSimpleGET_Transaction);
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Similar to the above test, except here cache write fails and the
// validated transactions should be restarted.
TEST_F(HttpCacheSimpleGetTest, ParallelWritersFailWrite) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  const int kNumTransactions = 5;
  std::vector<std::unique_ptr<Context>> context_list;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    auto& c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
    EXPECT_EQ(LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // All requests are waiting for the active entry.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, context->trans->GetLoadState());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  // All transactions become writers.
  std::string cache_key = request.CacheKey();
  EXPECT_EQ(kNumTransactions, cache.GetCountWriterTransactions(cache_key));

  // All requests depend on the writer, and the writer is between Start and
  // Read, i.e. idle.
  for (auto& context : context_list) {
    EXPECT_EQ(LOAD_STATE_IDLE, context->trans->GetLoadState());
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Fail the request.
  cache.disk_cache()->set_soft_failures_mask(MockDiskEntry::FAIL_ALL);
  // We have to open the entry again to propagate the failure flag.
  disk_cache::Entry* en;
  cache.OpenBackendEntry(cache_key, &en);
  en->Close();

  for (int i = 0; i < kNumTransactions; ++i) {
    auto& c = context_list[i];
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }
    if (i == 1) {
      // The earlier entry must be destroyed and its disk entry doomed.
      EXPECT_TRUE(cache.disk_cache()->IsDiskEntryDoomed(cache_key));
    }

    if (i == 0) {
      // Consumer gets the response even if cache write failed.
      ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
    } else {
      // Read should lead to a failure being returned.
      const int kBufferSize = 5;
      auto buffer = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
      ReleaseBufferCompletionCallback cb(buffer.get());
      c->result = c->trans->Read(buffer.get(), kBufferSize, cb.callback());
      EXPECT_EQ(ERR_CACHE_WRITE_FAILURE, cb.GetResult(c->result));
    }
  }
}

// This is a test for http://code.google.com/p/chromium/issues/detail?id=4769.
// If cancelling a request is racing with another request for the same resource
// finishing, we have to make sure that we remove both transactions from the
// entry.
TEST_F(HttpCacheSimpleGetTest, RacingReaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);
  MockHttpRequest reader_request(kSimpleGET_Transaction);
  reader_request.load_flags = LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    Context* c = context_list[i].get();

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    if (i == 1 || i == 2) {
      this_request = &reader_request;
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  // The first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  Context* c = context_list[0].get();
  ASSERT_THAT(c->result, IsError(ERR_IO_PENDING));
  c->result = c->callback.WaitForResult();
  ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);

  // Now all transactions should be waiting for read to be invoked. Two readers
  // are because of the load flags and remaining two transactions were converted
  // to readers after skipping validation. Note that the remaining two went on
  // to process the headers in parallel with readers present on the entry.
  EXPECT_EQ(LOAD_STATE_IDLE, context_list[2]->trans->GetLoadState());
  EXPECT_EQ(LOAD_STATE_IDLE, context_list[3]->trans->GetLoadState());

  c = context_list[1].get();
  ASSERT_THAT(c->result, IsError(ERR_IO_PENDING));
  c->result = c->callback.WaitForResult();
  if (c->result == OK) {
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // At this point we have one reader, two pending transactions and a task on
  // the queue to move to the next transaction. Now we cancel the request that
  // is the current reader, and expect the queued task to be able to start the
  // next request.

  c = context_list[2].get();
  c->trans.reset();

  for (int i = 3; i < kNumTransactions; ++i) {
    c = context_list[i].get();
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }
    if (c->result == OK) {
      ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
    }
  }

  // We should not have had to re-open the disk entry.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we can doom an entry with pending transactions and delete one of
// the pending transactions before the first one completes.
// See http://code.google.com/p/chromium/issues/detail?id=25588
TEST_F(HttpCacheSimpleGetTest, DoomWithPending) {
  // We need simultaneous doomed / not_doomed entries so let's use a real cache.
  MockHttpCache cache(HttpCache::DefaultBackend::InMemory(1024 * 1024));

  MockHttpRequest request(kSimpleGET_Transaction);
  MockHttpRequest writer_request(kSimpleGET_Transaction);
  writer_request.load_flags = LOAD_BYPASS_CACHE;

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 4;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    Context* c = context_list[i].get();

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    MockHttpRequest* this_request = &request;
    if (i == 3) {
      this_request = &writer_request;
    }

    c->result = c->trans->Start(this_request, c->callback.callback(),
                                NetLogWithSource());
  }

  base::RunLoop().RunUntilIdle();

  // The first request should be a writer at this point, and the two subsequent
  // requests should be pending. The last request doomed the first entry.

  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  // Cancel the second transaction. Note that this and the 3rd transactions
  // would have completed their headers phase and would be waiting in the
  // done_headers_queue when the 2nd transaction is cancelled.
  context_list[1].reset();

  for (int i = 0; i < kNumTransactions; ++i) {
    if (i == 1) {
      continue;
    }
    Context* c = context_list[i].get();
    ASSERT_THAT(c->result, IsError(ERR_IO_PENDING));
    c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }
}

TEST_F(HttpCacheTest, DoomDoesNotSetHints) {
  // Test that a doomed writer doesn't set in-memory index hints.
  MockHttpCache cache;
  cache.disk_cache()->set_support_in_memory_entry_data(true);

  // Request 1 is a normal one to a no-cache/no-etag resource, to potentially
  // set a "this is unvalidatable" hint in the cache. We also need it to
  // actually write out to the doomed entry after request 2 does its thing,
  // so its transaction is paused.
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "Cache-Control: no-cache\n";
  MockHttpRequest request1(transaction);

  Context c1;
  c1.result = cache.CreateTransaction(&c1.trans);
  ASSERT_THAT(c1.result, IsOk());
  c1.trans->SetBeforeNetworkStartCallback(
      base::BindOnce([](bool* defer) { *defer = true; }));
  c1.result =
      c1.trans->Start(&request1, c1.callback.callback(), NetLogWithSource());
  ASSERT_THAT(c1.result, IsError(ERR_IO_PENDING));

  // It starts, copies over headers info, but doesn't get to proceed.
  base::RunLoop().RunUntilIdle();

  // Request 2 sets LOAD_BYPASS_CACHE to force the first one to be doomed ---
  // it'll want to be a writer.
  transaction.response_headers = kSimpleGET_Transaction.response_headers;
  MockHttpRequest request2(transaction);
  request2.load_flags = LOAD_BYPASS_CACHE;

  Context c2;
  c2.result = cache.CreateTransaction(&c2.trans);
  ASSERT_THAT(c2.result, IsOk());
  c2.result =
      c2.trans->Start(&request2, c2.callback.callback(), NetLogWithSource());
  ASSERT_THAT(c2.result, IsError(ERR_IO_PENDING));

  // Run Request2, then let the first one wrap up.
  base::RunLoop().RunUntilIdle();
  c2.callback.WaitForResult();
  ReadAndVerifyTransaction(c2.trans.get(), kSimpleGET_Transaction);

  c1.trans->ResumeNetworkStart();
  c1.callback.WaitForResult();
  ReadAndVerifyTransaction(c1.trans.get(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  // Request 3 tries to read from cache, and it should successfully do so. It's
  // run after the previous two transactions finish so it doesn't try to
  // cooperate with them, and is entirely driven by the state of the cache.
  MockHttpRequest request3(kSimpleGET_Transaction);
  Context context3;
  context3.result = cache.CreateTransaction(&context3.trans);
  ASSERT_THAT(context3.result, IsOk());
  context3.result = context3.trans->Start(
      &request3, context3.callback.callback(), NetLogWithSource());
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(context3.result, IsError(ERR_IO_PENDING));
  context3.result = context3.callback.WaitForResult();
  ReadAndVerifyTransaction(context3.trans.get(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// This is a test for http://code.google.com/p/chromium/issues/detail?id=4731.
// We may attempt to delete an entry synchronously with the act of adding a new
// transaction to said entry.
TEST_F(HttpCacheTest, FastNoStoreGetDoneWithPending) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kFastNoStoreGET_Transaction);
  // The headers will be served right from the call to Start() the request.
  MockHttpRequest request(transaction);
  FastTransactionServer request_handler;

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 3;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    Context* c = context_list[i].get();

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::RunLoop().RunUntilIdle();

  // The first request should be a writer at this point, and the subsequent
  // requests should have completed validation. Since the validation does not
  // result in a match, a new entry would be created.

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());

  // Now, make sure that the second request asks for the entry not to be stored.
  request_handler.set_no_store(true);

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i].get();
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }
    ReadAndVerifyTransaction(c->trans.get(), transaction);
    context_list[i].reset();
  }

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheSimpleGetTest, ManyWritersCancelFirst) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(std::make_unique<Context>());
    Context* c = context_list[i].get();

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // Allow all requests to move from the Create queue to the active entry.
  // All would have been added to writers.
  base::RunLoop().RunUntilIdle();
  std::string cache_key = *HttpCache::GenerateCacheKeyForRequest(&request);
  EXPECT_EQ(kNumTransactions, cache.GetCountWriterTransactions(cache_key));

  // The second transaction skipped validation, thus only one network
  // transaction is created.
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i].get();
    if (c->result == ERR_IO_PENDING) {
      c->result = c->callback.WaitForResult();
    }
    // Destroy only the first transaction.
    // This should not impact the other writer transaction and the network
    // transaction will continue to be used by that transaction.
    if (i == 0) {
      context_list[i].reset();
    }
  }

  // Complete the rest of the transactions.
  for (int i = 1; i < kNumTransactions; ++i) {
    Context* c = context_list[i].get();
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we can cancel requests that are queued waiting to open the disk
// cache entry.
TEST_F(HttpCacheSimpleGetTest, ManyWritersCancelCreate) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(std::make_unique<Context>());
    Context* c = context_list[i].get();

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // The first request should be creating the disk cache entry and the others
  // should be pending.

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Cancel a request from the pending queue.
  context_list[3].reset();

  // Cancel the request that is creating the entry. This will force the pending
  // operations to restart.
  context_list[0].reset();

  // Complete the rest of the transactions.
  for (int i = 1; i < kNumTransactions; i++) {
    Context* c = context_list[i].get();
    if (c) {
      c->result = c->callback.GetResult(c->result);
      ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
    }
  }

  // We should have had to re-create the disk entry.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we can cancel a single request to open a disk cache entry.
TEST_F(HttpCacheSimpleGetTest, CancelCreate) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  auto c = std::make_unique<Context>();

  c->result = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(c->result, IsOk());

  c->result =
      c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_THAT(c->result, IsError(ERR_IO_PENDING));

  // Release the reference that the mock disk cache keeps for this entry, so
  // that we test that the http cache handles the cancellation correctly.
  cache.disk_cache()->ReleaseAll();
  c.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we delete/create entries even if multiple requests are queued.
TEST_F(HttpCacheSimpleGetTest, ManyWritersBypassCache) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);
  request.load_flags = LOAD_BYPASS_CACHE;

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(std::make_unique<Context>());
    Context* c = context_list[i].get();

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // The first request should be deleting the disk cache entry and the others
  // should be pending.

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  // Complete the transactions.
  for (int i = 0; i < kNumTransactions; i++) {
    Context* c = context_list[i].get();
    c->result = c->callback.GetResult(c->result);
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // We should have had to re-create the disk entry multiple times.

  EXPECT_EQ(5, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(5, cache.disk_cache()->create_count());
}

// Tests that a (simulated) timeout allows transactions waiting on the cache
// lock to continue.
TEST_F(HttpCacheSimpleGetTest, WriterTimeout) {
  MockHttpCache cache;
  cache.SimulateCacheLockTimeout();

  MockHttpRequest request(kSimpleGET_Transaction);
  Context c1, c2;
  ASSERT_THAT(cache.CreateTransaction(&c1.trans), IsOk());
  ASSERT_EQ(ERR_IO_PENDING, c1.trans->Start(&request, c1.callback.callback(),
                                            NetLogWithSource()));
  ASSERT_THAT(cache.CreateTransaction(&c2.trans), IsOk());
  ASSERT_EQ(ERR_IO_PENDING, c2.trans->Start(&request, c2.callback.callback(),
                                            NetLogWithSource()));

  // The second request is queued after the first one.

  c2.callback.WaitForResult();
  ReadAndVerifyTransaction(c2.trans.get(), kSimpleGET_Transaction);

  // Complete the first transaction.
  c1.callback.WaitForResult();
  ReadAndVerifyTransaction(c1.trans.get(), kSimpleGET_Transaction);
}

// Tests that a (simulated) timeout allows transactions waiting on the cache
// lock to continue but read only transactions to error out.
TEST_F(HttpCacheSimpleGetTest, WriterTimeoutReadOnlyError) {
  MockHttpCache cache;

  // Simulate timeout.
  cache.SimulateCacheLockTimeout();

  MockHttpRequest request(kSimpleGET_Transaction);
  Context c1, c2;
  ASSERT_THAT(cache.CreateTransaction(&c1.trans), IsOk());
  ASSERT_EQ(ERR_IO_PENDING, c1.trans->Start(&request, c1.callback.callback(),
                                            NetLogWithSource()));

  request.load_flags = LOAD_ONLY_FROM_CACHE;
  ASSERT_THAT(cache.CreateTransaction(&c2.trans), IsOk());
  ASSERT_EQ(ERR_IO_PENDING, c2.trans->Start(&request, c2.callback.callback(),
                                            NetLogWithSource()));

  // The second request is queued after the first one.
  int res = c2.callback.WaitForResult();
  ASSERT_EQ(ERR_CACHE_MISS, res);

  // Complete the first transaction.
  c1.callback.WaitForResult();
  ReadAndVerifyTransaction(c1.trans.get(), kSimpleGET_Transaction);
}

TEST_F(HttpCacheSimpleGetTest, AbandonedCacheRead) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  MockHttpRequest request(kSimpleGET_Transaction);
  TestCompletionCallback callback;

  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());
  int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
  }
  ASSERT_THAT(rv, IsOk());

  auto buf = base::MakeRefCounted<IOBufferWithSize>(256);
  rv = trans->Read(buf.get(), 256, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Test that destroying the transaction while it is reading from the cache
  // works properly.
  trans.reset();

  // Make sure we pump any pending events, which should include a call to
  // HttpCache::Transaction::OnCacheReadCompleted.
  base::RunLoop().RunUntilIdle();
}

// Tests that we can delete the HttpCache and deal with queued transactions
// ("waiting for the backend" as opposed to Active or Doomed entries).
TEST_F(HttpCacheSimpleGetTest, ManyWritersDeleteCache) {
  auto cache = std::make_unique<MockHttpCache>(
      std::make_unique<MockBackendNoCbFactory>());

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(std::make_unique<Context>());
    Context* c = context_list[i].get();

    c->result = cache->CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());

    c->result =
        c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  }

  // The first request should be creating the disk cache entry and the others
  // should be pending.

  EXPECT_EQ(0, cache->network_layer()->transaction_count());
  EXPECT_EQ(0, cache->disk_cache()->open_count());
  EXPECT_EQ(0, cache->disk_cache()->create_count());

  cache.reset();
}

// Tests that we queue requests when initializing the backend.
TEST_F(HttpCacheSimpleGetTest, WaitForBackend) {
  auto factory = std::make_unique<MockBlockingBackendFactory>();
  MockBlockingBackendFactory* factory_ptr = factory.get();
  MockHttpCache cache(std::move(factory));

  MockHttpRequest request0(kSimpleGET_Transaction);
  MockHttpRequest request1(kTypicalGET_Transaction);
  MockHttpRequest request2(kETagGET_Transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 3;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(std::make_unique<Context>());
    Context* c = context_list[i].get();

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
  }

  context_list[0]->result = context_list[0]->trans->Start(
      &request0, context_list[0]->callback.callback(), NetLogWithSource());
  context_list[1]->result = context_list[1]->trans->Start(
      &request1, context_list[1]->callback.callback(), NetLogWithSource());
  context_list[2]->result = context_list[2]->trans->Start(
      &request2, context_list[2]->callback.callback(), NetLogWithSource());

  // Just to make sure that everything is still pending.
  base::RunLoop().RunUntilIdle();

  // The first request should be creating the disk cache.
  EXPECT_FALSE(context_list[0]->callback.have_result());

  factory_ptr->FinishCreation();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    EXPECT_TRUE(context_list[i]->callback.have_result());
    context_list[i].reset();
  }
}

// Tests that we can cancel requests that are queued waiting for the backend
// to be initialized.
TEST_F(HttpCacheSimpleGetTest, WaitForBackend_CancelCreate) {
  auto factory = std::make_unique<MockBlockingBackendFactory>();
  MockBlockingBackendFactory* factory_ptr = factory.get();
  MockHttpCache cache(std::move(factory));

  MockHttpRequest request0(kSimpleGET_Transaction);
  MockHttpRequest request1(kTypicalGET_Transaction);
  MockHttpRequest request2(kETagGET_Transaction);

  std::vector<std::unique_ptr<Context>> context_list;
  const int kNumTransactions = 3;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(std::make_unique<Context>());
    Context* c = context_list[i].get();

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_THAT(c->result, IsOk());
  }

  context_list[0]->result = context_list[0]->trans->Start(
      &request0, context_list[0]->callback.callback(), NetLogWithSource());
  context_list[1]->result = context_list[1]->trans->Start(
      &request1, context_list[1]->callback.callback(), NetLogWithSource());
  context_list[2]->result = context_list[2]->trans->Start(
      &request2, context_list[2]->callback.callback(), NetLogWithSource());

  // Just to make sure that everything is still pending.
  base::RunLoop().RunUntilIdle();

  // The first request should be creating the disk cache.
  EXPECT_FALSE(context_list[0]->callback.have_result());

  // Cancel a request from the pending queue.
  context_list[1].reset();

  // Cancel the request that is creating the entry.
  context_list[0].reset();

  // Complete the last transaction.
  factory_ptr->FinishCreation();

  context_list[2]->result =
      context_list[2]->callback.GetResult(context_list[2]->result);
  ReadAndVerifyTransaction(context_list[2]->trans.get(), kETagGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we can delete the HttpCache while creating the backend.
TEST_F(HttpCacheTest, DeleteCacheWaitingForBackend) {
  auto factory = std::make_unique<MockBlockingBackendFactory>();
  MockBlockingBackendFactory* factory_ptr = factory.get();
  auto cache = std::make_unique<MockHttpCache>(std::move(factory));

  MockHttpRequest request(kSimpleGET_Transaction);

  auto c = std::make_unique<Context>();
  c->result = cache->CreateTransaction(&c->trans);
  ASSERT_THAT(c->result, IsOk());

  c->trans->Start(&request, c->callback.callback(), NetLogWithSource());

  // Just to make sure that everything is still pending.
  base::RunLoop().RunUntilIdle();

  // The request should be creating the disk cache.
  EXPECT_FALSE(c->callback.have_result());

  // Manually arrange for completion to happen after ~HttpCache.
  // This can't be done via FinishCreation() since that's in `factory`, and
  // that's owned by `cache`.
  disk_cache::BackendResultCallback callback = factory_ptr->ReleaseCallback();

  cache.reset();
  base::RunLoop().RunUntilIdle();

  // Simulate the backend completion callback running now the HttpCache is gone.
  std::move(callback).Run(disk_cache::BackendResult::MakeError(ERR_ABORTED));
}

// Tests that we can delete the cache while creating the backend, from within
// one of the callbacks.
TEST_F(HttpCacheTest, DeleteCacheWaitingForBackend2) {
  auto factory = std::make_unique<MockBlockingBackendFactory>();
  MockBlockingBackendFactory* factory_ptr = factory.get();
  auto cache = std::make_unique<MockHttpCache>(std::move(factory));
  auto* cache_ptr = cache.get();

  DeleteCacheCompletionCallback cb(std::move(cache));
  auto [rv, _] = cache_ptr->http_cache()->GetBackend(cb.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Now let's queue a regular transaction
  MockHttpRequest request(kSimpleGET_Transaction);

  auto c = std::make_unique<Context>();
  c->result = cache_ptr->CreateTransaction(&c->trans);
  ASSERT_THAT(c->result, IsOk());

  c->trans->Start(&request, c->callback.callback(), NetLogWithSource());

  // And another direct backend request.
  TestGetBackendCompletionCallback cb2;
  auto [rv2, _2] = cache_ptr->http_cache()->GetBackend(cb2.callback());
  EXPECT_THAT(rv2, IsError(ERR_IO_PENDING));

  // Just to make sure that everything is still pending.
  base::RunLoop().RunUntilIdle();

  // The request should be queued.
  EXPECT_FALSE(c->callback.have_result());

  // Generate the callback.
  factory_ptr->FinishCreation();
  cb.WaitForResult();

  // The cache should be gone by now.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(c->callback.GetResult(c->result), IsOk());
  EXPECT_FALSE(cb2.have_result());
}

TEST_F(HttpCacheTest, TypicalGetConditionalRequest) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kTypicalGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but this time we expect it to result
  // in a conditional request.
  LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), kTypicalGET_Transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

static const auto kETagGetConditionalRequestHandler =
    base::BindRepeating([](const HttpRequestInfo* request,
                           std::string* response_status,
                           std::string* response_headers,
                           std::string* response_data) {
      EXPECT_TRUE(
          request->extra_headers.HasHeader(HttpRequestHeaders::kIfNoneMatch));
      response_status->assign("HTTP/1.1 304 Not Modified");
      response_headers->assign(kETagGET_Transaction.response_headers);
      response_data->clear();
    });

using HttpCacheETagGetTest = HttpCacheTest;

TEST_F(HttpCacheETagGetTest, ConditionalRequest304) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but this time we expect it to result
  // in a conditional request.
  transaction.load_flags = LOAD_VALIDATE_CACHE;
  transaction.handler = kETagGetConditionalRequestHandler;
  LoadTimingInfo load_timing_info;
  IPEndPoint remote_endpoint;
  RunTransactionTestAndGetTimingAndConnectedSocketAddress(
      cache.http_cache(), transaction,
      NetLogWithSource::Make(NetLogSourceType::NONE), &load_timing_info,
      &remote_endpoint);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  EXPECT_FALSE(remote_endpoint.address().empty());
}

class RevalidationServer {
 public:
  RevalidationServer() = default;

  bool EtagUsed() { return etag_used_; }
  bool LastModifiedUsed() { return last_modified_used_; }

  MockTransactionHandler GetHandlerCallback() {
    return base::BindLambdaForTesting([this](const HttpRequestInfo* request,
                                             std::string* response_status,
                                             std::string* response_headers,
                                             std::string* response_data) {
      if (request->extra_headers.HasHeader(HttpRequestHeaders::kIfNoneMatch)) {
        etag_used_ = true;
      }

      if (request->extra_headers.HasHeader(
              HttpRequestHeaders::kIfModifiedSince)) {
        last_modified_used_ = true;
      }

      if (etag_used_ || last_modified_used_) {
        response_status->assign("HTTP/1.1 304 Not Modified");
        response_headers->assign(kTypicalGET_Transaction.response_headers);
        response_data->clear();
      } else {
        response_status->assign(kTypicalGET_Transaction.status);
        response_headers->assign(kTypicalGET_Transaction.response_headers);
        response_data->assign(kTypicalGET_Transaction.data);
      }
    });
  }

 private:
  bool etag_used_ = false;
  bool last_modified_used_ = false;
};

using HttpCacheGetTest = HttpCacheTest;

// Tests revalidation after a vary match.
TEST_F(HttpCacheGetTest, ValidateCacheVaryMatch) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Read from the cache.
  RevalidationServer server;
  transaction.handler = server.GetHandlerCallback();
  LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  EXPECT_TRUE(server.EtagUsed());
  EXPECT_TRUE(server.LastModifiedUsed());
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests revalidation after a vary mismatch if etag is present.
TEST_F(HttpCacheGetTest, ValidateCacheVaryMismatch) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Read from the cache and revalidate the entry.
  RevalidationServer server;
  transaction.handler = server.GetHandlerCallback();
  transaction.request_headers = "Foo: none\r\n";
  LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  EXPECT_TRUE(server.EtagUsed());
  EXPECT_FALSE(server.LastModifiedUsed());
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests revalidation after a vary mismatch due to vary: * if etag is present.
TEST_F(HttpCacheGetTest, ValidateCacheVaryMismatchStar) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=0\n"
      "Vary: *\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Read from the cache and revalidate the entry.
  RevalidationServer server;
  transaction.handler = server.GetHandlerCallback();
  LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  EXPECT_TRUE(server.EtagUsed());
  EXPECT_FALSE(server.LastModifiedUsed());
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests lack of revalidation after a vary mismatch and no etag.
TEST_F(HttpCacheGetTest, DontValidateCacheVaryMismatch) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Read from the cache and don't revalidate the entry.
  RevalidationServer server;
  transaction.handler = server.GetHandlerCallback();
  transaction.request_headers = "Foo: none\r\n";
  LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction,
                                 NetLogWithSource::Make(NetLogSourceType::NONE),
                                 &load_timing_info);

  EXPECT_FALSE(server.EtagUsed());
  EXPECT_FALSE(server.LastModifiedUsed());
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests that a new vary header provided when revalidating an entry is saved.
TEST_F(HttpCacheGetTest, ValidateCacheVaryMatchUpdateVary) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n Name: bar\r\n";
  transaction.response_headers =
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Validate the entry and change the vary field in the response.
  transaction.request_headers = "Foo: bar\r\n Name: none\r\n";
  transaction.status = "HTTP/1.1 304 Not Modified";
  transaction.response_headers =
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=3600\n"
      "Vary: Name\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the ActiveEntry is gone.
  base::RunLoop().RunUntilIdle();

  // Generate a vary mismatch.
  transaction.request_headers = "Foo: bar\r\n Name: bar\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that new request headers causing a vary mismatch are paired with the
// new response when the server says the old response can be used.
TEST_F(HttpCacheGetTest, ValidateCacheVaryMismatchUpdateRequestHeader) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=3600\n"
      "Vary: Foo\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Vary-mismatch validation receives 304.
  transaction.request_headers = "Foo: none\r\n";
  transaction.status = "HTTP/1.1 304 Not Modified";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the ActiveEntry is gone.
  base::RunLoop().RunUntilIdle();

  // Generate a vary mismatch.
  transaction.request_headers = "Foo: bar\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that a 304 without vary headers doesn't delete the previously stored
// vary data after a vary match revalidation.
TEST_F(HttpCacheGetTest, ValidateCacheVaryMatchDontDeleteVary) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Validate the entry and remove the vary field in the response.
  transaction.status = "HTTP/1.1 304 Not Modified";
  transaction.response_headers =
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=3600\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the ActiveEntry is gone.
  base::RunLoop().RunUntilIdle();

  // Generate a vary mismatch.
  transaction.request_headers = "Foo: none\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that a 304 without vary headers doesn't delete the previously stored
// vary data after a vary mismatch.
TEST_F(HttpCacheGetTest, ValidateCacheVaryMismatchDontDeleteVary) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=3600\n"
      "Vary: Foo\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Vary-mismatch validation receives 304 and no vary header.
  transaction.request_headers = "Foo: none\r\n";
  transaction.status = "HTTP/1.1 304 Not Modified";
  transaction.response_headers =
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=3600\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the ActiveEntry is gone.
  base::RunLoop().RunUntilIdle();

  // Generate a vary mismatch.
  transaction.request_headers = "Foo: bar\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

static void ETagGet_UnconditionalRequest_Handler(const HttpRequestInfo* request,
                                                 std::string* response_status,
                                                 std::string* response_headers,
                                                 std::string* response_data) {
  EXPECT_FALSE(
      request->extra_headers.HasHeader(HttpRequestHeaders::kIfNoneMatch));
}

TEST_F(HttpCacheETagGetTest, Http10) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);
  transaction.status = "HTTP/1.0 200 OK";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, without generating a conditional request.
  transaction.load_flags = LOAD_VALIDATE_CACHE;
  transaction.handler =
      base::BindRepeating(&ETagGet_UnconditionalRequest_Handler);
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheETagGetTest, Http10Range) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);
  transaction.status = "HTTP/1.0 200 OK";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but use a byte range request.
  transaction.load_flags = LOAD_VALIDATE_CACHE;
  transaction.handler =
      base::BindRepeating(&ETagGet_UnconditionalRequest_Handler);
  transaction.request_headers = "Range: bytes = 5-\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

static void ETagGet_ConditionalRequest_NoStore_Handler(
    const HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(
      request->extra_headers.HasHeader(HttpRequestHeaders::kIfNoneMatch));
  response_status->assign("HTTP/1.1 304 Not Modified");
  response_headers->assign("Cache-Control: no-store\n");
  response_data->clear();
}

TEST_F(HttpCacheETagGetTest, ConditionalRequest304NoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but this time we expect it to result
  // in a conditional request.
  transaction.load_flags = LOAD_VALIDATE_CACHE;
  transaction.handler =
      base::BindRepeating(&ETagGet_ConditionalRequest_NoStore_Handler);
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Reset transaction
  transaction.load_flags = kETagGET_Transaction.load_flags;
  transaction.handler = kETagGET_Transaction.handler;

  // Write to the cache again. This should create a new entry.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Helper that does 4 requests using HttpCache:
//
// (1) loads |kUrl| -- expects |net_response_1| to be returned.
// (2) loads |kUrl| from cache only -- expects |net_response_1| to be returned.
// (3) loads |kUrl| using |extra_request_headers| -- expects |net_response_2| to
//     be returned.
// (4) loads |kUrl| from cache only -- expects |cached_response_2| to be
//     returned.
// The entry will be created once and will be opened for the 3 subsequent
// requests.
static void ConditionalizedRequestUpdatesCacheHelper(
    const Response& net_response_1,
    const Response& net_response_2,
    const Response& cached_response_2,
    const char* extra_request_headers) {
  MockHttpCache cache;

  // The URL we will be requesting.
  const char kUrl[] = "http://foobar.com/main.css";

  // Junk network response.
  static const Response kUnexpectedResponse = {"HTTP/1.1 500 Unexpected",
                                               "Server: unexpected_header",
                                               "unexpected body"};

  // We will control the network layer's responses for |kUrl| using
  // |mock_network_response|.
  ScopedMockTransaction mock_network_response(kUrl);

  // Request |kUrl| for the first time. It should hit the network and
  // receive |kNetResponse1|, which it saves into the HTTP cache.

  MockTransaction request = {nullptr};
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = "";

  net_response_1.AssignTo(&mock_network_response);  // Network mock.
  net_response_1.AssignTo(&request);                // Expected result.

  std::string response_headers;
  RunTransactionTestWithResponse(cache.http_cache(), request,
                                 &response_headers);

  EXPECT_EQ(net_response_1.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Request |kUrl| a second time. Now |kNetResponse1| it is in the HTTP
  // cache, so we don't hit the network.

  request.load_flags = LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;

  kUnexpectedResponse.AssignTo(&mock_network_response);  // Network mock.
  net_response_1.AssignTo(&request);                     // Expected result.

  RunTransactionTestWithResponse(cache.http_cache(), request,
                                 &response_headers);

  EXPECT_EQ(net_response_1.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Request |kUrl| yet again, but this time give the request an
  // "If-Modified-Since" header. This will cause the request to re-hit the
  // network. However now the network response is going to be
  // different -- this simulates a change made to the CSS file.

  request.request_headers = extra_request_headers;
  request.load_flags = LOAD_NORMAL;

  net_response_2.AssignTo(&mock_network_response);  // Network mock.
  net_response_2.AssignTo(&request);                // Expected result.

  RunTransactionTestWithResponse(cache.http_cache(), request,
                                 &response_headers);

  EXPECT_EQ(net_response_2.status_and_headers(), response_headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Finally, request |kUrl| again. This request should be serviced from
  // the cache. Moreover, the value in the cache should be |kNetResponse2|
  // and NOT |kNetResponse1|. The previous step should have replaced the
  // value in the cache with the modified response.

  request.request_headers = "";
  request.load_flags = LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;

  kUnexpectedResponse.AssignTo(&mock_network_response);  // Network mock.
  cached_response_2.AssignTo(&request);                  // Expected result.

  RunTransactionTestWithResponse(cache.http_cache(), request,
                                 &response_headers);

  EXPECT_EQ(cached_response_2.status_and_headers(), response_headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Check that when an "if-modified-since" header is attached
// to the request, the result still updates the cached entry.
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache1) {
  // First network response for |kUrl|.
  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      "body1"};

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
      "HTTP/1.1 200 OK",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
      "body2"};

  const char extra_headers[] =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n";

  ConditionalizedRequestUpdatesCacheHelper(kNetResponse1, kNetResponse2,
                                           kNetResponse2, extra_headers);
}

// Check that when an "if-none-match" header is attached
// to the request, the result updates the cached entry.
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache2) {
  // First network response for |kUrl|.
  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Etag: \"ETAG1\"\n"
      "Expires: Wed, 7 Sep 2033 21:46:42 GMT\n",  // Should never expire.
      "body1"};

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
      "HTTP/1.1 200 OK",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Etag: \"ETAG2\"\n"
      "Expires: Wed, 7 Sep 2033 21:46:42 GMT\n",  // Should never expire.
      "body2"};

  const char extra_headers[] = "If-None-Match: \"ETAG1\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(kNetResponse1, kNetResponse2,
                                           kNetResponse2, extra_headers);
}

// Check that when an "if-modified-since" header is attached
// to a request, the 304 (not modified result) result updates the cached
// headers, and the 304 response is returned rather than the cached response.
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache3) {
  // First network response for |kUrl|.
  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Server: server1\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      "body1"};

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
      "HTTP/1.1 304 Not Modified",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Server: server2\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      ""};

  static const Response kCachedResponse2 = {
      "HTTP/1.1 200 OK",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Server: server2\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      "body1"};

  const char extra_headers[] =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n";

  ConditionalizedRequestUpdatesCacheHelper(kNetResponse1, kNetResponse2,
                                           kCachedResponse2, extra_headers);
}

// Test that when doing an externally conditionalized if-modified-since
// and there is no corresponding cache entry, a new cache entry is NOT
// created (304 response).
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache4) {
  MockHttpCache cache;

  const char kUrl[] = "http://foobar.com/main.css";

  static const Response kNetResponse = {
      "HTTP/1.1 304 Not Modified",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      ""};

  const char kExtraRequestHeaders[] =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n";

  // We will control the network layer's responses for |kUrl| using
  // |mock_network_response|.
  ScopedMockTransaction mock_network_response(kUrl);

  MockTransaction request = {nullptr};
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = kExtraRequestHeaders;

  kNetResponse.AssignTo(&mock_network_response);  // Network mock.
  kNetResponse.AssignTo(&request);                // Expected result.

  std::string response_headers;
  RunTransactionTestWithResponse(cache.http_cache(), request,
                                 &response_headers);

  EXPECT_EQ(kNetResponse.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Test that when doing an externally conditionalized if-modified-since
// and there is no corresponding cache entry, a new cache entry is NOT
// created (200 response).
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache5) {
  MockHttpCache cache;

  const char kUrl[] = "http://foobar.com/main.css";

  static const Response kNetResponse = {
      "HTTP/1.1 200 OK",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      "foobar!!!"};

  const char kExtraRequestHeaders[] =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n";

  // We will control the network layer's responses for |kUrl| using
  // |mock_network_response|.
  ScopedMockTransaction mock_network_response(kUrl);

  MockTransaction request = {nullptr};
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = kExtraRequestHeaders;

  kNetResponse.AssignTo(&mock_network_response);  // Network mock.
  kNetResponse.AssignTo(&request);                // Expected result.

  std::string response_headers;
  RunTransactionTestWithResponse(cache.http_cache(), request,
                                 &response_headers);

  EXPECT_EQ(kNetResponse.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Test that when doing an externally conditionalized if-modified-since
// if the date does not match the cache entry's last-modified date,
// then we do NOT use the response (304) to update the cache.
// (the if-modified-since date is 2 days AFTER the cache's modification date).
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache6) {
  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Server: server1\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      "body1"};

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
      "HTTP/1.1 304 Not Modified",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Server: server2\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      ""};

  // This is two days in the future from the original response's last-modified
  // date!
  const char kExtraRequestHeaders[] =
      "If-Modified-Since: Fri, 08 Feb 2008 22:38:21 GMT\r\n";

  ConditionalizedRequestUpdatesCacheHelper(kNetResponse1, kNetResponse2,
                                           kNetResponse1, kExtraRequestHeaders);
}

// Test that when doing an externally conditionalized if-none-match
// if the etag does not match the cache entry's etag, then we do not use the
// response (304) to update the cache.
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache7) {
  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Etag: \"Foo1\"\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      "body1"};

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
      "HTTP/1.1 304 Not Modified",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Etag: \"Foo2\"\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      ""};

  // Different etag from original response.
  const char kExtraRequestHeaders[] = "If-None-Match: \"Foo2\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(kNetResponse1, kNetResponse2,
                                           kNetResponse1, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with both if-none-match
// and if-modified-since updates the cache.
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache8) {
  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Etag: \"Foo1\"\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      "body1"};

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
      "HTTP/1.1 200 OK",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Etag: \"Foo2\"\n"
      "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
      "body2"};

  const char kExtraRequestHeaders[] =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n"
      "If-None-Match: \"Foo1\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(kNetResponse1, kNetResponse2,
                                           kNetResponse2, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with both if-none-match
// and if-modified-since does not update the cache with only one match.
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache9) {
  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Etag: \"Foo1\"\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      "body1"};

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
      "HTTP/1.1 200 OK",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Etag: \"Foo2\"\n"
      "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
      "body2"};

  // The etag doesn't match what we have stored.
  const char kExtraRequestHeaders[] =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n"
      "If-None-Match: \"Foo2\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(kNetResponse1, kNetResponse2,
                                           kNetResponse1, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with both if-none-match
// and if-modified-since does not update the cache with only one match.
TEST_F(HttpCacheTest, ConditionalizedRequestUpdatesCache10) {
  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Etag: \"Foo1\"\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      "body1"};

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
      "HTTP/1.1 200 OK",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Etag: \"Foo2\"\n"
      "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
      "body2"};

  // The modification date doesn't match what we have stored.
  const char kExtraRequestHeaders[] =
      "If-Modified-Since: Fri, 08 Feb 2008 22:38:21 GMT\r\n"
      "If-None-Match: \"Foo1\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(kNetResponse1, kNetResponse2,
                                           kNetResponse1, kExtraRequestHeaders);
}

TEST_F(HttpCacheTest, UrlContainingHash) {
  MockHttpCache cache;

  // Do a typical GET request -- should write an entry into our cache.
  MockTransaction trans(kTypicalGET_Transaction);
  RunTransactionTest(cache.http_cache(), trans);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Request the same URL, but this time with a reference section (hash).
  // Since the cache key strips the hash sections, this should be a cache hit.
  std::string url_with_hash = std::string(trans.url) + "#multiple#hashes";
  trans.url = url_with_hash.c_str();
  trans.load_flags = LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;

  RunTransactionTest(cache.http_cache(), trans);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we skip the cache for POST requests that do not have an upload
// identifier.
TEST_F(HttpCacheSimplePostTest, SkipsCache) {
  MockHttpCache cache;

  RunTransactionTest(cache.http_cache(), kSimplePOST_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests POST handling with a disabled cache (no DCHECK).
TEST_F(HttpCacheSimplePostTest, DisabledCache) {
  MockHttpCache cache;
  cache.http_cache()->set_mode(HttpCache::Mode::DISABLE);

  RunTransactionTest(cache.http_cache(), kSimplePOST_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheSimplePostTest, LoadOnlyFromCacheMiss) {
  MockHttpCache cache;

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
  ASSERT_THAT(callback.GetResult(rv), IsError(ERR_CACHE_MISS));

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

using HttpCacheSimplePostTest = HttpCacheTest;

TEST_F(HttpCacheSimplePostTest, LoadOnlyFromCacheHit) {
  MockHttpCache cache;

  // Test that we hit the cache for POST requests.

  MockTransaction transaction(kSimplePOST_Transaction);

  const int64_t kUploadId = 1;  // Just a dummy value.

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers),
                                              kUploadId);
  MockHttpRequest request(transaction);
  request.upload_data_stream = &upload_data_stream;

  // Populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Load from cache.
  request.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Test that we don't hit the cache for POST requests if there is a byte range.
TEST_F(HttpCacheSimplePostTest, WithRanges) {
  MockHttpCache cache;

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.request_headers = "Range: bytes = 0-4\r\n";

  const int64_t kUploadId = 1;  // Just a dummy value.

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers),
                                              kUploadId);

  MockHttpRequest request(transaction);
  request.upload_data_stream = &upload_data_stream;

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that a POST is cached separately from a GET.
TEST_F(HttpCacheSimplePostTest, SeparateCache) {
  MockHttpCache cache;

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 1);

  MockTransaction transaction(kSimplePOST_Transaction);
  MockHttpRequest req1(transaction);
  req1.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "GET";
  MockHttpRequest req2(transaction);

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that a successful POST invalidates a previously cached GET.
TEST_F(HttpCacheSimplePostTest, Invalidate205) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 1);

  transaction.method = "POST";
  transaction.status = "HTTP/1.1 205 No Content";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());
}

// Tests that a successful POST invalidates a previously cached GET,
// with cache split by top-frame origin.
TEST_F(HttpCacheTestSplitCacheFeatureEnabled,
       SimplePostInvalidate205SplitCache) {
  SchemefulSite site_a(GURL("http://a.com"));
  SchemefulSite site_b(GURL("http://b.com"));

  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);
  req1.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  req1.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  // Same for a different origin.
  MockHttpRequest req1b(transaction);
  req1b.network_isolation_key = NetworkIsolationKey(site_b, site_b);
  req1b.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_b);
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1b,
                                nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 1);

  transaction.method = "POST";
  transaction.status = "HTTP/1.1 205 No Content";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;
  req2.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  req2.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());

  // req1b should still be cached, since it has a different top-level frame
  // origin.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1b,
                                nullptr);
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());

  // req1 should not be cached after the POST.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(4, cache.disk_cache()->create_count());
}

// Tests that a successful POST invalidates a previously cached GET, even when
// there is no upload identifier.
TEST_F(HttpCacheSimplePostTest, NoUploadIdInvalidate205) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  transaction.method = "POST";
  transaction.status = "HTTP/1.1 205 No Content";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that processing a POST before creating the backend doesn't crash.
TEST_F(HttpCacheSimplePostTest, NoUploadIdNoBackend) {
  // This will initialize a cache object with NULL backend.
  auto factory = std::make_unique<MockBlockingBackendFactory>();
  factory->set_fail(true);
  factory->FinishCreation();
  MockHttpCache cache(std::move(factory));

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  ScopedMockTransaction transaction(kSimplePOST_Transaction);
  MockHttpRequest req(transaction);
  req.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req, nullptr);
}

// Tests that we don't invalidate entries as a result of a failed POST.
TEST_F(HttpCacheSimplePostTest, DontInvalidate100) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 1);

  transaction.method = "POST";
  transaction.status = "HTTP/1.1 100 Continue";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

using HttpCacheSimpleHeadTest = HttpCacheTest;

// Tests that a HEAD request is not cached by itself.
TEST_F(HttpCacheSimpleHeadTest, LoadOnlyFromCacheMiss) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kSimplePOST_Transaction);
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  transaction.method = "HEAD";

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
  ASSERT_THAT(callback.GetResult(rv), IsError(ERR_CACHE_MISS));

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that a HEAD request is served from a cached GET.
TEST_F(HttpCacheSimpleHeadTest, LoadOnlyFromCacheHit) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);

  // Populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Load from cache.
  transaction.method = "HEAD";
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  transaction.data = "";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that a read-only request served from the cache preserves CL.
TEST_F(HttpCacheSimpleHeadTest, ContentLengthOnHitRead) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "Content-Length: 42\n";

  // Populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  // Load from cache.
  transaction.method = "HEAD";
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  transaction.data = "";
  std::string headers;

  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ("HTTP/1.1 200 OK\nContent-Length: 42\n", headers);
}

// Tests that a read-write request served from the cache preserves CL.
TEST_F(HttpCacheTest, ETagHeadContentLengthOnHitReadWrite) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kETagGET_Transaction);
  std::string server_headers(kETagGET_Transaction.response_headers);
  server_headers.append("Content-Length: 42\n");
  transaction.response_headers = server_headers.data();

  // Populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  // Load from cache.
  transaction.method = "HEAD";
  transaction.data = "";
  std::string headers;

  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_NE(std::string::npos, headers.find("Content-Length: 42\n"));
}

// Tests that a HEAD request that includes byte ranges bypasses the cache.
TEST_F(HttpCacheSimpleHeadTest, WithRanges) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);

  // Populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  // Load from cache.
  transaction.method = "HEAD";
  transaction.request_headers = "Range: bytes = 0-4\r\n";
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  transaction.start_return_code = ERR_CACHE_MISS;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that a HEAD request can be served from a partially cached resource.
TEST_F(HttpCacheSimpleHeadTest, WithCachedRanges) {
  MockHttpCache cache;
  {
    ScopedMockTransaction scoped_mock_transaction(kRangeGET_TransactionOK);
    // Write to the cache (40-49).
    RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  }

  ScopedMockTransaction transaction(kSimpleGET_Transaction,
                                    kRangeGET_TransactionOK.url);
  transaction.method = "HEAD";
  transaction.data = "";
  std::string headers;

  // Load from cache.
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_NE(std::string::npos, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_NE(std::string::npos, headers.find("Content-Length: 80\n"));
  EXPECT_EQ(std::string::npos, headers.find("Content-Range"));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that a HEAD request can be served from a truncated resource.
TEST_F(HttpCacheSimpleHeadTest, WithTruncatedEntry) {
  MockHttpCache cache;
  {
    ScopedMockTransaction scoped_mock_transaction(kRangeGET_TransactionOK);
    std::string raw_headers(
        "HTTP/1.1 200 OK\n"
        "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
        "ETag: \"foo\"\n"
        "Accept-Ranges: bytes\n"
        "Content-Length: 80\n");
    CreateTruncatedEntry(raw_headers, &cache);
  }

  ScopedMockTransaction transaction(kSimpleGET_Transaction,
                                    kRangeGET_TransactionOK.url);
  transaction.method = "HEAD";
  transaction.data = "";
  std::string headers;

  // Load from cache.
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_NE(std::string::npos, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_NE(std::string::npos, headers.find("Content-Length: 80\n"));
  EXPECT_EQ(std::string::npos, headers.find("Content-Range"));
  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

using HttpCacheTypicalHeadTest = HttpCacheTest;

// Tests that a HEAD request updates the cached response.
TEST_F(HttpCacheTypicalHeadTest, UpdatesResponse) {
  MockHttpCache cache;
  std::string headers;
  {
    ScopedMockTransaction transaction(kTypicalGET_Transaction);

    // Populate the cache.
    RunTransactionTest(cache.http_cache(), transaction);

    // Update the cache.
    transaction.method = "HEAD";
    transaction.response_headers = "Foo: bar\n";
    transaction.data = "";
    transaction.status = "HTTP/1.1 304 Not Modified\n";
    RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);
  }

  EXPECT_NE(std::string::npos, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  ScopedMockTransaction transaction2(kTypicalGET_Transaction);

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Load from the cache.
  transaction2.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);

  EXPECT_NE(std::string::npos, headers.find("Foo: bar\n"));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that an externally conditionalized HEAD request updates the cache.
TEST_F(HttpCacheTypicalHeadTest, ConditionalizedRequestUpdatesResponse) {
  MockHttpCache cache;
  std::string headers;

  {
    ScopedMockTransaction transaction(kTypicalGET_Transaction);

    // Populate the cache.
    RunTransactionTest(cache.http_cache(), transaction);

    // Update the cache.
    transaction.method = "HEAD";
    transaction.request_headers =
        "If-Modified-Since: Wed, 28 Nov 2007 00:40:09 GMT\r\n";
    transaction.response_headers = "Foo: bar\n";
    transaction.data = "";
    transaction.status = "HTTP/1.1 304 Not Modified\n";
    RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

    EXPECT_NE(std::string::npos, headers.find("HTTP/1.1 304 Not Modified\n"));
    EXPECT_EQ(2, cache.network_layer()->transaction_count());

    // Make sure we are done with the previous transaction.
    base::RunLoop().RunUntilIdle();
  }
  {
    ScopedMockTransaction transaction2(kTypicalGET_Transaction);

    // Load from the cache.
    transaction2.load_flags |=
        LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
    RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);

    EXPECT_NE(std::string::npos, headers.find("Foo: bar\n"));
    EXPECT_EQ(2, cache.network_layer()->transaction_count());
    EXPECT_EQ(2, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }
}

// Tests that a HEAD request invalidates an old cached entry.
TEST_F(HttpCacheSimpleHeadTest, InvalidatesEntry) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kTypicalGET_Transaction);

  // Populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  // Update the cache.
  transaction.method = "HEAD";
  transaction.data = "";
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  // Load from the cache.
  transaction.method = "GET";
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  transaction.start_return_code = ERR_CACHE_MISS;
  RunTransactionTest(cache.http_cache(), transaction);
}

using HttpCacheSimplePutTest = HttpCacheTest;

// Tests that we do not cache the response of a PUT.
TEST_F(HttpCacheSimplePutTest, Miss) {
  MockHttpCache cache;

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.method = "PUT";

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  MockHttpRequest request(transaction);
  request.upload_data_stream = &upload_data_stream;

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that we invalidate entries as a result of a PUT.
TEST_F(HttpCacheSimplePutTest, Invalidate) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  transaction.method = "PUT";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we invalidate entries as a result of a PUT.
TEST_F(HttpCacheSimplePutTest, Invalidate305) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  transaction.method = "PUT";
  transaction.status = "HTTP/1.1 305 Use Proxy";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we don't invalidate entries as a result of a failed PUT.
TEST_F(HttpCacheSimplePutTest, DontInvalidate404) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  transaction.method = "PUT";
  transaction.status = "HTTP/1.1 404 Not Found";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

using HttpCacheSimpleDeleteTest = HttpCacheTest;

// Tests that we do not cache the response of a DELETE.
TEST_F(HttpCacheSimpleDeleteTest, Miss) {
  MockHttpCache cache;

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.method = "DELETE";

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  MockHttpRequest request(transaction);
  request.upload_data_stream = &upload_data_stream;

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request,
                                nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that we invalidate entries as a result of a DELETE.
TEST_F(HttpCacheSimpleDeleteTest, Invalidate) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  transaction.method = "DELETE";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we invalidate entries as a result of a DELETE.
TEST_F(HttpCacheSimpleDeleteTest, Invalidate301) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);

  // Attempt to populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "DELETE";
  transaction.status = "HTTP/1.1 301 Moved Permanently ";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "GET";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we don't invalidate entries as a result of a failed DELETE.
TEST_F(HttpCacheSimpleDeleteTest, DontInvalidate416) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);

  // Attempt to populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "DELETE";
  transaction.status = "HTTP/1.1 416 Requested Range Not Satisfiable";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "GET";
  transaction.status = "HTTP/1.1 200 OK";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

using HttpCacheSimplePatchTest = HttpCacheTest;

// Tests that we invalidate entries as a result of a PATCH.
TEST_F(HttpCacheSimplePatchTest, Invalidate) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);

  transaction.method = "PATCH";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, nullptr);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, nullptr);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we invalidate entries as a result of a PATCH.
TEST_F(HttpCacheSimplePatchTest, Invalidate301) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);

  // Attempt to populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "PATCH";
  transaction.status = "HTTP/1.1 301 Moved Permanently ";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "GET";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we don't invalidate entries as a result of a failed PATCH.
TEST_F(HttpCacheSimplePatchTest, DontInvalidate416) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);

  // Attempt to populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "PATCH";
  transaction.status = "HTTP/1.1 416 Requested Range Not Satisfiable";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "GET";
  transaction.status = "HTTP/1.1 200 OK";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we don't invalidate entries after a failed network transaction.
TEST_F(HttpCacheSimpleGetTest, DontInvalidateOnFailure) {
  MockHttpCache cache;

  // Populate the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Fail the network request.
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.start_return_code = ERR_FAILED;
  transaction.load_flags |= LOAD_VALIDATE_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  transaction.load_flags = LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  transaction.start_return_code = OK;
  RunTransactionTest(cache.http_cache(), transaction);

  // Make sure the transaction didn't reach the network.
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
}

TEST_F(HttpCacheRangeGetTest, SkipsCache) {
  MockHttpCache cache;

  // Test that we skip the cache for range GET requests.  Eventually, we will
  // want to cache these, but we'll still have cases where skipping the cache
  // makes sense, so we want to make sure that it works properly.

  RunTransactionTest(cache.http_cache(), kRangeGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "If-None-Match: foo\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers =
      "If-Modified-Since: Wed, 28 Nov 2007 00:45:20 GMT\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Test that we skip the cache for range requests that include a validation
// header.
TEST_F(HttpCacheRangeGetTest, SkipsCache2) {
  MockHttpCache cache;

  MockTransaction transaction(kRangeGET_Transaction);
  transaction.request_headers =
      "If-None-Match: foo\r\n" EXTRA_HEADER "Range: bytes = 40-49\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers =
      "If-Modified-Since: Wed, 28 Nov 2007 00:45:20 GMT\r\n" EXTRA_HEADER
      "Range: bytes = 40-49\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers =
      "If-Range: bla\r\n" EXTRA_HEADER "Range: bytes = 40-49\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheSimpleGetTest, DoesntLogHeaders) {
  MockHttpCache cache;

  RecordingNetLogObserver net_log_observer;
  RunTransactionTestWithLog(cache.http_cache(), kSimpleGET_Transaction,
                            NetLogWithSource::Make(NetLogSourceType::NONE));

  EXPECT_FALSE(LogContainsEventType(
      net_log_observer, NetLogEventType::HTTP_CACHE_CALLER_REQUEST_HEADERS));
}

TEST_F(HttpCacheRangeGetTest, LogsHeaders) {
  MockHttpCache cache;

  RecordingNetLogObserver net_log_observer;
  RunTransactionTestWithLog(cache.http_cache(), kRangeGET_Transaction,
                            NetLogWithSource::Make(NetLogSourceType::NONE));

  EXPECT_TRUE(LogContainsEventType(
      net_log_observer, NetLogEventType::HTTP_CACHE_CALLER_REQUEST_HEADERS));
}

TEST_F(HttpCacheTest, ExternalValidationLogsHeaders) {
  MockHttpCache cache;

  RecordingNetLogObserver net_log_observer;
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "If-None-Match: foo\r\n" EXTRA_HEADER;
  RunTransactionTestWithLog(cache.http_cache(), transaction,
                            NetLogWithSource::Make(NetLogSourceType::NONE));

  EXPECT_TRUE(LogContainsEventType(
      net_log_observer, NetLogEventType::HTTP_CACHE_CALLER_REQUEST_HEADERS));
}

TEST_F(HttpCacheTest, SpecialHeadersLogsHeaders) {
  MockHttpCache cache;

  RecordingNetLogObserver net_log_observer;
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: no-cache\r\n" EXTRA_HEADER;
  RunTransactionTestWithLog(cache.http_cache(), transaction,
                            NetLogWithSource::Make(NetLogSourceType::NONE));

  EXPECT_TRUE(LogContainsEventType(
      net_log_observer, NetLogEventType::HTTP_CACHE_CALLER_REQUEST_HEADERS));
}

// Tests that receiving 206 for a regular request is handled correctly.
TEST_F(HttpCacheGetTest, Crazy206) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.handler = MockTransactionHandler();
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This should read again from the net.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that receiving 416 for a regular request is handled correctly.
TEST_F(HttpCacheGetTest, Crazy416) {
  MockHttpCache cache;

  // Write to the cache.
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.status = "HTTP/1.1 416 Requested Range Not Satisfiable";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we don't store partial responses that can't be validated.
TEST_F(HttpCacheRangeGetTest, NoStrongValidators) {
  MockHttpCache cache;
  std::string headers;

  // Attempt to write to the cache (40-49).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.response_headers =
      "Content-Length: 10\n"
      "Cache-Control: max-age=3600\n"
      "ETag: w/\"foo\"\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now verify that there's no cached data.
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests failures to conditionalize byte range requests.
TEST_F(HttpCacheRangeGetTest, NoConditionalization) {
  MockHttpCache cache;
  cache.FailConditionalizations();
  std::string headers;

  // Write to the cache (40-49).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.response_headers =
      "Content-Length: 10\n"
      "ETag: \"foo\"\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now verify that the cached data is not used.
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that restarting a partial request when the cached data cannot be
// revalidated logs an event.
TEST_F(HttpCacheRangeGetTest, NoValidationLogsRestart) {
  MockHttpCache cache;
  cache.FailConditionalizations();

  // Write to the cache (40-49).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.response_headers =
      "Content-Length: 10\n"
      "ETag: \"foo\"\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Now verify that the cached data is not used.
  RecordingNetLogObserver net_log_observer;
  RunTransactionTestWithLog(cache.http_cache(), kRangeGET_TransactionOK,
                            NetLogWithSource::Make(NetLogSourceType::NONE));

  EXPECT_TRUE(LogContainsEventType(
      net_log_observer, NetLogEventType::HTTP_CACHE_RESTART_PARTIAL_REQUEST));
}

// Tests that a failure to conditionalize a regular request (no range) with a
// sparse entry results in a full response.
TEST_F(HttpCacheGetTest, NoConditionalization) {
  for (bool use_memory_entry_data : {false, true}) {
    MockHttpCache cache;
    cache.disk_cache()->set_support_in_memory_entry_data(use_memory_entry_data);
    cache.FailConditionalizations();
    std::string headers;

    // Write to the cache (40-49).
    ScopedMockTransaction transaction(kRangeGET_TransactionOK);
    transaction.response_headers =
        "Content-Length: 10\n"
        "ETag: \"foo\"\n";
    RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

    Verify206Response(headers, 40, 49);
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());

    // Now verify that the cached data is not used.
    // Don't ask for a range. The cache will attempt to use the cached data but
    // should discard it as it cannot be validated. A regular request should go
    // to the server and a new entry should be created.
    transaction.request_headers = EXTRA_HEADER;
    transaction.data = "Not a range";
    RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

    EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
    EXPECT_EQ(2, cache.network_layer()->transaction_count());
    EXPECT_EQ(1, cache.disk_cache()->open_count());
    EXPECT_EQ(2, cache.disk_cache()->create_count());

    // The last response was saved.
    RunTransactionTest(cache.http_cache(), transaction);
    EXPECT_EQ(3, cache.network_layer()->transaction_count());
    if (use_memory_entry_data) {
      // The cache entry isn't really useful, since when
      // &RangeTransactionServer::RangeHandler gets a non-range request,
      // (the network transaction #2) it returns headers without ETag,
      // Last-Modified or caching headers, with a Date in 2007 (so no heuristic
      // freshness), so it's both expired and not conditionalizable --- so in
      // this branch we avoid opening it.
      EXPECT_EQ(1, cache.disk_cache()->open_count());
      EXPECT_EQ(3, cache.disk_cache()->create_count());
    } else {
      EXPECT_EQ(2, cache.disk_cache()->open_count());
      EXPECT_EQ(2, cache.disk_cache()->create_count());
    }
  }
}

// Verifies that conditionalization failures when asking for a range that would
// require the cache to modify the range to ask, result in a network request
// that matches the user's one.
TEST_F(HttpCacheRangeGetTest, NoConditionalization2) {
  MockHttpCache cache;
  cache.FailConditionalizations();
  std::string headers;

  // Write to the cache (40-49).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.response_headers =
      "Content-Length: 10\n"
      "ETag: \"foo\"\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now verify that the cached data is not used.
  // Ask for a range that extends before and after the cached data so that the
  // cache would normally mix data from three sources. After deleting the entry,
  // the response will come from a single network request.
  transaction.request_headers = "Range: bytes = 20-59\r\n" EXTRA_HEADER;
  transaction.data = "rg: 20-29 rg: 30-39 rg: 40-49 rg: 50-59 ";
  transaction.response_headers = kRangeGET_TransactionOK.response_headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 20, 59);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  // The last response was saved.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we cache partial responses that lack content-length.
TEST_F(HttpCacheRangeGetTest, NoContentLength) {
  MockHttpCache cache;
  std::string headers;

  // Attempt to write to the cache (40-49).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.response_headers =
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Range: bytes 40-49/80\n";
  transaction.handler = MockTransactionHandler();
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now verify that there's no cached data.
  transaction.handler =
      base::BindRepeating(&RangeTransactionServer::RangeHandler);
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we can cache range requests and fetch random blocks from the
// cache and the network.
TEST_F(HttpCacheRangeGetTest, OK) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Write to the cache (30-39).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 30-39\r\n" EXTRA_HEADER;
  transaction.data = "rg: 30-39 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 30, 39);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Write and read from the cache (20-59).
  transaction.request_headers = "Range: bytes = 20-59\r\n" EXTRA_HEADER;
  transaction.data = "rg: 20-29 rg: 30-39 rg: 40-49 rg: 50-59 ";
  LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers,
      NetLogWithSource::Make(NetLogSourceType::NONE), &load_timing_info);

  Verify206Response(headers, 20, 59);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(3, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

TEST_F(HttpCacheRangeGetTest, CacheReadError) {
  // Tests recovery on cache read error on range request.
  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  cache.disk_cache()->set_soft_failures_one_instance(MockDiskEntry::FAIL_ALL);

  // Try to read from the cache (40-49), which will fail quickly enough to
  // restart, due to the failure injected above.  This should still be a range
  // request. (https://crbug.com/891212)
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that range requests with no-store get correct content-length
// (https://crbug.com/700197).
TEST_F(HttpCacheRangeGetTest, NoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  std::string response_headers = base::StrCat(
      {kRangeGET_TransactionOK.response_headers, "Cache-Control: no-store\n"});
  transaction.response_headers = response_headers.c_str();

  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests a 304 setting no-store on existing 206 entry.
TEST_F(HttpCacheRangeGetTest, NoStore304) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  std::string response_headers = base::StrCat(
      {kRangeGET_TransactionOK.response_headers, "Cache-Control: max-age=0\n"});
  transaction.response_headers = response_headers.c_str();

  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  response_headers = base::StrCat(
      {kRangeGET_TransactionOK.response_headers, "Cache-Control: no-store\n"});
  transaction.response_headers = response_headers.c_str();
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);
  Verify206Response(headers, 40, 49);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Fetch again, this one should be from newly created cache entry, due to
  // earlier no-store.
  transaction.response_headers = kRangeGET_TransactionOK.response_headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  Verify206Response(headers, 40, 49);
}

// Tests that we can cache range requests and fetch random blocks from the
// cache and the network, with synchronous responses.
TEST_F(HttpCacheRangeGetTest, SyncOK) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.test_mode = TEST_MODE_SYNC_ALL;

  // Write to the cache (40-49).
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Write to the cache (30-39).
  transaction.request_headers = "Range: bytes = 30-39\r\n" EXTRA_HEADER;
  transaction.data = "rg: 30-39 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 30, 39);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Write and read from the cache (20-59).
  transaction.request_headers = "Range: bytes = 20-59\r\n" EXTRA_HEADER;
  transaction.data = "rg: 20-29 rg: 30-39 rg: 40-49 rg: 50-59 ";
  LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers,
      NetLogWithSource::Make(NetLogSourceType::NONE), &load_timing_info);

  Verify206Response(headers, 20, 59);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests that if the previous transaction is cancelled while busy (doing sparse
// IO), a new transaction (that reuses that same ActiveEntry) waits until the
// entry is ready again.
TEST_F(HttpCacheTest, SparseWaitForEntry) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  // Create a sparse entry.
  RunTransactionTest(cache.http_cache(), transaction);

  // Simulate a previous transaction being cancelled.
  disk_cache::Entry* entry;
  MockHttpRequest request(transaction);
  std::string cache_key = *HttpCache::GenerateCacheKeyForRequest(&request);
  ASSERT_TRUE(cache.OpenBackendEntry(cache_key, &entry));
  entry->CancelSparseIO();

  // Test with a range request.
  RunTransactionTest(cache.http_cache(), transaction);

  // Now test with a regular request.
  entry->CancelSparseIO();
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = kFullRangeData;
  RunTransactionTest(cache.http_cache(), transaction);

  entry->Close();
}

// Tests that we don't revalidate an entry unless we are required to do so.
TEST_F(HttpCacheRangeGetTest, Revalidate1) {
  MockHttpCache cache;
  std::string headers;

  // Write to the cache (40-49).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
      "Expires: Wed, 7 Sep 2033 21:46:42 GMT\n"  // Should never expire.
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 10\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  NetLogWithSource net_log_with_source =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(cache.http_cache(), transaction,
                                             &headers, net_log_with_source,
                                             &load_timing_info);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingCachedResponse(load_timing_info);

  // Read again forcing the revalidation.
  transaction.load_flags |= LOAD_VALIDATE_CACHE;
  RunTransactionTestWithResponseAndGetTiming(cache.http_cache(), transaction,
                                             &headers, net_log_with_source,
                                             &load_timing_info);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Checks that we revalidate an entry when the headers say so.
TEST_F(HttpCacheRangeGetTest, Revalidate2) {
  MockHttpCache cache;
  std::string headers;

  // Write to the cache (40-49).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
      "Expires: Sat, 18 Apr 2009 01:10:43 GMT\n"  // Expired.
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 10\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);
  Verify206Response(headers, 40, 49);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we deal with 304s for range requests.
TEST_F(HttpCacheRangeGetTest, 304) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), scoped_transaction,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RangeTransactionServer handler;
  handler.set_not_modified(true);
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.load_flags |= LOAD_VALIDATE_CACHE;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we deal with 206s when revalidating range requests.
TEST_F(HttpCacheRangeGetTest, ModifiedResult) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), scoped_transaction,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Attempt to read from the cache (40-49).
  RangeTransactionServer handler;
  handler.set_modified(true);
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.load_flags |= LOAD_VALIDATE_CACHE;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // And the entry should be gone.
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that when a server returns 206 with a sub-range of the requested range,
// and there is nothing stored in the cache, the returned response is passed to
// the caller as is. In this context, a subrange means a response that starts
// with the same byte that was requested, but that is not the whole range that
// was requested.
TEST_F(HttpCacheRangeGetTest, 206ReturnsSubrangeRangeNoCachedContent) {
  MockHttpCache cache;
  std::string headers;

  // Request a large range (40-59). The server sends 40-49.
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 40-59\r\n" EXTRA_HEADER;
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 10\n"
      "Content-Range: bytes 40-49/80\n";
  transaction.handler = MockTransactionHandler();
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that when a server returns 206 with a sub-range of the requested range,
// and there was an entry stored in the cache, the cache gets out of the way.
TEST_F(HttpCacheRangeGetTest, 206ReturnsSubrangeRangeCachedContent) {
  MockHttpCache cache;
  std::string headers;

  // Write to the cache (70-79).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 70-79\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);
  Verify206Response(headers, 70, 79);

  // Request a large range (40-79). The cache will ask the server for 40-59.
  // The server returns 40-49. The cache should consider the server confused and
  // abort caching, restarting the request without caching.
  transaction.request_headers = "Range: bytes = 40-79\r\n" EXTRA_HEADER;
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 10\n"
      "Content-Range: bytes 40-49/80\n";
  transaction.handler = MockTransactionHandler();
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // Two new network requests were issued, one from the cache and another after
  // deleting the entry.
  Verify206Response(headers, 40, 49);
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The entry was deleted.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that when a server returns 206 with a sub-range of the requested range,
// and there was an entry stored in the cache, the cache gets out of the way,
// when the caller is not using ranges.
TEST_F(HttpCacheGetTest, 206ReturnsSubrangeRangeCachedContent) {
  MockHttpCache cache;
  std::string headers;

  // Write to the cache (70-79).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 70-79\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);
  Verify206Response(headers, 70, 79);

  // Don't ask for a range. The cache will ask the server for 0-69.
  // The server returns 40-49. The cache should consider the server confused and
  // abort caching, restarting the request.
  // The second network request should not be a byte range request so the server
  // should return 200 + "Not a range"
  transaction.request_headers = "X-Return-Default-Range:\r\n" EXTRA_HEADER;
  transaction.data = "Not a range";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The entry was deleted.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that when a server returns 206 with a random range and there is
// nothing stored in the cache, the returned response is passed to the caller
// as is. In this context, a WrongRange means that the returned range may or may
// not have any relationship with the requested range (may or may not be
// contained). The important part is that the first byte doesn't match the first
// requested byte.
TEST_F(HttpCacheRangeGetTest, 206ReturnsWrongRangeNoCachedContent) {
  MockHttpCache cache;
  std::string headers;

  // Request a large range (30-59). The server sends (40-49).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 30-59\r\n" EXTRA_HEADER;
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 10\n"
      "Content-Range: bytes 40-49/80\n";
  transaction.handler = MockTransactionHandler();
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The entry was deleted.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that when a server returns 206 with a random range and there is
// an entry stored in the cache, the cache gets out of the way.
TEST_F(HttpCacheRangeGetTest, 206ReturnsWrongRangeCachedContent) {
  MockHttpCache cache;
  std::string headers;

  // Write to the cache (70-79).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 70-79\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);
  Verify206Response(headers, 70, 79);

  // Request a large range (30-79). The cache will ask the server for 30-69.
  // The server returns 40-49. The cache should consider the server confused and
  // abort caching, returning the weird range to the caller.
  transaction.request_headers = "Range: bytes = 30-79\r\n" EXTRA_HEADER;
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 10\n"
      "Content-Range: bytes 40-49/80\n";
  transaction.handler = MockTransactionHandler();
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The entry was deleted.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that when a caller asks for a range beyond EOF, with an empty cache,
// the response matches the one provided by the server.
TEST_F(HttpCacheRangeGetTest, 206ReturnsSmallerFileNoCachedContent) {
  MockHttpCache cache;
  std::string headers;

  // Request a large range (70-99). The server sends 70-79.
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 70-99\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 70, 79);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(1, cache.disk_cache()->open_count());
}

// Tests that when a caller asks for a range beyond EOF, with a cached entry,
// the cache automatically fixes the request.
TEST_F(HttpCacheRangeGetTest, 206ReturnsSmallerFileCachedContent) {
  MockHttpCache cache;
  std::string headers;

  // Write to the cache (40-49).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // Request a large range (70-99). The server sends 70-79.
  transaction.request_headers = "Range: bytes = 70-99\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 70, 79);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The entry was not deleted (the range was automatically fixed).
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that when a caller asks for a not-satisfiable range, the server's
// response is forwarded to the caller.
TEST_F(HttpCacheRangeGetTest, 416NoCachedContent) {
  MockHttpCache cache;
  std::string headers;

  // Request a range beyond EOF (80-99).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 80-99\r\n" EXTRA_HEADER;
  transaction.data = "";
  transaction.status = "HTTP/1.1 416 Requested Range Not Satisfiable";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(0U, headers.find(transaction.status));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The entry was deleted.
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we cache 301s for range requests.
TEST_F(HttpCacheRangeGetTest, MovedPermanently301) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.status = "HTTP/1.1 301 Moved Permanently";
  transaction.response_headers = "Location: http://www.bar.com/\n";
  transaction.data = "";
  transaction.handler = MockTransactionHandler();

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

using HttpCacheUnknownRangeGetTest = HttpCacheTest;

// Tests that we can cache range requests when the start or end is unknown.
// We start with one suffix request, followed by a request from a given point.
TEST_F(HttpCacheUnknownRangeGetTest, SuffixRangeThenIntRange) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (70-79).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = -10\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 70, 79);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Write and read from the cache (60-79).
  transaction.request_headers = "Range: bytes = 60-\r\n" EXTRA_HEADER;
  transaction.data = "rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 60, 79);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we can cache range requests when the start or end is unknown.
// We start with one request from a given point, followed by a suffix request.
// We'll also verify that synchronous cache responses work as intended.
TEST_F(HttpCacheUnknownRangeGetTest, IntRangeThenSuffixRange) {
  MockHttpCache cache;
  std::string headers;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.test_mode = TEST_MODE_SYNC_CACHE_START |
                          TEST_MODE_SYNC_CACHE_READ |
                          TEST_MODE_SYNC_CACHE_WRITE;

  // Write to the cache (70-79).
  transaction.request_headers = "Range: bytes = 70-\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 70, 79);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Write and read from the cache (60-79).
  transaction.request_headers = "Range: bytes = -20\r\n" EXTRA_HEADER;
  transaction.data = "rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 60, 79);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Similar to UnknownRangeGET_2, except that the resource size is empty.
// Regression test for crbug.com/813061, and probably https://crbug.com/1375128
TEST_F(HttpCacheUnknownRangeGetTest, SuffixRangeEmptyResponse) {
  MockHttpCache cache;
  std::string headers;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Cache-Control: max-age=10000\n"
      "Content-Length: 0\n",
  transaction.data = "";
  transaction.test_mode = TEST_MODE_SYNC_CACHE_START |
                          TEST_MODE_SYNC_CACHE_READ |
                          TEST_MODE_SYNC_CACHE_WRITE;

  // Write the empty resource to the cache.
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(
      "HTTP/1.1 200 OK\nCache-Control: max-age=10000\nContent-Length: 0\n",
      headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Write and read from the cache. This used to trigger a DCHECK
  // (or loop infinitely with it off).
  transaction.request_headers = "Range: bytes = -20\r\n" EXTRA_HEADER;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(
      "HTTP/1.1 200 OK\nCache-Control: max-age=10000\nContent-Length: 0\n",
      headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Testcase for https://crbug.com/1433305, validation of range request to a
// cache 302, which is notably bodiless.
TEST_F(HttpCacheUnknownRangeGetTest, Empty302) {
  MockHttpCache cache;
  std::string headers;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.status = "HTTP/1.1 302 Found";
  transaction.response_headers =
      "Cache-Control: max-age=0\n"
      "Content-Length: 0\n"
      "Location: https://example.org/\n",

  transaction.data = "";
  transaction.request_headers = "Range: bytes = 0-\r\n" EXTRA_HEADER;
  transaction.test_mode = TEST_MODE_SYNC_CACHE_START |
                          TEST_MODE_SYNC_CACHE_READ |
                          TEST_MODE_SYNC_CACHE_WRITE;

  // Write the empty resource to the cache.
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(
      "HTTP/1.1 302 Found\n"
      "Cache-Control: max-age=0\n"
      "Content-Length: 0\n"
      "Location: https://example.org/\n",
      headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Try to read from the cache. This should send a network request to
  // validate it, and get a different redirect.
  transaction.response_headers =
      "Cache-Control: max-age=0\n"
      "Content-Length: 0\n"
      "Location: https://example.com/\n",
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(
      "HTTP/1.1 302 Found\n"
      "Cache-Control: max-age=0\n"
      "Content-Length: 0\n"
      "Location: https://example.com/\n",
      headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  // A new entry is created since this one isn't conditionalizable.
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Testcase for https://crbug.com/1433305, validation of range request to a
// cache 302, which is notably bodiless, where the 302 is replaced with an
// actual body.
TEST_F(HttpCacheUnknownRangeGetTest, Empty302Replaced) {
  MockHttpCache cache;
  std::string headers;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.status = "HTTP/1.1 302 Found";
  transaction.response_headers =
      "Cache-Control: max-age=0\n"
      "Content-Length: 0\n"
      "Location: https://example.org/\n",

  transaction.data = "";
  transaction.test_mode = TEST_MODE_SYNC_CACHE_START |
                          TEST_MODE_SYNC_CACHE_READ |
                          TEST_MODE_SYNC_CACHE_WRITE;

  // Write the empty resource to the cache.
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(
      "HTTP/1.1 302 Found\n"
      "Cache-Control: max-age=0\n"
      "Content-Length: 0\n"
      "Location: https://example.org/\n",
      headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::RunLoop().RunUntilIdle();

  // Try to read from the cache. This should send a network request to
  // validate it, and get a different response.
  transaction.handler =
      base::BindRepeating(&RangeTransactionServer::RangeHandler);
  transaction.request_headers = "Range: bytes = -30\r\n" EXTRA_HEADER;
  // Tail 30 bytes out of 80
  transaction.data = "rg: 50-59 rg: 60-69 rg: 70-79 ";
  transaction.status = "HTTP/1.1 206 Partial Content";
  transaction.response_headers = "Content-Length: 10\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(
      "HTTP/1.1 206 Partial Content\n"
      "Content-Range: bytes 50-79/80\n"
      "Content-Length: 30\n",
      headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  // A new entry is created since this one isn't conditionalizable.
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that receiving Not Modified when asking for an open range doesn't mess
// up things.
TEST_F(HttpCacheUnknownRangeGetTest, Basic304) {
  MockHttpCache cache;
  std::string headers;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  RangeTransactionServer handler;
  handler.set_not_modified(true);

  // Ask for the end of the file, without knowing the length.
  transaction.request_headers = "Range: bytes = 70-\r\n" EXTRA_HEADER;
  transaction.data = "";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // We just bypass the cache.
  EXPECT_EQ(0U, headers.find("HTTP/1.1 304 Not Modified\n"));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we can handle non-range requests when we have cached a range.
TEST_F(HttpCacheGetTest, Previous206) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  std::string headers;
  NetLogWithSource net_log_with_source =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  LoadTimingInfo load_timing_info;

  // Write to the cache (40-49).
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), kRangeGET_TransactionOK, &headers,
      net_log_with_source, &load_timing_info);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  // Write and read from the cache (0-79), when not asked for a range.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = kFullRangeData;
  RunTransactionTestWithResponseAndGetTiming(cache.http_cache(), transaction,
                                             &headers, net_log_with_source,
                                             &load_timing_info);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests that we can handle non-range requests when we have cached the first
// part of the object and the server replies with 304 (Not Modified).
TEST_F(HttpCacheGetTest, Previous206NotModified) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  std::string headers;
  NetLogWithSource net_log_with_source =
      NetLogWithSource::Make(NetLogSourceType::NONE);

  LoadTimingInfo load_timing_info;

  // Write to the cache (0-9).
  transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
  transaction.data = "rg: 00-09 ";
  RunTransactionTestWithResponseAndGetTiming(cache.http_cache(), transaction,
                                             &headers, net_log_with_source,
                                             &load_timing_info);
  Verify206Response(headers, 0, 9);
  TestLoadTimingNetworkRequest(load_timing_info);

  // Write to the cache (70-79).
  transaction.request_headers = "Range: bytes = 70-79\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponseAndGetTiming(cache.http_cache(), transaction,
                                             &headers, net_log_with_source,
                                             &load_timing_info);
  Verify206Response(headers, 70, 79);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  // Read from the cache (0-9), write and read from cache (10 - 79).
  transaction.load_flags |= LOAD_VALIDATE_CACHE;
  transaction.request_headers = "Foo: bar\r\n" EXTRA_HEADER;
  transaction.data = kFullRangeData;
  RunTransactionTestWithResponseAndGetTiming(cache.http_cache(), transaction,
                                             &headers, net_log_with_source,
                                             &load_timing_info);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests that we can handle a regular request to a sparse entry, that results in
// new content provided by the server (206).
TEST_F(HttpCacheGetTest, Previous206NewContent) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (0-9).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
  transaction.data = "rg: 00-09 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 0, 9);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now we'll issue a request without any range that should result first in a
  // 206 (when revalidating), and then in a weird standard answer: the test
  // server will not modify the response so we'll get the default range... a
  // real server will answer with 200.
  MockTransaction transaction2(kRangeGET_TransactionOK);
  transaction2.request_headers = EXTRA_HEADER;
  transaction2.load_flags |= LOAD_VALIDATE_CACHE;
  transaction2.data = "Not a range";
  RangeTransactionServer handler;
  handler.set_modified(true);
  LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction2, &headers,
      NetLogWithSource::Make(NetLogSourceType::NONE), &load_timing_info);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  // Verify that the previous request deleted the entry.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we can handle cached 206 responses that are not sparse.
TEST_F(HttpCacheGetTest, Previous206NotSparse) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);
  // Create a disk cache entry that stores 206 headers while not being sparse.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.CreateBackendEntry(request.CacheKey(), &entry, nullptr));

  std::string raw_headers(kRangeGET_TransactionOK.status);
  raw_headers.append("\n");
  raw_headers.append(kRangeGET_TransactionOK.response_headers);

  HttpResponseInfo response;
  response.headers = base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(raw_headers));
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, false));

  auto buf(base::MakeRefCounted<IOBufferWithSize>(500));
  int len = static_cast<int>(
      base::strlcpy(buf->data(), kRangeGET_TransactionOK.data, 500));
  TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  EXPECT_EQ(len, cb.GetResult(rv));
  entry->Close();

  // Now see that we don't use the stored entry.
  std::string headers;
  LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), kSimpleGET_Transaction, &headers,
      NetLogWithSource::Make(NetLogSourceType::NONE), &load_timing_info);

  // We are expecting a 200.
  std::string expected_headers(kSimpleGET_Transaction.status);
  expected_headers.append("\n");
  expected_headers.append(kSimpleGET_Transaction.response_headers);
  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests that we can handle cached 206 responses that are not sparse. This time
// we issue a range request and expect to receive a range.
TEST_F(HttpCacheRangeGetTest, Previous206NotSparser2) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  // Create a disk cache entry that stores 206 headers while not being sparse.
  MockHttpRequest request(kRangeGET_TransactionOK);
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.CreateBackendEntry(request.CacheKey(), &entry, nullptr));

  std::string raw_headers(kRangeGET_TransactionOK.status);
  raw_headers.append("\n");
  raw_headers.append(kRangeGET_TransactionOK.response_headers);

  HttpResponseInfo response;
  response.headers = base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(raw_headers));
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, false));

  auto buf = base::MakeRefCounted<IOBufferWithSize>(500);
  int len = static_cast<int>(
      base::strlcpy(buf->data(), kRangeGET_TransactionOK.data, 500));
  TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  EXPECT_EQ(len, cb.GetResult(rv));
  entry->Close();

  // Now see that we don't use the stored entry.
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  // We are expecting a 206.
  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we can handle cached 206 responses that can't be validated.
TEST_F(HttpCacheGetTest, Previous206NotValidation) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);
  // Create a disk cache entry that stores 206 headers.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.CreateBackendEntry(request.CacheKey(), &entry, nullptr));

  // Make sure that the headers cannot be validated with the server.
  std::string raw_headers(kRangeGET_TransactionOK.status);
  raw_headers.append("\n");
  raw_headers.append("Content-Length: 80\n");

  HttpResponseInfo response;
  response.headers = base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(raw_headers));
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, false));

  auto buf = base::MakeRefCounted<IOBufferWithSize>(500);
  int len = static_cast<int>(
      base::strlcpy(buf->data(), kRangeGET_TransactionOK.data, 500));
  TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  EXPECT_EQ(len, cb.GetResult(rv));
  entry->Close();

  // Now see that we don't use the stored entry.
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), kSimpleGET_Transaction,
                                 &headers);

  // We are expecting a 200.
  std::string expected_headers(kSimpleGET_Transaction.status);
  expected_headers.append("\n");
  expected_headers.append(kSimpleGET_Transaction.response_headers);
  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we can handle range requests with cached 200 responses.
TEST_F(HttpCacheRangeGetTest, Previous200) {
  MockHttpCache cache;

  {
    // Store the whole thing with status 200.
    ScopedMockTransaction transaction(kTypicalGET_Transaction,
                                      kRangeGET_TransactionOK.url);
    transaction.data = kFullRangeData;
    RunTransactionTest(cache.http_cache(), transaction);
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }

  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  // Now see that we use the stored entry.
  std::string headers;
  MockTransaction transaction2(kRangeGET_TransactionOK);
  RangeTransactionServer handler;
  handler.set_not_modified(true);
  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);

  // We are expecting a 206.
  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The last transaction has finished so make sure the entry is deactivated.
  base::RunLoop().RunUntilIdle();

  // Make a request for an invalid range.
  MockTransaction transaction3(kRangeGET_TransactionOK);
  transaction3.request_headers = "Range: bytes = 80-90\r\n" EXTRA_HEADER;
  transaction3.data = kFullRangeData;
  transaction3.load_flags = LOAD_SKIP_CACHE_VALIDATION;
  RunTransactionTestWithResponse(cache.http_cache(), transaction3, &headers);
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 "));
  EXPECT_EQ(std::string::npos, headers.find("Content-Range:"));
  EXPECT_EQ(std::string::npos, headers.find("Content-Length: 80"));

  // Make sure the entry is deactivated.
  base::RunLoop().RunUntilIdle();

  // Even though the request was invalid, we should have the entry.
  RunTransactionTest(cache.http_cache(), transaction2);
  EXPECT_EQ(3, cache.disk_cache()->open_count());

  // Make sure the entry is deactivated.
  base::RunLoop().RunUntilIdle();

  // Now we should receive a range from the server and drop the stored entry.
  handler.set_not_modified(false);
  transaction2.request_headers = kRangeGET_TransactionOK.request_headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);
  Verify206Response(headers, 40, 49);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(4, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), transaction2);
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we can handle a 200 response when dealing with sparse entries.
TEST_F(HttpCacheTest, RangeRequestResultsIn200) {
  MockHttpCache cache;
  std::string headers;

  {
    ScopedMockTransaction transaction(kRangeGET_TransactionOK);
    // Write to the cache (70-79).
    transaction.request_headers = "Range: bytes = -10\r\n" EXTRA_HEADER;
    transaction.data = "rg: 70-79 ";
    RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

    Verify206Response(headers, 70, 79);
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }
  // Now we'll issue a request that results in a plain 200 response, but to
  // the to the same URL that we used to store sparse data, and making sure
  // that we ask for a range.
  ScopedMockTransaction transaction2(kSimpleGET_Transaction,
                                     kRangeGET_TransactionOK.url);
  transaction2.request_headers = kRangeGET_TransactionOK.request_headers;

  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);

  std::string expected_headers(kSimpleGET_Transaction.status);
  expected_headers.append("\n");
  expected_headers.append(kSimpleGET_Transaction.response_headers);
  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that a range request that falls outside of the size that we know about
// only deletes the entry if the resource has indeed changed.
TEST_F(HttpCacheRangeGetTest, MoreThanCurrentSize) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // A weird request should not delete this entry. Ask for bytes 120-.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 120-\r\n" EXTRA_HEADER;
  transaction.data = "";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 416 "));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we don't delete a sparse entry when we cancel a request.
TEST_F(HttpCacheRangeGetTest, Cancel) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  MockHttpRequest request(kRangeGET_TransactionOK);

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(10);
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }
  EXPECT_EQ(buf->size(), rv);

  // Destroy the transaction.
  c.reset();

  // Verify that the entry has not been deleted.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(request.CacheKey(), &entry));
  entry->Close();
}

// Tests that we don't mark an entry as truncated if it is partial and not
// already truncated.
TEST_F(HttpCacheRangeGetTest, CancelWhileReading) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  MockHttpRequest request(kRangeGET_TransactionOK);

  auto context = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&context->trans);
  ASSERT_THAT(rv, IsOk());

  rv = context->trans->Start(&request, context->callback.callback(),
                             NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = context->callback.WaitForResult();
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Start Read.
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(5);
  rv = context->trans->Read(buf.get(), buf->size(),
                            context->callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Destroy the transaction.
  context.reset();

  // Complete Read.
  base::RunLoop().RunUntilIdle();

  // Verify that the entry has not been marked as truncated.
  VerifyTruncatedFlag(&cache, request.CacheKey(), false, 0);
}

// Tests that we don't delete a sparse entry when we start a new request after
// cancelling the previous one.
TEST_F(HttpCacheRangeGetTest, Cancel2) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  MockHttpRequest request(kRangeGET_TransactionOK);
  request.load_flags |= LOAD_VALIDATE_CACHE;

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that we revalidate the entry and read from the cache (a single
  // read will return while waiting for the network).
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(5);
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(5, c->callback.GetResult(rv));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Destroy the transaction before completing the read.
  c.reset();

  // We have the read and the delete (OnProcessPendingQueue) waiting on the
  // message loop. This means that a new transaction will just reuse the same
  // active entry (no open or create).

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// A slight variation of the previous test, this time we cancel two requests in
// a row, making sure that the second is waiting for the entry to be ready.
TEST_F(HttpCacheRangeGetTest, Cancel3) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  MockHttpRequest request(kRangeGET_TransactionOK);
  request.load_flags |= LOAD_VALIDATE_CACHE;

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = c->callback.WaitForResult();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that we revalidate the entry and read from the cache (a single
  // read will return while waiting for the network).
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(5);
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(5, c->callback.GetResult(rv));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Destroy the previous transaction before completing the read.
  c.reset();

  // We have the read and the delete (OnProcessPendingQueue) waiting on the
  // message loop. This means that a new transaction will just reuse the same
  // active entry (no open or create).

  c = std::make_unique<Context>();
  rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  MockDiskEntry::IgnoreCallbacks(true);
  base::RunLoop().RunUntilIdle();
  MockDiskEntry::IgnoreCallbacks(false);

  // The new transaction is waiting for the query range callback.
  c.reset();

  // And we should not crash when the callback is delivered.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that an invalid range response results in no cached entry.
TEST_F(HttpCacheRangeGetTest, InvalidResponse1) {
  MockHttpCache cache;
  std::string headers;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = MockTransactionHandler();
  transaction.response_headers =
      "Content-Range: bytes 40-49/45\n"
      "Content-Length: 10\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  std::string expected(transaction.status);
  expected.append("\n");
  expected.append(transaction.response_headers);
  EXPECT_EQ(expected, headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that we don't have a cached entry.
  disk_cache::Entry* entry;
  MockHttpRequest request(transaction);
  EXPECT_FALSE(cache.OpenBackendEntry(request.CacheKey(), &entry));
}

// Tests that we reject a range that doesn't match the content-length.
TEST_F(HttpCacheRangeGetTest, InvalidResponse2) {
  MockHttpCache cache;
  std::string headers;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = MockTransactionHandler();
  transaction.response_headers =
      "Content-Range: bytes 40-49/80\n"
      "Content-Length: 20\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  std::string expected(transaction.status);
  expected.append("\n");
  expected.append(transaction.response_headers);
  EXPECT_EQ(expected, headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that we don't have a cached entry.
  disk_cache::Entry* entry;
  MockHttpRequest request(transaction);
  EXPECT_FALSE(cache.OpenBackendEntry(request.CacheKey(), &entry));
}

// Tests that if a server tells us conflicting information about a resource we
// drop the entry.
TEST_F(HttpCacheRangeGetTest, InvalidResponse3) {
  MockHttpCache cache;
  std::string headers;
  {
    ScopedMockTransaction transaction(kRangeGET_TransactionOK);
    transaction.handler = MockTransactionHandler();
    transaction.request_headers = "Range: bytes = 50-59\r\n" EXTRA_HEADER;
    std::string response_headers(transaction.response_headers);
    response_headers.append("Content-Range: bytes 50-59/160\n");
    transaction.response_headers = response_headers.c_str();
    RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

    Verify206Response(headers, 50, 59);
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  // This transaction will report a resource size of 80 bytes, and we think it's
  // 160 so we should ignore the response.
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the entry is gone.
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we handle large range values properly.
TEST_F(HttpCacheRangeGetTest, LargeValues) {
  // We need a real sparse cache for this test.
  MockHttpCache cache(HttpCache::DefaultBackend::InMemory(1024 * 1024));
  std::string headers;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = MockTransactionHandler();
  transaction.request_headers =
      "Range: bytes = 4294967288-4294967297\r\n" EXTRA_HEADER;
  transaction.response_headers =
      "ETag: \"foo\"\n"
      "Content-Range: bytes 4294967288-4294967297/4294967299\n"
      "Content-Length: 10\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  std::string expected(transaction.status);
  expected.append("\n");
  expected.append(transaction.response_headers);
  EXPECT_EQ(expected, headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Verify that we have a cached entry.
  disk_cache::Entry* en;
  MockHttpRequest request(transaction);
  ASSERT_TRUE(cache.OpenBackendEntry(request.CacheKey(), &en));
  en->Close();
}

// Tests that we don't crash with a range request if the disk cache was not
// initialized properly.
TEST_F(HttpCacheRangeGetTest, NoDiskCache) {
  auto factory = std::make_unique<MockBlockingBackendFactory>();
  factory->set_fail(true);
  factory->FinishCreation();  // We'll complete synchronously.
  MockHttpCache cache(std::move(factory));

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
}

// Tests that we handle byte range requests that skip the cache.
TEST_F(HttpCacheTest, RangeHead) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = -10\r\n" EXTRA_HEADER;
  transaction.method = "HEAD";
  transaction.data = "rg: 70-79 ";

  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 70, 79);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that we don't crash when after reading from the cache we issue a
// request for the next range and the server gives us a 200 synchronously.
TEST_F(HttpCacheRangeGetTest, FastFlakyServer) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 40-\r\n" EXTRA_HEADER;
  transaction.test_mode = TEST_MODE_SYNC_NET_START;
  transaction.load_flags |= LOAD_VALIDATE_CACHE;

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);

  // And now read from the cache and the network.
  RangeTransactionServer handler;
  handler.set_bad_200(true);
  transaction.data = "Not a range";
  RecordingNetLogObserver net_log_observer;
  RunTransactionTestWithLog(cache.http_cache(), transaction,
                            NetLogWithSource::Make(NetLogSourceType::NONE));

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  EXPECT_TRUE(LogContainsEventType(
      net_log_observer, NetLogEventType::HTTP_CACHE_RE_SEND_PARTIAL_REQUEST));
}

// Tests that when the server gives us less data than expected, we don't keep
// asking for more data.
TEST_F(HttpCacheRangeGetTest, FastFlakyServer2) {
  MockHttpCache cache;

  // First, check with an empty cache (WRITE mode).
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 40-49\r\n" EXTRA_HEADER;
  transaction.data = "rg: 40-";  // Less than expected.
  transaction.handler = MockTransactionHandler();
  std::string headers(transaction.response_headers);
  headers.append("Content-Range: bytes 40-49/80\n");
  transaction.response_headers = headers.c_str();

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now verify that even in READ_WRITE mode, we forward the bad response to
  // the caller.
  transaction.request_headers = "Range: bytes = 60-69\r\n" EXTRA_HEADER;
  transaction.data = "rg: 60-";  // Less than expected.
  headers = kRangeGET_TransactionOK.response_headers;
  headers.append("Content-Range: bytes 60-69/80\n");
  transaction.response_headers = headers.c_str();

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheRangeGetTest, OkLoadOnlyFromCache) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  // Write to the cache (40-49).
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Force this transaction to read from the cache.
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  std::unique_ptr<HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
  }
  ASSERT_THAT(rv, IsError(ERR_CACHE_MISS));

  trans.reset();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests the handling of the "truncation" flag.
TEST_F(HttpCacheTest, WriteResponseInfoTruncated) {
  MockHttpCache cache;
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.CreateBackendEntry(
      GenerateCacheKey("http://www.google.com"), &entry, nullptr));

  HttpResponseInfo response;
  response.headers = base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders("HTTP/1.1 200 OK"));

  // Set the last argument for this to be an incomplete request.
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, true));
  bool truncated = false;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_TRUE(truncated);

  // And now test the opposite case.
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, false));
  truncated = true;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_FALSE(truncated);
  entry->Close();
}

// Tests basic pickling/unpickling of HttpResponseInfo.
TEST_F(HttpCacheTest, PersistHttpResponseInfo) {
  const IPEndPoint expected_endpoint = IPEndPoint(IPAddress(1, 2, 3, 4), 80);
  // Set some fields (add more if needed.)
  HttpResponseInfo response1;
  response1.was_cached = false;
  response1.remote_endpoint = expected_endpoint;
  response1.headers =
      base::MakeRefCounted<HttpResponseHeaders>("HTTP/1.1 200 OK");

  // Pickle.
  base::Pickle pickle;
  response1.Persist(&pickle, false, false);

  // Unpickle.
  HttpResponseInfo response2;
  bool response_truncated;
  EXPECT_TRUE(response2.InitFromPickle(pickle, &response_truncated));
  EXPECT_FALSE(response_truncated);

  // Verify fields.
  EXPECT_TRUE(response2.was_cached);  // InitFromPickle sets this flag.
  EXPECT_EQ(expected_endpoint, response2.remote_endpoint);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

// Tests that we delete an entry when the request is cancelled before starting
// to read from the network.
TEST_F(HttpCacheTest, DoomOnDestruction) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    c->result = c->callback.WaitForResult();
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Destroy the transaction. We only have the headers so we should delete this
  // entry.
  c.reset();

  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we delete an entry when the request is cancelled if the response
// does not have content-length and strong validators.
TEST_F(HttpCacheTest, DoomOnDestruction2) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(10);
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }
  EXPECT_EQ(buf->size(), rv);

  // Destroy the transaction.
  c.reset();

  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we delete an entry when the request is cancelled if the response
// has an "Accept-Ranges: none" header.
TEST_F(HttpCacheTest, DoomOnDestruction3) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 22\n"
      "Accept-Ranges: none\n"
      "Etag: \"foopy\"\n";
  MockHttpRequest request(transaction);

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(10);
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }
  EXPECT_EQ(buf->size(), rv);

  // Destroy the transaction.
  c.reset();

  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we mark an entry as incomplete when the request is cancelled.
TEST_F(HttpCacheTest, SetTruncatedFlag) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 22\n"
      "Etag: \"foopy\"\n";
  MockHttpRequest request(transaction);

  auto c = std::make_unique<Context>();

  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(10);
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }
  EXPECT_EQ(buf->size(), rv);

  // We want to cancel the request when the transaction is busy.
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(c->callback.have_result());

  // Destroy the transaction.
  c->trans.reset();

  // Make sure that we don't invoke the callback. We may have an issue if the
  // UrlRequestJob is killed directly (without cancelling the UrlRequest) so we
  // could end up with the transaction being deleted twice if we send any
  // notification from the transaction destructor (see http://crbug.com/31723).
  EXPECT_FALSE(c->callback.have_result());

  base::RunLoop().RunUntilIdle();
  VerifyTruncatedFlag(&cache, request.CacheKey(), true, 0);
}

// Tests that we do not mark an entry as truncated when the request is
// cancelled.
TEST_F(HttpCacheTest, DontSetTruncatedFlagForGarbledResponseCode) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 22\n"
      "Etag: \"foopy\"\n";
  transaction.status = "HTTP/1.1 2";
  MockHttpRequest request(transaction);

  auto c = std::make_unique<Context>();

  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(10);
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }
  EXPECT_EQ(buf->size(), rv);

  // We want to cancel the request when the transaction is busy.
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(c->callback.have_result());

  MockHttpCache::SetTestMode(TEST_MODE_SYNC_ALL);

  // Destroy the transaction.
  c->trans.reset();
  MockHttpCache::SetTestMode(0);

  // Make sure that we don't invoke the callback. We may have an issue if the
  // UrlRequestJob is killed directly (without cancelling the UrlRequest) so we
  // could end up with the transaction being deleted twice if we send any
  // notification from the transaction destructor (see http://crbug.com/31723).
  EXPECT_FALSE(c->callback.have_result());

  // Verify that the entry is deleted as well, since the response status is
  // garbled. Note that the entry will be deleted after the pending Read is
  // complete.
  base::RunLoop().RunUntilIdle();
  disk_cache::Entry* entry;
  ASSERT_FALSE(cache.OpenBackendEntry(request.CacheKey(), &entry));
}

// Tests that we don't mark an entry as truncated when we read everything.
TEST_F(HttpCacheTest, DontSetTruncatedFlag) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 22\n"
      "Etag: \"foopy\"\n";
  MockHttpRequest request(transaction);

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_THAT(c->callback.GetResult(rv), IsOk());

  // Read everything.
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(22);
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(buf->size(), c->callback.GetResult(rv));

  // Destroy the transaction.
  c->trans.reset();

  // Verify that the entry is not marked as truncated.
  VerifyTruncatedFlag(&cache, request.CacheKey(), false, 0);
}

// Tests that sparse entries don't set the truncate flag.
TEST_F(HttpCacheRangeGetTest, DontTruncate) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 0-19\r\n" EXTRA_HEADER;

  auto request = std::make_unique<MockHttpRequest>(transaction);
  std::unique_ptr<HttpTransaction> trans;

  int rv = cache.http_cache()->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());

  TestCompletionCallback cb;
  rv = trans->Start(request.get(), cb.callback(), NetLogWithSource());
  EXPECT_EQ(0, cb.GetResult(rv));

  auto buf = base::MakeRefCounted<IOBufferWithSize>(10);
  rv = trans->Read(buf.get(), 10, cb.callback());
  EXPECT_EQ(10, cb.GetResult(rv));

  // Should not trigger any DCHECK.
  trans.reset();
  VerifyTruncatedFlag(&cache, request->CacheKey(), false, 0);
}

// Tests that sparse entries don't set the truncate flag (when the byte range
//  starts after 0).
TEST_F(HttpCacheRangeGetTest, DontTruncate2) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 30-49\r\n" EXTRA_HEADER;

  auto request = std::make_unique<MockHttpRequest>(transaction);
  std::unique_ptr<HttpTransaction> trans;

  int rv = cache.http_cache()->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());

  TestCompletionCallback cb;
  rv = trans->Start(request.get(), cb.callback(), NetLogWithSource());
  EXPECT_EQ(0, cb.GetResult(rv));

  auto buf = base::MakeRefCounted<IOBufferWithSize>(10);
  rv = trans->Read(buf.get(), 10, cb.callback());
  EXPECT_EQ(10, cb.GetResult(rv));

  // Should not trigger any DCHECK.
  trans.reset();
  VerifyTruncatedFlag(&cache, request->CacheKey(), false, 0);
}

// Tests that we can continue with a request that was interrupted.
TEST_F(HttpCacheGetTest, IncompleteResource) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  std::string headers;
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = kFullRangeData;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // We update the headers with the ones received while revalidating.
  std::string expected_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "Accept-Ranges: bytes\n"
      "ETag: \"foo\"\n"
      "Content-Length: 80\n");

  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was updated.
  MockHttpRequest request(transaction);
  VerifyTruncatedFlag(&cache, request.CacheKey(), false, 80);
}

// Tests the handling of no-store when revalidating a truncated entry.
TEST_F(HttpCacheGetTest, IncompleteResourceNoStore) {
  MockHttpCache cache;
  {
    ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

    std::string raw_headers(
        "HTTP/1.1 200 OK\n"
        "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
        "ETag: \"foo\"\n"
        "Accept-Ranges: bytes\n"
        "Content-Length: 80\n");
    CreateTruncatedEntry(raw_headers, &cache);
  }

  // Now make a regular request.
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  std::string response_headers(transaction.response_headers);
  response_headers += ("Cache-Control: no-store\n");
  transaction.response_headers = response_headers.c_str();
  transaction.data = kFullRangeData;

  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // We update the headers with the ones received while revalidating.
  std::string expected_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "Accept-Ranges: bytes\n"
      "Cache-Control: no-store\n"
      "ETag: \"foo\"\n"
      "Content-Length: 80\n");

  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was deleted.
  disk_cache::Entry* entry;
  MockHttpRequest request(transaction);
  EXPECT_FALSE(cache.OpenBackendEntry(request.CacheKey(), &entry));
}

// Tests cancelling a request after the server sent no-store.
TEST_F(HttpCacheGetTest, IncompleteResourceCancel) {
  MockHttpCache cache;
  {
    ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
    std::string raw_headers(
        "HTTP/1.1 200 OK\n"
        "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
        "ETag: \"foo\"\n"
        "Accept-Ranges: bytes\n"
        "Content-Length: 80\n");
    CreateTruncatedEntry(raw_headers, &cache);
  }

  // Now make a regular request.
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  std::string response_headers(transaction.response_headers);
  response_headers += ("Cache-Control: no-store\n");
  transaction.response_headers = response_headers.c_str();
  transaction.data = kFullRangeData;

  MockHttpRequest request(transaction);
  auto c = std::make_unique<Context>();

  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  // Queue another request to this transaction. We have to start this request
  // before the first one gets the response from the server and dooms the entry,
  // otherwise it will just create a new entry without being queued to the first
  // request.
  auto pending = std::make_unique<Context>();
  ASSERT_THAT(cache.CreateTransaction(&pending->trans), IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_EQ(ERR_IO_PENDING,
            pending->trans->Start(&request, pending->callback.callback(),
                                  NetLogWithSource()));
  EXPECT_THAT(c->callback.GetResult(rv), IsOk());

  // Make sure that the entry has some data stored.
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(5);
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(5, c->callback.GetResult(rv));

  // Since |pending| is currently validating the already written headers
  // it will be restarted as well.
  c.reset();
  pending.reset();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  base::RunLoop().RunUntilIdle();
}

// Tests that we delete truncated entries if the server changes its mind midway.
TEST_F(HttpCacheGetTest, IncompleteResource2) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  // Content-length will be intentionally bad.
  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 50\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request. We expect the code to fail the validation and
  // retry the request without using byte ranges.
  std::string headers;
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "Not a range";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // The server will return 200 instead of a byte range.
  std::string expected_headers(
      "HTTP/1.1 200 OK\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n");

  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was deleted.
  disk_cache::Entry* entry;
  MockHttpRequest request(transaction);
  ASSERT_FALSE(cache.OpenBackendEntry(request.CacheKey(), &entry));
}

// Tests that we always validate a truncated request.
TEST_F(HttpCacheGetTest, IncompleteResource3) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  // This should not require validation for 10 hours.
  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Cache-Control: max-age= 36000\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  std::string headers;
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = kFullRangeData;

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  MockHttpRequest request(transaction);
  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_THAT(c->callback.GetResult(rv), IsOk());

  // We should have checked with the server before finishing Start().
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we handle 401s for truncated resources.
TEST_F(HttpCacheGetTest, IncompleteResourceWithAuth) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "X-Require-Mock-Auth: dummy\r\n" EXTRA_HEADER;
  transaction.data = kFullRangeData;
  RangeTransactionServer handler;

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  MockHttpRequest request(transaction);
  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_THAT(c->callback.GetResult(rv), IsOk());

  const HttpResponseInfo* response = c->trans->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_EQ(401, response->headers->response_code());
  rv = c->trans->RestartWithAuth(AuthCredentials(), c->callback.callback());
  EXPECT_THAT(c->callback.GetResult(rv), IsOk());
  response = c->trans->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_EQ(200, response->headers->response_code());

  ReadAndVerifyTransaction(c->trans.get(), transaction);
  c.reset();  // The destructor could delete the entry.
  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  // Verify that the entry was deleted.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(request.CacheKey(), &entry));
  entry->Close();
}

// Test that the transaction won't retry failed partial requests
// after it starts reading data.  http://crbug.com/474835
TEST_F(HttpCacheTest, TransactionRetryLimit) {
  MockHttpCache cache;

  // Cache 0-9, so that we have data to read before failing.
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
  transaction.data = "rg: 00-09 ";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // And now read from the cache and the network.  10-19 will get a
  // 401, but will have already returned 0-9.
  // We do not set X-Require-Mock-Auth because that causes the mock
  // network transaction to become IsReadyToRestartForAuth().
  transaction.request_headers =
      "Range: bytes = 0-79\r\n"
      "X-Require-Mock-Auth-Alt: dummy\r\n" EXTRA_HEADER;

  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  MockHttpRequest request(transaction);

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = c->callback.WaitForResult();
  }
  std::string content;
  rv = ReadTransaction(c->trans.get(), &content);
  EXPECT_THAT(rv, IsError(ERR_CACHE_AUTH_FAILURE_AFTER_READ));
}

// Tests that we cache a 200 response to the validation request.
TEST_F(HttpCacheGetTest, IncompleteResource4) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  std::string headers;
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "Not a range";
  RangeTransactionServer handler;
  handler.set_bad_200(true);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was updated.
  MockHttpRequest request(transaction);
  VerifyTruncatedFlag(&cache, request.CacheKey(), false, 11);
}

// Tests that when we cancel a request that was interrupted, we mark it again
// as truncated.
TEST_F(HttpCacheGetTest, CancelIncompleteResource) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  transaction.request_headers = EXTRA_HEADER;

  MockHttpRequest request(transaction);
  auto c = std::make_unique<Context>();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_THAT(rv, IsOk());

  rv = c->trans->Start(&request, c->callback.callback(), NetLogWithSource());
  EXPECT_THAT(c->callback.GetResult(rv), IsOk());

  // Read 20 bytes from the cache, and 10 from the net.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(100);
  rv = c->trans->Read(buf.get(), 20, c->callback.callback());
  EXPECT_EQ(20, c->callback.GetResult(rv));
  rv = c->trans->Read(buf.get(), 10, c->callback.callback());
  EXPECT_EQ(10, c->callback.GetResult(rv));

  // At this point, we are already reading so canceling the request should leave
  // a truncated one.
  c.reset();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was updated: now we have 30 bytes.
  VerifyTruncatedFlag(&cache, request.CacheKey(), true, 30);
}

// Tests that we can handle range requests when we have a truncated entry.
TEST_F(HttpCacheRangeGetTest, IncompleteResource) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  // Content-length will be intentionally bogus.
  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: something\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 10\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a range request.
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST_F(HttpCacheTest, SyncRead) {
  MockHttpCache cache;

  // This test ensures that a read that completes synchronously does not cause
  // any problems.

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.test_mode |=
      (TEST_MODE_SYNC_CACHE_START | TEST_MODE_SYNC_CACHE_READ |
       TEST_MODE_SYNC_CACHE_WRITE);

  MockHttpRequest r1(transaction), r2(transaction), r3(transaction);

  TestTransactionConsumer c1(DEFAULT_PRIORITY, cache.http_cache()),
      c2(DEFAULT_PRIORITY, cache.http_cache()),
      c3(DEFAULT_PRIORITY, cache.http_cache());

  c1.Start(&r1, NetLogWithSource());

  r2.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  c2.Start(&r2, NetLogWithSource());

  r3.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  c3.Start(&r3, NetLogWithSource());

  EXPECT_TRUE(c1.is_done());
  EXPECT_TRUE(c2.is_done());
  EXPECT_TRUE(c3.is_done());

  EXPECT_THAT(c1.error(), IsOk());
  EXPECT_THAT(c2.error(), IsOk());
  EXPECT_THAT(c3.error(), IsOk());
}

TEST_F(HttpCacheTest, ValidationResultsIn200) {
  MockHttpCache cache;

  // This test ensures that a conditional request, which results in a 200
  // instead of a 304, properly truncates the existing response data.

  // write to the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kETagGET_Transaction);
  transaction.load_flags |= LOAD_VALIDATE_CACHE;
  RunTransactionTest(cache.http_cache(), transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);
}

TEST_F(HttpCacheTest, CachedRedirect) {
  MockHttpCache cache;

  ScopedMockTransaction kTestTransaction(kSimpleGET_Transaction);
  kTestTransaction.status = "HTTP/1.1 301 Moved Permanently";
  kTestTransaction.response_headers = "Location: http://www.bar.com/\n";

  MockHttpRequest request(kTestTransaction);
  TestCompletionCallback callback;

  // Write to the cache.
  {
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    if (rv == ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    ASSERT_THAT(rv, IsOk());

    const HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(nullptr, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Mark the transaction as completed so it is cached.
    trans->DoneReading();

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Active entries in the cache are not retired synchronously. Make
  // sure the next run hits the MockHttpCache and open_count is
  // correct.
  base::RunLoop().RunUntilIdle();

  // Read from the cache.
  {
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    if (rv == ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    ASSERT_THAT(rv, IsOk());

    const HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(nullptr, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Mark the transaction as completed so it is cached.
    trans->DoneReading();

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Verify that no-cache resources are stored in cache, but are not fetched from
// cache during normal loads.
TEST_F(HttpCacheTest, CacheControlNoCacheNormalLoad) {
  for (bool use_memory_entry_data : {false, true}) {
    MockHttpCache cache;
    cache.disk_cache()->set_support_in_memory_entry_data(use_memory_entry_data);

    ScopedMockTransaction transaction(kSimpleGET_Transaction);
    transaction.response_headers = "cache-control: no-cache\n";

    // Initial load.
    RunTransactionTest(cache.http_cache(), transaction);

    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());

    // Try loading again; it should result in a network fetch.
    RunTransactionTest(cache.http_cache(), transaction);

    EXPECT_EQ(2, cache.network_layer()->transaction_count());
    if (use_memory_entry_data) {
      EXPECT_EQ(0, cache.disk_cache()->open_count());
      EXPECT_EQ(2, cache.disk_cache()->create_count());
    } else {
      EXPECT_EQ(1, cache.disk_cache()->open_count());
      EXPECT_EQ(1, cache.disk_cache()->create_count());
    }

    disk_cache::Entry* entry;
    MockHttpRequest request(transaction);
    EXPECT_TRUE(cache.OpenBackendEntry(request.CacheKey(), &entry));
    entry->Close();
  }
}

// Verify that no-cache resources are stored in cache and fetched from cache
// when the LOAD_SKIP_CACHE_VALIDATION flag is set.
TEST_F(HttpCacheTest, CacheControlNoCacheHistoryLoad) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "cache-control: no-cache\n";

  // Initial load.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Try loading again with LOAD_SKIP_CACHE_VALIDATION.
  transaction.load_flags = LOAD_SKIP_CACHE_VALIDATION;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  MockHttpRequest request(transaction);
  EXPECT_TRUE(cache.OpenBackendEntry(request.CacheKey(), &entry));
  entry->Close();
}

TEST_F(HttpCacheTest, CacheControlNoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "cache-control: no-store\n";

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  MockHttpRequest request(transaction);
  EXPECT_FALSE(cache.OpenBackendEntry(request.CacheKey(), &entry));
}

TEST_F(HttpCacheTest, CacheControlNoStore2) {
  // this test is similar to the above test, except that the initial response
  // is cachable, but when it is validated, no-store is received causing the
  // cached document to be deleted.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  MockHttpRequest request(transaction);
  EXPECT_FALSE(cache.OpenBackendEntry(request.CacheKey(), &entry));
}

TEST_F(HttpCacheTest, CacheControlNoStore3) {
  // this test is similar to the above test, except that the response is a 304
  // instead of a 200.  this should never happen in practice, but it seems like
  // a good thing to verify that we still destroy the cache entry.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  transaction.status = "HTTP/1.1 304 Not Modified";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  MockHttpRequest request(transaction);
  EXPECT_FALSE(cache.OpenBackendEntry(request.CacheKey(), &entry));
}

// Ensure that we don't cache requests served over bad HTTPS.
TEST_F(HttpCacheSimpleGetTest, SSLError) {
  MockHttpCache cache;

  MockTransaction transaction = kSimpleGET_Transaction;
  transaction.cert_status = CERT_STATUS_REVOKED;
  ScopedMockTransaction scoped_transaction(transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  // Test that it was not cached.
  transaction.load_flags |= LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

  int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
  }
  ASSERT_THAT(rv, IsError(ERR_CACHE_MISS));
}

// Ensure that we don't crash by if left-behind transactions.
TEST_F(HttpCacheTest, OutlivedTransactions) {
  auto cache = std::make_unique<MockHttpCache>();

  std::unique_ptr<HttpTransaction> trans;
  EXPECT_THAT(cache->CreateTransaction(&trans), IsOk());

  cache.reset();
  trans.reset();
}

// Test that the disabled mode works.
TEST_F(HttpCacheTest, CacheDisabledMode) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // go into disabled mode
  cache.http_cache()->set_mode(HttpCache::DISABLE);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Other tests check that the response headers of the cached response
// get updated on 304. Here we specifically check that the
// HttpResponseHeaders::request_time and HttpResponseHeaders::response_time
// fields also gets updated.
// http://crbug.com/20594.
TEST_F(HttpCacheTest, UpdatesRequestResponseTimeOn304) {
  MockHttpCache cache;

  const char kUrl[] = "http://foobar";
  const char kData[] = "body";

  ScopedMockTransaction mock_network_response(kUrl);

  // Request |kUrl|, causing |kNetResponse1| to be written to the cache.

  MockTransaction request = {nullptr};
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = "\r\n";
  request.data = kData;

  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      kData};

  kNetResponse1.AssignTo(&mock_network_response);

  RunTransactionTest(cache.http_cache(), request);

  // Request |kUrl| again, this time validating the cache and getting
  // a 304 back.

  request.load_flags = LOAD_VALIDATE_CACHE;

  static const Response kNetResponse2 = {
      "HTTP/1.1 304 Not Modified", "Date: Wed, 22 Jul 2009 03:15:26 GMT\n", ""};

  kNetResponse2.AssignTo(&mock_network_response);

  base::Time request_time = base::Time() + base::Hours(1234);
  base::Time response_time = base::Time() + base::Hours(1235);

  mock_network_response.request_time = request_time;
  mock_network_response.response_time = response_time;

  HttpResponseInfo response;
  RunTransactionTestWithResponseInfo(cache.http_cache(), request, &response);

  // The request and response times should have been updated.
  EXPECT_EQ(request_time.ToInternalValue(),
            response.request_time.ToInternalValue());
  EXPECT_EQ(response_time.ToInternalValue(),
            response.response_time.ToInternalValue());

  EXPECT_EQ(
      "HTTP/1.1 200 OK\n"
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      ToSimpleString(response.headers));
}

TEST_F(HttpCacheTestSplitCacheFeatureEnabled,
       SplitCacheWithNetworkIsolationKey) {
  MockHttpCache cache;
  HttpResponseInfo response;

  SchemefulSite site_a(GURL("http://a.com"));
  SchemefulSite site_b(GURL("http://b.com"));
  SchemefulSite site_data(GURL("data:text/html,<body>Hello World</body>"));

  MockHttpRequest trans_info = MockHttpRequest(kSimpleGET_Transaction);
  // Request with a.com as the top frame and subframe origins. This should
  // result in a cache miss.
  trans_info.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // The second request should result in a cache hit.
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  // Now request with b.com as the subframe origin. It should result in a cache
  // miss.
  trans_info.network_isolation_key = NetworkIsolationKey(site_a, site_b);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateCrossSite(site_a);
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // The second request should result in a cache hit.
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  // Another request with a.com as the top frame and subframe origin should
  // still result in a cache hit.
  trans_info.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  // Now make a request with an opaque subframe site. It shouldn't cause
  // anything to be added to the cache because the NIK makes use of the frame
  // site.
  trans_info.network_isolation_key = NetworkIsolationKey(site_b, site_data);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateCrossSite(site_b);
  EXPECT_EQ(std::nullopt, trans_info.network_isolation_key.ToCacheKeyString());

  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // On the second request, expect a cache miss since the NIK uses the frame
  // site.
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // Verify that a post transaction with a data stream uses a separate key.
  const int64_t kUploadId = 1;  // Just a dummy value.

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers),
                                              kUploadId);

  MockHttpRequest post_info = MockHttpRequest(kSimplePOST_Transaction);
  post_info.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  post_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);
  post_info.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), kSimplePOST_Transaction,
                                post_info, &response);
  EXPECT_FALSE(response.was_cached);
}

TEST_F(HttpCacheTest, HttpCacheProfileThirdPartyCSS) {
  base::HistogramTester histograms;
  MockHttpCache cache;
  HttpResponseInfo response;

  url::Origin origin_a = url::Origin::Create(GURL(kSimpleGET_Transaction.url));
  url::Origin origin_b = url::Origin::Create(GURL("http://b.com"));
  SchemefulSite site_a(origin_a);
  SchemefulSite site_b(origin_b);

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "Content-Type: text/css\n";

  MockHttpRequest trans_info = MockHttpRequest(transaction);

  // Requesting with the same top-frame site should not count as third-party
  // but should still be recorded as CSS
  trans_info.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);
  trans_info.possibly_top_frame_origin = origin_a;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, trans_info,
                                &response);

  histograms.ExpectTotalCount("HttpCache.Pattern", 1);
  histograms.ExpectTotalCount("HttpCache.Pattern.CSS", 1);
  histograms.ExpectTotalCount("HttpCache.Pattern.CSSThirdParty", 0);

  // Requesting with a different top-frame site should count as third-party
  // and recorded as CSS
  trans_info.network_isolation_key = NetworkIsolationKey(site_b, site_b);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_b);
  trans_info.possibly_top_frame_origin = origin_b;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, trans_info,
                                &response);
  histograms.ExpectTotalCount("HttpCache.Pattern", 2);
  histograms.ExpectTotalCount("HttpCache.Pattern.CSS", 2);
  histograms.ExpectTotalCount("HttpCache.Pattern.CSSThirdParty", 1);
}

TEST_F(HttpCacheTest, HttpCacheProfileThirdPartyJavaScript) {
  base::HistogramTester histograms;
  MockHttpCache cache;
  HttpResponseInfo response;

  url::Origin origin_a = url::Origin::Create(GURL(kSimpleGET_Transaction.url));
  url::Origin origin_b = url::Origin::Create(GURL("http://b.com"));
  SchemefulSite site_a(origin_a);
  SchemefulSite site_b(origin_b);

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "Content-Type: application/javascript\n";

  MockHttpRequest trans_info = MockHttpRequest(transaction);

  // Requesting with the same top-frame site should not count as third-party
  // but should still be recorded as JavaScript
  trans_info.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);
  trans_info.possibly_top_frame_origin = origin_a;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, trans_info,
                                &response);

  histograms.ExpectTotalCount("HttpCache.Pattern", 1);
  histograms.ExpectTotalCount("HttpCache.Pattern.JavaScript", 1);
  histograms.ExpectTotalCount("HttpCache.Pattern.JavaScriptThirdParty", 0);

  // Requesting with a different top-frame site should count as third-party
  // and recorded as JavaScript
  trans_info.network_isolation_key = NetworkIsolationKey(site_b, site_b);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_b);
  trans_info.possibly_top_frame_origin = origin_b;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, trans_info,
                                &response);
  histograms.ExpectTotalCount("HttpCache.Pattern", 2);
  histograms.ExpectTotalCount("HttpCache.Pattern.JavaScript", 2);
  histograms.ExpectTotalCount("HttpCache.Pattern.JavaScriptThirdParty", 1);
}

TEST_F(HttpCacheTest, HttpCacheProfileThirdPartyFont) {
  base::HistogramTester histograms;
  MockHttpCache cache;
  HttpResponseInfo response;

  url::Origin origin_a = url::Origin::Create(GURL(kSimpleGET_Transaction.url));
  url::Origin origin_b = url::Origin::Create(GURL("http://b.com"));
  SchemefulSite site_a(origin_a);
  SchemefulSite site_b(origin_b);

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "Content-Type: font/otf\n";

  MockHttpRequest trans_info = MockHttpRequest(transaction);

  // Requesting with the same top-frame site should not count as third-party
  // but should still be recorded as a font
  trans_info.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);
  trans_info.possibly_top_frame_origin = origin_a;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, trans_info,
                                &response);

  histograms.ExpectTotalCount("HttpCache.Pattern", 1);
  histograms.ExpectTotalCount("HttpCache.Pattern.Font", 1);
  histograms.ExpectTotalCount("HttpCache.Pattern.FontThirdParty", 0);

  // Requesting with a different top-frame site should count as third-party
  // and recorded as a font
  trans_info.network_isolation_key = NetworkIsolationKey(site_b, site_b);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_b);
  trans_info.possibly_top_frame_origin = origin_b;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, trans_info,
                                &response);
  histograms.ExpectTotalCount("HttpCache.Pattern", 2);
  histograms.ExpectTotalCount("HttpCache.Pattern.Font", 2);
  histograms.ExpectTotalCount("HttpCache.Pattern.FontThirdParty", 1);
}

TEST_P(HttpCacheTestSplitCacheFeature, SplitCache) {
  if (!IsSplitCacheEnabled()) {
    GTEST_SKIP() << "This test is relevant only with SplitCache.";
  }
  MockHttpCache cache;
  HttpResponseInfo response;

  const SchemefulSite site_a(GURL("http://a.com"));
  const url::Origin origin_b = url::Origin::Create(GURL("http://b.com"));
  const SchemefulSite site_b(origin_b);
  const SchemefulSite site_data(
      GURL("data:text/html,<body>Hello World</body>"));

  // A request without a top frame origin shouldn't result in anything being
  // added to the cache.
  MockHttpRequest trans_info = MockHttpRequest(kSimpleGET_Transaction);
  trans_info.network_isolation_key = NetworkIsolationKey();
  trans_info.network_anonymization_key = NetworkAnonymizationKey();
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // Now request with a.com as the top frame origin. This should initially
  // result in a cache miss since the cached resource has a different top frame
  // origin.
  NetworkIsolationKey key_a(site_a, site_a);
  auto nak_a = NetworkAnonymizationKey::CreateSameSite(site_a);
  trans_info.network_isolation_key = key_a;
  trans_info.network_anonymization_key = nak_a;
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // The second request should result in a cache hit.
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  // If the same resource with the same NIK is for a subframe document resource,
  // it should not be a cache hit.
  MockHttpRequest subframe_document_trans_info = trans_info;
  subframe_document_trans_info.is_subframe_document_resource = true;
  switch (GetParam()) {
    case SplitCacheTestCase::kDisabled:
      NOTREACHED_NORETURN();
    case SplitCacheTestCase::kEnabledTripleKeyed:
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
      // The `is_subframe_document_resource` being true is enough to cause a
      // different cache partition to be used.
      break;
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      // The `is_subframe_document_resource` bit is not used, in favor of using
      // the request initiator. Note that with this partitioning scheme a
      // navigation and a resource will share a cache partition if the
      // navigation has a same-site initiator, so for this test set a cross-site
      // initiator.
      subframe_document_trans_info.initiator = origin_b;
      break;
  }
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                subframe_document_trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // Same request again should be a cache hit.
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                subframe_document_trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  // Now request with b.com as the top frame origin. It should be a cache miss.
  trans_info.network_isolation_key = NetworkIsolationKey(site_b, site_b);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_b);
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // The second request should be a cache hit.
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  // Another request for a.com should still result in a cache hit.
  trans_info.network_isolation_key = key_a;
  trans_info.network_anonymization_key = nak_a;
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  // Now make a request with an opaque top frame origin. It shouldn't result in
  // a cache hit.
  trans_info.network_isolation_key = NetworkIsolationKey(site_data, site_data);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_data);
  EXPECT_EQ(std::nullopt, trans_info.network_isolation_key.ToCacheKeyString());
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // On the second request, it still shouldn't result in a cache hit.
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // Verify that a post transaction with a data stream uses a separate key.
  const int64_t kUploadId = 1;  // Just a dummy value.

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers),
                                              kUploadId);

  MockHttpRequest post_info = MockHttpRequest(kSimplePOST_Transaction);
  post_info.network_isolation_key = NetworkIsolationKey(site_a, site_a);
  post_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site_a);
  post_info.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), kSimplePOST_Transaction,
                                post_info, &response);
  EXPECT_FALSE(response.was_cached);
}

TEST_P(HttpCacheTestSplitCacheFeature, GenerateCacheKeyForRequestFailures) {
  GURL url("http://example.com");
  SchemefulSite site(url);

  HttpRequestInfo cacheable_request;
  cacheable_request.url = url;
  cacheable_request.method = "GET";
  cacheable_request.network_isolation_key = NetworkIsolationKey(site, site);
  cacheable_request.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(site);
  std::optional<std::string> cache_key =
      HttpCache::GenerateCacheKeyForRequest(&cacheable_request);
  EXPECT_NE(std::nullopt, cache_key);

  // Should return false for a request corresponding to an opaque origin
  // context.
  const SchemefulSite site_data(
      GURL("data:text/html,<body>Hello World</body>"));
  HttpRequestInfo opaque_top_level_site_request = cacheable_request;
  opaque_top_level_site_request.network_isolation_key =
      NetworkIsolationKey(site_data, site);
  opaque_top_level_site_request.network_anonymization_key =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          opaque_top_level_site_request.network_isolation_key);
  bool is_request_cacheable;
  switch (GetParam()) {
    case SplitCacheTestCase::kDisabled:
      is_request_cacheable = true;
      break;
    case SplitCacheTestCase::kEnabledTripleKeyed:
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      is_request_cacheable = false;
      break;
  }
  cache_key =
      HttpCache::GenerateCacheKeyForRequest(&opaque_top_level_site_request);
  EXPECT_EQ(is_request_cacheable, cache_key.has_value());

  // A renderer-initiated main frame navigation from an opaque origin context
  // should not be cacheable if the HTTP cache partitioning scheme uses the
  // initiator in the key.
  HttpRequestInfo opaque_initiator_main_frame_request = cacheable_request;
  opaque_initiator_main_frame_request.is_main_frame_navigation = true;
  opaque_initiator_main_frame_request.initiator = url::Origin();

  switch (GetParam()) {
    case SplitCacheTestCase::kDisabled:
    case SplitCacheTestCase::kEnabledTripleKeyed:
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
      is_request_cacheable = true;
      break;
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      is_request_cacheable = false;
      break;
  }
  cache_key = HttpCache::GenerateCacheKeyForRequest(
      &opaque_initiator_main_frame_request);
  EXPECT_EQ(is_request_cacheable, cache_key.has_value());

  // Same as above but for a renderer-initiated subframe navigation.
  HttpRequestInfo opaque_initiator_subframe_request = cacheable_request;
  opaque_initiator_subframe_request.is_subframe_document_resource = true;
  opaque_initiator_subframe_request.initiator = url::Origin();

  switch (GetParam()) {
    case SplitCacheTestCase::kDisabled:
    case SplitCacheTestCase::kEnabledTripleKeyed:
    case SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool:
    case SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator:
      is_request_cacheable = true;
      break;
    case SplitCacheTestCase::kEnabledTriplePlusNavInitiator:
      is_request_cacheable = false;
      break;
  }
  cache_key =
      HttpCache::GenerateCacheKeyForRequest(&opaque_initiator_subframe_request);
  EXPECT_EQ(is_request_cacheable, cache_key.has_value());
}
TEST_F(HttpCacheTest, SplitCacheEnabledByDefault) {
  HttpCache::ClearGlobalsForTesting();
  HttpCache::SplitCacheFeatureEnableByDefault();
  EXPECT_TRUE(HttpCache::IsSplitCacheEnabled());

  MockHttpCache cache;
  HttpResponseInfo response;

  SchemefulSite site_a(GURL("http://a.com"));
  SchemefulSite site_b(GURL("http://b.com"));
  MockHttpRequest trans_info = MockHttpRequest(kSimpleGET_Transaction);
  NetworkIsolationKey key_a(site_a, site_a);
  auto nak_a = NetworkAnonymizationKey::CreateSameSite(site_a);
  trans_info.network_isolation_key = key_a;
  trans_info.network_anonymization_key = nak_a;
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // Subsequent requests with the same NIK and different NIK will be a cache hit
  // and miss respectively.
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  NetworkIsolationKey key_b(site_b, site_b);
  auto nak_b = NetworkAnonymizationKey::CreateSameSite(site_b);
  trans_info.network_isolation_key = key_b;
  trans_info.network_anonymization_key = nak_b;
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);
}

TEST_F(HttpCacheTest, SplitCacheEnabledByDefaultButOverridden) {
  HttpCache::ClearGlobalsForTesting();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kSplitCacheByNetworkIsolationKey);

  // Enabling it here should have no effect as it is already overridden.
  HttpCache::SplitCacheFeatureEnableByDefault();
  EXPECT_FALSE(HttpCache::IsSplitCacheEnabled());
}

TEST_F(HttpCacheTestSplitCacheFeatureEnabled, SplitCacheUsesRegistrableDomain) {
  MockHttpCache cache;
  HttpResponseInfo response;
  MockHttpRequest trans_info = MockHttpRequest(kSimpleGET_Transaction);

  SchemefulSite site_a(GURL("http://a.foo.com"));
  SchemefulSite site_b(GURL("http://b.foo.com"));

  NetworkIsolationKey key_a(site_a, site_a);
  auto nak_a = NetworkAnonymizationKey::CreateSameSite(site_a);
  trans_info.network_isolation_key = key_a;
  trans_info.network_anonymization_key = nak_a;
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // The second request with a different origin but the same registrable domain
  // should be a cache hit.
  NetworkIsolationKey key_b(site_b, site_b);
  auto nak_b = NetworkAnonymizationKey::CreateSameSite(site_b);
  trans_info.network_isolation_key = key_b;
  trans_info.network_anonymization_key = nak_b;
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  // Request with a different registrable domain. It should be a cache miss.
  SchemefulSite new_site_a(GURL("http://a.bar.com"));
  NetworkIsolationKey new_key_a(new_site_a, new_site_a);
  auto new_nak_a = NetworkAnonymizationKey::CreateSameSite(new_site_a);
  trans_info.network_isolation_key = new_key_a;
  trans_info.network_anonymization_key = new_nak_a;
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);
}

TEST_F(HttpCacheTest, NonSplitCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kSplitCacheByNetworkIsolationKey);

  MockHttpCache cache;
  HttpResponseInfo response;

  // A request without a top frame is added to the cache normally.
  MockHttpRequest trans_info = MockHttpRequest(kSimpleGET_Transaction);
  trans_info.network_isolation_key = NetworkIsolationKey();
  trans_info.network_anonymization_key = NetworkAnonymizationKey();
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_FALSE(response.was_cached);

  // The second request should result in a cache hit.
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);

  // Now request with a.com as the top frame origin. The same cached object
  // should be used.
  const SchemefulSite kSiteA(GURL("http://a.com/"));
  trans_info.network_isolation_key = NetworkIsolationKey(kSiteA, kSiteA);
  trans_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(kSiteA);
  RunTransactionTestWithRequest(cache.http_cache(), kSimpleGET_Transaction,
                                trans_info, &response);
  EXPECT_TRUE(response.was_cached);
}

TEST_F(HttpCacheTest, SkipVaryCheck) {
  MockHttpCache cache;

  // Write a simple vary transaction to the cache.
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "accept-encoding: gzip\r\n";
  transaction.response_headers =
      "Vary: accept-encoding\n"
      "Cache-Control: max-age=10000\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Change the request headers so that the request doesn't match due to vary.
  // The request should fail.
  transaction.load_flags = LOAD_ONLY_FROM_CACHE;
  transaction.request_headers = "accept-encoding: foo\r\n";
  transaction.start_return_code = ERR_CACHE_MISS;
  RunTransactionTest(cache.http_cache(), transaction);

  // Change the load flags to ignore vary checks, the request should now hit.
  transaction.load_flags = LOAD_ONLY_FROM_CACHE | LOAD_SKIP_VARY_CHECK;
  transaction.start_return_code = OK;
  RunTransactionTest(cache.http_cache(), transaction);
}

TEST_F(HttpCacheTest, SkipVaryCheckStar) {
  MockHttpCache cache;

  // Write a simple vary:* transaction to the cache.
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "accept-encoding: gzip\r\n";
  transaction.response_headers =
      "Vary: *\n"
      "Cache-Control: max-age=10000\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // The request shouldn't match even with the same request headers due to the
  // Vary: *. The request should fail.
  transaction.load_flags = LOAD_ONLY_FROM_CACHE;
  transaction.start_return_code = ERR_CACHE_MISS;
  RunTransactionTest(cache.http_cache(), transaction);

  // Change the load flags to ignore vary checks, the request should now hit.
  transaction.load_flags = LOAD_ONLY_FROM_CACHE | LOAD_SKIP_VARY_CHECK;
  transaction.start_return_code = OK;
  RunTransactionTest(cache.http_cache(), transaction);
}

// Tests that we only return valid entries with LOAD_ONLY_FROM_CACHE
// transactions unless LOAD_SKIP_CACHE_VALIDATION is set.
TEST_F(HttpCacheTest, ValidLoadOnlyFromCache) {
  MockHttpCache cache;
  base::SimpleTestClock clock;
  cache.http_cache()->SetClockForTesting(&clock);
  cache.network_layer()->SetClock(&clock);

  // Write a resource that will expire in 100 seconds.
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "Cache-Control: max-age=100\n";
  RunTransactionTest(cache.http_cache(), transaction);

  // Move forward in time such that the cached response is no longer valid.
  clock.Advance(base::Seconds(101));

  // Skipping cache validation should still return a response.
  transaction.load_flags = LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION;
  RunTransactionTest(cache.http_cache(), transaction);

  // If the cache entry is checked for validitiy, it should fail.
  transaction.load_flags = LOAD_ONLY_FROM_CACHE;
  transaction.start_return_code = ERR_CACHE_MISS;
  RunTransactionTest(cache.http_cache(), transaction);
}

TEST_F(HttpCacheTest, InvalidLoadFlagCombination) {
  MockHttpCache cache;

  // Put the resource in the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // Now try to fetch it again, but with a flag combination disallowing both
  // cache and network access.
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  // DevTools relies on this combination of flags for "disable cache" mode
  // when a resource is only supposed to be loaded from cache.
  transaction.load_flags = LOAD_ONLY_FROM_CACHE | LOAD_BYPASS_CACHE;
  transaction.start_return_code = ERR_CACHE_MISS;
  RunTransactionTest(cache.http_cache(), transaction);
}

// Tests that we don't mark entries as truncated when a filter detects the end
// of the stream.
TEST_F(HttpCacheTest, FilterCompletion) {
  MockHttpCache cache;
  TestCompletionCallback callback;

  {
    MockHttpRequest request(kSimpleGET_Transaction);
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());

    auto buf = base::MakeRefCounted<IOBufferWithSize>(256);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);

    // Now make sure that the entry is preserved.
    trans->DoneReading();
  }

  // Make sure that the ActiveEntry is gone.
  base::RunLoop().RunUntilIdle();

  // Read from the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we don't mark entries as truncated and release the cache
// entry when DoneReading() is called before any Read() calls, such as
// for a redirect.
TEST_F(HttpCacheTest, DoneReading) {
  MockHttpCache cache;
  TestCompletionCallback callback;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.data = "";
  MockHttpRequest request(transaction);

  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

  int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
  EXPECT_THAT(callback.GetResult(rv), IsOk());

  trans->DoneReading();
  // Leave the transaction around.

  // Make sure that the ActiveEntry is gone.
  base::RunLoop().RunUntilIdle();

  // Read from the cache. This should not deadlock.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we stop caching when told.
TEST_F(HttpCacheTest, StopCachingDeletesEntry) {
  MockHttpCache cache;
  TestCompletionCallback callback;
  MockHttpRequest request(kSimpleGET_Transaction);

  {
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());

    auto buf = base::MakeRefCounted<IOBufferWithSize>(256);
    rv = trans->Read(buf.get(), 10, callback.callback());
    EXPECT_EQ(10, callback.GetResult(rv));

    trans->StopCaching();

    // We should be able to keep reading.
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_EQ(0, callback.GetResult(rv));
  }

  // Make sure that the ActiveEntry is gone.
  base::RunLoop().RunUntilIdle();

  // Verify that the entry is gone.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we stop caching when told, even if DoneReading is called
// after StopCaching.
TEST_F(HttpCacheTest, StopCachingThenDoneReadingDeletesEntry) {
  MockHttpCache cache;
  TestCompletionCallback callback;
  MockHttpRequest request(kSimpleGET_Transaction);

  {
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());

    auto buf = base::MakeRefCounted<IOBufferWithSize>(256);
    rv = trans->Read(buf.get(), 10, callback.callback());
    EXPECT_EQ(10, callback.GetResult(rv));

    trans->StopCaching();

    // We should be able to keep reading.
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_EQ(0, callback.GetResult(rv));

    // We should be able to call DoneReading.
    trans->DoneReading();
  }

  // Make sure that the ActiveEntry is gone.
  base::RunLoop().RunUntilIdle();

  // Verify that the entry is gone.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we stop caching when told, when using auth.
TEST_F(HttpCacheTest, StopCachingWithAuthDeletesEntry) {
  MockHttpCache cache;
  TestCompletionCallback callback;
  ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
  mock_transaction.status = "HTTP/1.1 401 Unauthorized";
  MockHttpRequest request(mock_transaction);

  {
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());

    trans->StopCaching();
  }

  // Make sure that the ActiveEntry is gone.
  base::RunLoop().RunUntilIdle();

  // Verify that the entry is gone.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that when we are told to stop caching we don't throw away valid data.
TEST_F(HttpCacheTest, StopCachingSavesEntry) {
  MockHttpCache cache;
  TestCompletionCallback callback;
  MockHttpRequest request(kSimpleGET_Transaction);

  {
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    // Force a response that can be resumed.
    ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
    mock_transaction.response_headers =
        "Cache-Control: max-age=10000\n"
        "Content-Length: 42\n"
        "Etag: \"foo\"\n";

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());

    auto buf = base::MakeRefCounted<IOBufferWithSize>(256);
    rv = trans->Read(buf.get(), 10, callback.callback());
    EXPECT_EQ(callback.GetResult(rv), 10);

    trans->StopCaching();

    // We should be able to keep reading.
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_EQ(callback.GetResult(rv), 0);
  }

  // Verify that the entry is marked as incomplete.
  // VerifyTruncatedFlag(&cache, kSimpleGET_Transaction.url, true, 0);
  // Verify that the entry is doomed.
  cache.disk_cache()->IsDiskEntryDoomed(request.CacheKey());
}

// Tests that we handle truncated enries when StopCaching is called.
TEST_F(HttpCacheTest, StopCachingTruncatedEntry) {
  MockHttpCache cache;
  TestCompletionCallback callback;
  MockHttpRequest request(kRangeGET_TransactionOK);
  request.extra_headers.Clear();
  request.extra_headers.AddHeaderFromString(EXTRA_HEADER_LINE);
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  {
    // Now make a regular request.
    std::unique_ptr<HttpTransaction> trans;
    ASSERT_THAT(cache.CreateTransaction(&trans), IsOk());

    int rv = trans->Start(&request, callback.callback(), NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());

    auto buf = base::MakeRefCounted<IOBufferWithSize>(256);
    rv = trans->Read(buf.get(), 10, callback.callback());
    EXPECT_EQ(callback.GetResult(rv), 10);

    // This is actually going to do nothing.
    trans->StopCaching();

    // We should be able to keep reading.
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_EQ(callback.GetResult(rv), 0);
  }

  // Verify that the disk entry was updated.
  VerifyTruncatedFlag(&cache, request.CacheKey(), false, 80);
}

namespace {

enum class TransactionPhase {
  BEFORE_FIRST_READ,
  AFTER_FIRST_READ,
  AFTER_NETWORK_READ
};

using CacheInitializer = void (*)(MockHttpCache*);
using HugeCacheTestConfiguration =
    std::pair<TransactionPhase, CacheInitializer>;

class HttpCacheHugeResourceTest
    : public ::testing::TestWithParam<HugeCacheTestConfiguration>,
      public WithTaskEnvironment {
 public:
  static std::list<HugeCacheTestConfiguration> GetTestModes();
  static std::list<HugeCacheTestConfiguration> kTestModes;

  // CacheInitializer callbacks. These are used to initialize the cache
  // depending on the test run configuration.

  // Initializes a cache containing a truncated entry containing the first 20
  // bytes of the reponse body.
  static void SetupTruncatedCacheEntry(MockHttpCache* cache);

  // Initializes a cache containing a sparse entry. The first 10 bytes are
  // present in the cache.
  static void SetupPrefixSparseCacheEntry(MockHttpCache* cache);

  // Initializes a cache containing a sparse entry. The 10 bytes at offset
  // 99990 are present in the cache.
  static void SetupInfixSparseCacheEntry(MockHttpCache* cache);

 protected:
  static void LargeResourceTransactionHandler(const HttpRequestInfo* request,
                                              std::string* response_status,
                                              std::string* response_headers,
                                              std::string* response_data);
  static int LargeBufferReader(int64_t content_length,
                               int64_t offset,
                               IOBuffer* buf,
                               int buf_len);

  static void SetFlagOnBeforeNetworkStart(bool* started, bool* /* defer */);

  // Size of resource to be tested.
  static const int64_t kTotalSize = 5000LL * 1000 * 1000;
};

const int64_t HttpCacheHugeResourceTest::kTotalSize;

// static
void HttpCacheHugeResourceTest::LargeResourceTransactionHandler(
    const HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  std::optional<std::string> if_range =
      request->extra_headers.GetHeader(HttpRequestHeaders::kIfRange);
  if (!if_range) {
    // If there were no range headers in the request, we are going to just
    // return the entire response body.
    *response_status = "HTTP/1.1 200 Success";
    *response_headers = base::StringPrintf("Content-Length: %" PRId64
                                           "\n"
                                           "ETag: \"foo\"\n"
                                           "Accept-Ranges: bytes\n",
                                           kTotalSize);
    return;
  }

  // From this point on, we should be processing a valid byte-range request.
  EXPECT_EQ("\"foo\"", *if_range);

  std::string range_header =
      request->extra_headers.GetHeader(HttpRequestHeaders::kRange).value();
  std::vector<HttpByteRange> ranges;

  EXPECT_TRUE(HttpUtil::ParseRangeHeader(range_header, &ranges));
  ASSERT_EQ(1u, ranges.size());

  HttpByteRange range = ranges[0];
  EXPECT_TRUE(range.HasFirstBytePosition());
  int64_t last_byte_position =
      range.HasLastBytePosition() ? range.last_byte_position() : kTotalSize - 1;

  *response_status = "HTTP/1.1 206 Partial";
  *response_headers = base::StringPrintf(
      "Content-Range: bytes %" PRId64 "-%" PRId64 "/%" PRId64
      "\n"
      "Content-Length: %" PRId64 "\n",
      range.first_byte_position(), last_byte_position, kTotalSize,
      last_byte_position - range.first_byte_position() + 1);
}

// static
int HttpCacheHugeResourceTest::LargeBufferReader(int64_t content_length,
                                                 int64_t offset,
                                                 IOBuffer* buf,
                                                 int buf_len) {
  // This test involves reading multiple gigabytes of data. To make it run in a
  // reasonable amount of time, we are going to skip filling the buffer with
  // data. Instead the test relies on verifying that the count of bytes expected
  // at the end is correct.
  EXPECT_LT(0, content_length);
  EXPECT_LE(offset, content_length);
  int num = std::min(static_cast<int64_t>(buf_len), content_length - offset);
  return num;
}

// static
void HttpCacheHugeResourceTest::SetFlagOnBeforeNetworkStart(bool* started,
                                                            bool* /* defer */) {
  *started = true;
}

// static
void HttpCacheHugeResourceTest::SetupTruncatedCacheEntry(MockHttpCache* cache) {
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);
  std::string cached_headers = base::StringPrintf(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: %" PRId64 "\n",
      kTotalSize);
  CreateTruncatedEntry(cached_headers, cache);
}

// static
void HttpCacheHugeResourceTest::SetupPrefixSparseCacheEntry(
    MockHttpCache* cache) {
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = MockTransactionHandler();
  transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Range: bytes 0-9/5000000000\n"
      "Content-Length: 10\n";
  std::string headers;
  RunTransactionTestWithResponse(cache->http_cache(), transaction, &headers);
}

// static
void HttpCacheHugeResourceTest::SetupInfixSparseCacheEntry(
    MockHttpCache* cache) {
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = MockTransactionHandler();
  transaction.request_headers = "Range: bytes = 99990-99999\r\n" EXTRA_HEADER;
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Range: bytes 99990-99999/5000000000\n"
      "Content-Length: 10\n";
  std::string headers;
  RunTransactionTestWithResponse(cache->http_cache(), transaction, &headers);
}

// static
std::list<HugeCacheTestConfiguration>
HttpCacheHugeResourceTest::GetTestModes() {
  std::list<HugeCacheTestConfiguration> test_modes;
  const TransactionPhase kTransactionPhases[] = {
      TransactionPhase::BEFORE_FIRST_READ, TransactionPhase::AFTER_FIRST_READ,
      TransactionPhase::AFTER_NETWORK_READ};
  const CacheInitializer kInitializers[] = {&SetupTruncatedCacheEntry,
                                            &SetupPrefixSparseCacheEntry,
                                            &SetupInfixSparseCacheEntry};

  for (const auto phase : kTransactionPhases) {
    for (const auto initializer : kInitializers) {
      test_modes.emplace_back(phase, initializer);
    }
  }

  return test_modes;
}

// static
std::list<HugeCacheTestConfiguration> HttpCacheHugeResourceTest::kTestModes =
    HttpCacheHugeResourceTest::GetTestModes();

INSTANTIATE_TEST_SUITE_P(
    _,
    HttpCacheHugeResourceTest,
    ::testing::ValuesIn(HttpCacheHugeResourceTest::kTestModes));

}  // namespace

// Test what happens when StopCaching() is called while reading a huge resource
// fetched via GET. Various combinations of cache state and when StopCaching()
// is called is controlled by the parameter passed into the test via the
// INSTANTIATE_TEST_SUITE_P invocation above.
TEST_P(HttpCacheHugeResourceTest,
       StopCachingFollowedByReadForHugeTruncatedResource) {
  // This test is going to be repeated for all combinations of TransactionPhase
  // and CacheInitializers returned by GetTestModes().
  const TransactionPhase stop_caching_phase = GetParam().first;
  const CacheInitializer cache_initializer = GetParam().second;

  MockHttpCache cache;
  (*cache_initializer)(&cache);

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.url = kRangeGET_TransactionOK.url;
  transaction.handler = base::BindRepeating(&LargeResourceTransactionHandler);
  transaction.read_handler = base::BindRepeating(&LargeBufferReader);
  ScopedMockTransaction scoped_transaction(transaction);

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;
  std::unique_ptr<HttpTransaction> http_transaction;
  int rv = cache.http_cache()->CreateTransaction(DEFAULT_PRIORITY,
                                                 &http_transaction);
  ASSERT_EQ(OK, rv);
  ASSERT_TRUE(http_transaction.get());

  bool network_transaction_started = false;
  if (stop_caching_phase == TransactionPhase::AFTER_NETWORK_READ) {
    http_transaction->SetBeforeNetworkStartCallback(base::BindOnce(
        &SetFlagOnBeforeNetworkStart, &network_transaction_started));
  }

  rv = http_transaction->Start(&request, callback.callback(),
                               NetLogWithSource());
  rv = callback.GetResult(rv);
  ASSERT_EQ(OK, rv);

  if (stop_caching_phase == TransactionPhase::BEFORE_FIRST_READ) {
    http_transaction->StopCaching();
  }

  int64_t total_bytes_received = 0;

  EXPECT_EQ(kTotalSize,
            http_transaction->GetResponseInfo()->headers->GetContentLength());
  do {
    // This test simulates reading gigabytes of data. Buffer size is set to 10MB
    // to reduce the number of reads and speed up the test.
    const int kBufferSize = 1024 * 1024 * 10;
    scoped_refptr<IOBuffer> buf =
        base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    rv = http_transaction->Read(buf.get(), kBufferSize, callback.callback());
    rv = callback.GetResult(rv);

    if (stop_caching_phase == TransactionPhase::AFTER_FIRST_READ &&
        total_bytes_received == 0) {
      http_transaction->StopCaching();
    }

    if (rv > 0) {
      total_bytes_received += rv;
    }

    if (network_transaction_started &&
        stop_caching_phase == TransactionPhase::AFTER_NETWORK_READ) {
      http_transaction->StopCaching();
      network_transaction_started = false;
    }
  } while (rv > 0);

  // The only verification we are going to do is that the received resource has
  // the correct size. This is sufficient to verify that the state machine
  // didn't terminate abruptly due to the StopCaching() call.
  EXPECT_EQ(kTotalSize, total_bytes_received);
}

// Tests that we detect truncated resources from the net when there is
// a Content-Length header.
TEST_F(HttpCacheTest, TruncatedByContentLength) {
  MockHttpCache cache;
  TestCompletionCallback callback;

  {
    ScopedMockTransaction transaction(kSimpleGET_Transaction);
    transaction.response_headers =
        "Cache-Control: max-age=10000\n"
        "Content-Length: 100\n";
    RunTransactionTest(cache.http_cache(), transaction);
  }

  // Read from the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we actually flag entries as truncated when we detect an error
// from the net.
TEST_F(HttpCacheTest, TruncatedByContentLength2) {
  MockHttpCache cache;
  TestCompletionCallback callback;

  {
    ScopedMockTransaction transaction(kSimpleGET_Transaction);
    transaction.response_headers =
        "Cache-Control: max-age=10000\n"
        "Content-Length: 100\n"
        "Etag: \"foo\"\n";
    RunTransactionTest(cache.http_cache(), transaction);
  }

  // Verify that the entry is marked as incomplete.
  MockHttpRequest request(kSimpleGET_Transaction);
  VerifyTruncatedFlag(&cache, request.CacheKey(), true, 0);
}

// Make sure that calling SetPriority on a cache transaction passes on
// its priority updates to its underlying network transaction.
TEST_F(HttpCacheTest, SetPriority) {
  MockHttpCache cache;

  HttpRequestInfo info;
  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.http_cache()->CreateTransaction(IDLE, &trans), IsOk());

  // Shouldn't crash, but doesn't do anything either.
  trans->SetPriority(LOW);

  EXPECT_FALSE(cache.network_layer()->last_transaction());
  EXPECT_EQ(DEFAULT_PRIORITY,
            cache.network_layer()->last_create_transaction_priority());

  info.url = GURL(kSimpleGET_Transaction.url);
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            trans->Start(&info, callback.callback(), NetLogWithSource()));

  EXPECT_TRUE(cache.network_layer()->last_transaction());
  if (cache.network_layer()->last_transaction()) {
    EXPECT_EQ(LOW, cache.network_layer()->last_create_transaction_priority());
    EXPECT_EQ(LOW, cache.network_layer()->last_transaction()->priority());
  }

  trans->SetPriority(HIGHEST);

  if (cache.network_layer()->last_transaction()) {
    EXPECT_EQ(LOW, cache.network_layer()->last_create_transaction_priority());
    EXPECT_EQ(HIGHEST, cache.network_layer()->last_transaction()->priority());
  }

  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

// Make sure that calling SetWebSocketHandshakeStreamCreateHelper on a cache
// transaction passes on its argument to the underlying network transaction.
TEST_F(HttpCacheTest, SetWebSocketHandshakeStreamCreateHelper) {
  MockHttpCache cache;
  HttpRequestInfo info;

  FakeWebSocketHandshakeStreamCreateHelper create_helper;
  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.http_cache()->CreateTransaction(IDLE, &trans), IsOk());

  EXPECT_FALSE(cache.network_layer()->last_transaction());

  info.url = GURL(kSimpleGET_Transaction.url);
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            trans->Start(&info, callback.callback(), NetLogWithSource()));

  ASSERT_TRUE(cache.network_layer()->last_transaction());
  EXPECT_FALSE(cache.network_layer()
                   ->last_transaction()
                   ->websocket_handshake_stream_create_helper());
  trans->SetWebSocketHandshakeStreamCreateHelper(&create_helper);
  EXPECT_EQ(&create_helper, cache.network_layer()
                                ->last_transaction()
                                ->websocket_handshake_stream_create_helper());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

// Make sure that a cache transaction passes on its priority to
// newly-created network transactions.
TEST_F(HttpCacheTest, SetPriorityNewTransaction) {
  MockHttpCache cache;
  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  std::string headers;
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = kFullRangeData;

  std::unique_ptr<HttpTransaction> trans;
  ASSERT_THAT(cache.http_cache()->CreateTransaction(MEDIUM, &trans), IsOk());
  EXPECT_EQ(DEFAULT_PRIORITY,
            cache.network_layer()->last_create_transaction_priority());

  MockHttpRequest info(transaction);
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            trans->Start(&info, callback.callback(), NetLogWithSource()));
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_EQ(MEDIUM, cache.network_layer()->last_create_transaction_priority());

  trans->SetPriority(HIGHEST);
  // Should trigger a new network transaction and pick up the new
  // priority.
  ReadAndVerifyTransaction(trans.get(), transaction);

  EXPECT_EQ(HIGHEST, cache.network_layer()->last_create_transaction_priority());
}

namespace {

void RunTransactionAndGetNetworkBytes(MockHttpCache* cache,
                                      const MockTransaction& trans_info,
                                      int64_t* sent_bytes,
                                      int64_t* received_bytes) {
  RunTransactionTestBase(
      cache->http_cache(), trans_info, MockHttpRequest(trans_info), nullptr,
      NetLogWithSource(), nullptr, sent_bytes, received_bytes, nullptr);
}

}  // namespace

TEST_F(HttpCacheTest, NetworkBytesCacheMissAndThenHit) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  int64_t sent, received;
  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(MockNetworkTransaction::kTotalSentBytes, sent);
  EXPECT_EQ(MockNetworkTransaction::kTotalReceivedBytes, received);

  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(0, sent);
  EXPECT_EQ(0, received);
}

TEST_F(HttpCacheTest, NetworkBytesConditionalRequest304) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);
  int64_t sent, received;
  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(MockNetworkTransaction::kTotalSentBytes, sent);
  EXPECT_EQ(MockNetworkTransaction::kTotalReceivedBytes, received);

  transaction.load_flags = LOAD_VALIDATE_CACHE;
  transaction.handler = kETagGetConditionalRequestHandler;
  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(MockNetworkTransaction::kTotalSentBytes, sent);
  EXPECT_EQ(MockNetworkTransaction::kTotalReceivedBytes, received);
}

TEST_F(HttpCacheTest, NetworkBytesConditionalRequest200) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  int64_t sent, received;
  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(MockNetworkTransaction::kTotalSentBytes, sent);
  EXPECT_EQ(MockNetworkTransaction::kTotalReceivedBytes, received);

  RevalidationServer server;
  transaction.handler = server.GetHandlerCallback();

  transaction.request_headers = "Foo: none\r\n";
  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(MockNetworkTransaction::kTotalSentBytes, sent);
  EXPECT_EQ(MockNetworkTransaction::kTotalReceivedBytes, received);
}

TEST_F(HttpCacheTest, NetworkBytesRange) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);

  // Read bytes 40-49 from the network.
  int64_t sent, received;
  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(MockNetworkTransaction::kTotalSentBytes, sent);
  EXPECT_EQ(MockNetworkTransaction::kTotalReceivedBytes, received);

  // Read bytes 40-49 from the cache.
  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(0, sent);
  EXPECT_EQ(0, received);
  base::RunLoop().RunUntilIdle();

  // Read bytes 30-39 from the network.
  transaction.request_headers = "Range: bytes = 30-39\r\n" EXTRA_HEADER;
  transaction.data = "rg: 30-39 ";
  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(MockNetworkTransaction::kTotalSentBytes, sent);
  EXPECT_EQ(MockNetworkTransaction::kTotalReceivedBytes, received);
  base::RunLoop().RunUntilIdle();

  // Read bytes 20-29 and 50-59 from the network, bytes 30-49 from the cache.
  transaction.request_headers = "Range: bytes = 20-59\r\n" EXTRA_HEADER;
  transaction.data = "rg: 20-29 rg: 30-39 rg: 40-49 rg: 50-59 ";
  RunTransactionAndGetNetworkBytes(&cache, transaction, &sent, &received);
  EXPECT_EQ(MockNetworkTransaction::kTotalSentBytes * 2, sent);
  EXPECT_EQ(MockNetworkTransaction::kTotalReceivedBytes * 2, received);
}

class HttpCachePrefetchValidationTest : public TestWithTaskEnvironment {
 protected:
  static const int kNumSecondsPerMinute = 60;
  static const int kMaxAgeSecs = 100;
  static const int kRequireValidationSecs = kMaxAgeSecs + 1;

  HttpCachePrefetchValidationTest() : transaction_(kSimpleGET_Transaction) {
    DCHECK_LT(kMaxAgeSecs, prefetch_reuse_mins() * kNumSecondsPerMinute);

    cache_.http_cache()->SetClockForTesting(&clock_);
    cache_.network_layer()->SetClock(&clock_);

    transaction_.response_headers = "Cache-Control: max-age=100\n";
  }

  bool TransactionRequiredNetwork(int load_flags) {
    int pre_transaction_count = transaction_count();
    transaction_.load_flags = load_flags;
    RunTransactionTest(cache_.http_cache(), transaction_);
    return pre_transaction_count != transaction_count();
  }

  void AdvanceTime(int seconds) { clock_.Advance(base::Seconds(seconds)); }

  int prefetch_reuse_mins() { return HttpCache::kPrefetchReuseMins; }

  // How many times this test has sent requests to the (fake) origin
  // server. Every test case needs to make at least one request to initialise
  // the cache.
  int transaction_count() {
    return cache_.network_layer()->transaction_count();
  }

  MockHttpCache cache_;
  ScopedMockTransaction transaction_;
  std::string response_headers_;
  base::SimpleTestClock clock_;
};

TEST_F(HttpCachePrefetchValidationTest, SkipValidationShortlyAfterPrefetch) {
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_FALSE(TransactionRequiredNetwork(LOAD_NORMAL));
}

TEST_F(HttpCachePrefetchValidationTest, ValidateLongAfterPrefetch) {
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(prefetch_reuse_mins() * kNumSecondsPerMinute);
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_NORMAL));
}

TEST_F(HttpCachePrefetchValidationTest, SkipValidationOnceOnly) {
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_FALSE(TransactionRequiredNetwork(LOAD_NORMAL));
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_NORMAL));
}

TEST_F(HttpCachePrefetchValidationTest, SkipValidationOnceReadOnly) {
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_FALSE(TransactionRequiredNetwork(LOAD_ONLY_FROM_CACHE |
                                          LOAD_SKIP_CACHE_VALIDATION));
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_NORMAL));
}

TEST_F(HttpCachePrefetchValidationTest, BypassCacheOverwritesPrefetch) {
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_BYPASS_CACHE));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_NORMAL));
}

TEST_F(HttpCachePrefetchValidationTest,
       SkipValidationOnExistingEntryThatNeedsValidation) {
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_NORMAL));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_FALSE(TransactionRequiredNetwork(LOAD_NORMAL));
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_NORMAL));
}

TEST_F(HttpCachePrefetchValidationTest,
       SkipValidationOnExistingEntryThatDoesNotNeedValidation) {
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_NORMAL));
  EXPECT_FALSE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_FALSE(TransactionRequiredNetwork(LOAD_NORMAL));
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_NORMAL));
}

TEST_F(HttpCachePrefetchValidationTest, PrefetchMultipleTimes) {
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_PREFETCH));
  EXPECT_FALSE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_FALSE(TransactionRequiredNetwork(LOAD_NORMAL));
}

TEST_F(HttpCachePrefetchValidationTest, ValidateOnDelayedSecondPrefetch) {
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_TRUE(TransactionRequiredNetwork(LOAD_PREFETCH));
  AdvanceTime(kRequireValidationSecs);
  EXPECT_FALSE(TransactionRequiredNetwork(LOAD_NORMAL));
}

TEST_F(HttpCacheTest, StaleContentNotUsedWhenLoadFlagNotSet) {
  MockHttpCache cache;

  ScopedMockTransaction stale_while_revalidate_transaction(
      kSimpleGET_Transaction);

  stale_while_revalidate_transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "Age: 10801\n"
      "Cache-Control: max-age=0,stale-while-revalidate=86400\n";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), stale_while_revalidate_transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Send the request again and check that it is sent to the network again.
  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(
      cache.http_cache(), stale_while_revalidate_transaction, &response_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_FALSE(response_info.async_revalidation_requested);
}

TEST_F(HttpCacheTest, StaleContentUsedWhenLoadFlagSetAndUsableThenTimesout) {
  MockHttpCache cache;
  base::SimpleTestClock clock;
  cache.http_cache()->SetClockForTesting(&clock);
  cache.network_layer()->SetClock(&clock);
  clock.Advance(base::Seconds(10));

  ScopedMockTransaction stale_while_revalidate_transaction(
      kSimpleGET_Transaction);
  stale_while_revalidate_transaction.load_flags |=
      LOAD_SUPPORT_ASYNC_REVALIDATION;
  stale_while_revalidate_transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "Age: 10801\n"
      "Cache-Control: max-age=0,stale-while-revalidate=86400\n";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), stale_while_revalidate_transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Send the request again and check that it is not sent to the network again.
  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(
      cache.http_cache(), stale_while_revalidate_transaction, &response_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_TRUE(response_info.async_revalidation_requested);
  EXPECT_FALSE(response_info.stale_revalidate_timeout.is_null());

  // Move forward in time such that the stale response is no longer valid.
  clock.SetNow(response_info.stale_revalidate_timeout);
  clock.Advance(base::Seconds(1));

  RunTransactionTestWithResponseInfo(
      cache.http_cache(), stale_while_revalidate_transaction, &response_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_FALSE(response_info.async_revalidation_requested);
}

TEST_F(HttpCacheTest, StaleContentUsedWhenLoadFlagSetAndUsable) {
  MockHttpCache cache;
  base::SimpleTestClock clock;
  cache.http_cache()->SetClockForTesting(&clock);
  cache.network_layer()->SetClock(&clock);
  clock.Advance(base::Seconds(10));

  ScopedMockTransaction stale_while_revalidate_transaction(
      kSimpleGET_Transaction);
  stale_while_revalidate_transaction.load_flags |=
      LOAD_SUPPORT_ASYNC_REVALIDATION;
  stale_while_revalidate_transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "Age: 10801\n"
      "Cache-Control: max-age=0,stale-while-revalidate=86400\n";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), stale_while_revalidate_transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Send the request again and check that it is not sent to the network again.
  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(
      cache.http_cache(), stale_while_revalidate_transaction, &response_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_TRUE(response_info.async_revalidation_requested);
  EXPECT_FALSE(response_info.stale_revalidate_timeout.is_null());
  base::Time revalidation_timeout = response_info.stale_revalidate_timeout;
  clock.Advance(base::Seconds(1));
  EXPECT_TRUE(clock.Now() < revalidation_timeout);

  // Fetch the resource again inside the revalidation timeout window.
  RunTransactionTestWithResponseInfo(
      cache.http_cache(), stale_while_revalidate_transaction, &response_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_TRUE(response_info.async_revalidation_requested);
  EXPECT_FALSE(response_info.stale_revalidate_timeout.is_null());
  // Expect that the original revalidation timeout hasn't changed.
  EXPECT_TRUE(revalidation_timeout == response_info.stale_revalidate_timeout);

  // mask of async revalidation flag.
  stale_while_revalidate_transaction.load_flags &=
      ~LOAD_SUPPORT_ASYNC_REVALIDATION;
  stale_while_revalidate_transaction.status = "HTTP/1.1 304 Not Modified";
  // Write 304 to the cache.
  RunTransactionTestWithResponseInfo(
      cache.http_cache(), stale_while_revalidate_transaction, &response_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_FALSE(response_info.async_revalidation_requested);
  EXPECT_TRUE(response_info.stale_revalidate_timeout.is_null());
}

TEST_F(HttpCacheTest, StaleContentNotUsedWhenUnusable) {
  MockHttpCache cache;

  ScopedMockTransaction stale_while_revalidate_transaction(
      kSimpleGET_Transaction);
  stale_while_revalidate_transaction.load_flags |=
      LOAD_SUPPORT_ASYNC_REVALIDATION;
  stale_while_revalidate_transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "Age: 10801\n"
      "Cache-Control: max-age=0,stale-while-revalidate=1800\n";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), stale_while_revalidate_transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Send the request again and check that it is sent to the network again.
  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(
      cache.http_cache(), stale_while_revalidate_transaction, &response_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_FALSE(response_info.async_revalidation_requested);
}

TEST_F(HttpCacheTest, StaleContentWriteError) {
  MockHttpCache cache;
  base::SimpleTestClock clock;
  cache.http_cache()->SetClockForTesting(&clock);
  cache.network_layer()->SetClock(&clock);
  clock.Advance(base::Seconds(10));

  ScopedMockTransaction stale_while_revalidate_transaction(
      kSimpleGET_Transaction);
  stale_while_revalidate_transaction.load_flags |=
      LOAD_SUPPORT_ASYNC_REVALIDATION;
  stale_while_revalidate_transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "Age: 10801\n"
      "Cache-Control: max-age=0,stale-while-revalidate=86400\n";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), stale_while_revalidate_transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Send the request again but inject a write fault. Should still work
  // (and not dereference any null pointers).
  cache.disk_cache()->set_soft_failures_mask(MockDiskEntry::FAIL_WRITE);
  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(
      cache.http_cache(), stale_while_revalidate_transaction, &response_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
}

// Tests that we allow multiple simultaneous, non-overlapping transactions to
// take place on a sparse entry.
TEST_F(HttpCacheRangeGetTest, MultipleRequests) {
  MockHttpCache cache;

  // Create a transaction for bytes 0-9.
  MockHttpRequest request(kRangeGET_TransactionOK);
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
  transaction.data = "rg: 00-09 ";

  TestCompletionCallback callback;
  std::unique_ptr<HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(trans.get());

  // Start our transaction.
  trans->Start(&request, callback.callback(), NetLogWithSource());

  // A second transaction on a different part of the file (the default
  // kRangeGET_TransactionOK requests 40-49) should not be blocked by
  // the already pending transaction.
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);

  // Let the first transaction complete.
  callback.WaitForResult();
}

// Verify that a range request can be satisfied from a completely cached
// resource with the LOAD_ONLY_FROM_CACHE flag set. Currently it's not
// implemented so it returns ERR_CACHE_MISS. See also
// HttpCacheTest.RangeGET_OK_LoadOnlyFromCache.
// TODO(ricea): Update this test if it is implemented in future.
TEST_F(HttpCacheRangeGetTest, Previous200LoadOnlyFromCache) {
  MockHttpCache cache;

  // Store the whole thing with status 200.
  {
    MockTransaction transaction(kETagGET_Transaction);
    transaction.url = kRangeGET_TransactionOK.url;
    transaction.data = kFullRangeData;
    ScopedMockTransaction scoped_transaction(transaction);
    RunTransactionTest(cache.http_cache(), transaction);
    EXPECT_EQ(1, cache.network_layer()->transaction_count());
    EXPECT_EQ(0, cache.disk_cache()->open_count());
    EXPECT_EQ(1, cache.disk_cache()->create_count());
  }

  ScopedMockTransaction scoped_transaction(kRangeGET_TransactionOK);

  // Now see that we use the stored entry.
  MockTransaction transaction2(kRangeGET_TransactionOK);
  transaction2.load_flags |= LOAD_ONLY_FROM_CACHE;
  MockHttpRequest request(transaction2);
  TestCompletionCallback callback;

  std::unique_ptr<HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(trans);

  rv = trans->Start(&request, callback.callback(), NetLogWithSource());
  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
  }
  EXPECT_THAT(rv, IsError(ERR_CACHE_MISS));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Makes sure that a request stops using the cache when the response headers
// with "Cache-Control: no-store" arrives. That means that another request for
// the same URL can be processed before the response body of the original
// request arrives.
TEST_F(HttpCacheTest, NoStoreResponseShouldNotBlockFollowingRequests) {
  MockHttpCache cache;
  ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
  mock_transaction.response_headers = "Cache-Control: no-store\n";
  MockHttpRequest request(mock_transaction);

  auto first = std::make_unique<Context>();
  first->result = cache.CreateTransaction(&first->trans);
  ASSERT_THAT(first->result, IsOk());
  EXPECT_EQ(LOAD_STATE_IDLE, first->trans->GetLoadState());
  first->result = first->trans->Start(&request, first->callback.callback(),
                                      NetLogWithSource());
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, first->trans->GetLoadState());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(LOAD_STATE_IDLE, first->trans->GetLoadState());
  ASSERT_TRUE(first->trans->GetResponseInfo());
  EXPECT_TRUE(first->trans->GetResponseInfo()->headers->HasHeaderValue(
      "Cache-Control", "no-store"));
  // Here we have read the response header but not read the response body yet.

  // Let us create the second (read) transaction.
  auto second = std::make_unique<Context>();
  second->result = cache.CreateTransaction(&second->trans);
  ASSERT_THAT(second->result, IsOk());
  EXPECT_EQ(LOAD_STATE_IDLE, second->trans->GetLoadState());
  second->result = second->trans->Start(&request, second->callback.callback(),
                                        NetLogWithSource());

  // Here the second transaction proceeds without reading the first body.
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, second->trans->GetLoadState());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(LOAD_STATE_IDLE, second->trans->GetLoadState());
  ASSERT_TRUE(second->trans->GetResponseInfo());
  EXPECT_TRUE(second->trans->GetResponseInfo()->headers->HasHeaderValue(
      "Cache-Control", "no-store"));
  ReadAndVerifyTransaction(second->trans.get(), kSimpleGET_Transaction);
}

// Tests that serving a response entirely from cache replays the previous
// SSLInfo.
TEST_F(HttpCacheTest, CachePreservesSSLInfo) {
  static const uint16_t kTLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 = 0xc02f;
  int status = 0;
  SSLConnectionStatusSetCipherSuite(kTLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                                    &status);
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_2, &status);

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");

  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.cert = cert;
  transaction.ssl_connection_status = status;

  // Fetch the resource.
  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response_info);

  // The request should have hit the network and a cache entry created.
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The expected SSL state was reported.
  EXPECT_EQ(transaction.ssl_connection_status,
            response_info.ssl_info.connection_status);
  EXPECT_TRUE(cert->EqualsIncludingChain(response_info.ssl_info.cert.get()));

  // Fetch the resource again.
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response_info);

  // The request should have been reused without hitting the network.
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The SSL state was preserved.
  EXPECT_EQ(status, response_info.ssl_info.connection_status);
  EXPECT_TRUE(cert->EqualsIncludingChain(response_info.ssl_info.cert.get()));
}

// Tests that SSLInfo gets updated when revalidating a cached response.
TEST_F(HttpCacheTest, RevalidationUpdatesSSLInfo) {
  static const uint16_t kTLS_RSA_WITH_RC4_128_MD5 = 0x0004;
  static const uint16_t kTLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 = 0xc02f;

  int status1 = 0;
  SSLConnectionStatusSetCipherSuite(kTLS_RSA_WITH_RC4_128_MD5, &status1);
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1, &status1);
  int status2 = 0;
  SSLConnectionStatusSetCipherSuite(kTLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                                    &status2);
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_2, &status2);

  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");

  MockHttpCache cache;

  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.cert = cert1;
  transaction.ssl_connection_status = status1;

  // Fetch the resource.
  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response_info);

  // The request should have hit the network and a cache entry created.
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  EXPECT_FALSE(response_info.was_cached);

  // The expected SSL state was reported.
  EXPECT_EQ(status1, response_info.ssl_info.connection_status);
  EXPECT_TRUE(cert1->EqualsIncludingChain(response_info.ssl_info.cert.get()));

  // The server deploys a more modern configuration but reports 304 on the
  // revalidation attempt.
  transaction.status = "HTTP/1.1 304 Not Modified";
  transaction.cert = cert2;
  transaction.ssl_connection_status = status2;

  // Fetch the resource again, forcing a revalidation.
  transaction.request_headers = "Cache-Control: max-age=0\r\n";
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response_info);

  // The request should have been successfully revalidated.
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  EXPECT_TRUE(response_info.was_cached);

  // The new SSL state is reported.
  EXPECT_EQ(status2, response_info.ssl_info.connection_status);
  EXPECT_TRUE(cert2->EqualsIncludingChain(response_info.ssl_info.cert.get()));
}

TEST_F(HttpCacheTest, CacheEntryStatusOther) {
  MockHttpCache cache;

  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), kRangeGET_Transaction,
                                     &response_info);

  EXPECT_FALSE(response_info.was_cached);
  EXPECT_TRUE(response_info.network_accessed);
  EXPECT_EQ(CacheEntryStatus::ENTRY_OTHER, response_info.cache_entry_status);
}

TEST_F(HttpCacheTest, CacheEntryStatusNotInCache) {
  MockHttpCache cache;

  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), kSimpleGET_Transaction,
                                     &response_info);

  EXPECT_FALSE(response_info.was_cached);
  EXPECT_TRUE(response_info.network_accessed);
  EXPECT_EQ(CacheEntryStatus::ENTRY_NOT_IN_CACHE,
            response_info.cache_entry_status);
}

TEST_F(HttpCacheTest, CacheEntryStatusUsed) {
  MockHttpCache cache;
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), kSimpleGET_Transaction,
                                     &response_info);

  EXPECT_TRUE(response_info.was_cached);
  EXPECT_FALSE(response_info.network_accessed);
  EXPECT_EQ(CacheEntryStatus::ENTRY_USED, response_info.cache_entry_status);
}

TEST_F(HttpCacheTest, CacheEntryStatusValidated) {
  MockHttpCache cache;
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

  ScopedMockTransaction still_valid(kETagGET_Transaction);
  still_valid.load_flags = LOAD_VALIDATE_CACHE;  // Force a validation.
  still_valid.handler = kETagGetConditionalRequestHandler;

  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), still_valid,
                                     &response_info);

  EXPECT_TRUE(response_info.was_cached);
  EXPECT_TRUE(response_info.network_accessed);
  EXPECT_EQ(CacheEntryStatus::ENTRY_VALIDATED,
            response_info.cache_entry_status);
}

TEST_F(HttpCacheTest, CacheEntryStatusUpdated) {
  MockHttpCache cache;
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

  ScopedMockTransaction update(kETagGET_Transaction);
  update.load_flags = LOAD_VALIDATE_CACHE;  // Force a validation.

  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), update,
                                     &response_info);

  EXPECT_FALSE(response_info.was_cached);
  EXPECT_TRUE(response_info.network_accessed);
  EXPECT_EQ(CacheEntryStatus::ENTRY_UPDATED, response_info.cache_entry_status);
}

TEST_F(HttpCacheTest, CacheEntryStatusCantConditionalize) {
  MockHttpCache cache;
  cache.FailConditionalizations();
  RunTransactionTest(cache.http_cache(), kTypicalGET_Transaction);

  HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(),
                                     kTypicalGET_Transaction, &response_info);

  EXPECT_FALSE(response_info.was_cached);
  EXPECT_TRUE(response_info.network_accessed);
  EXPECT_EQ(CacheEntryStatus::ENTRY_CANT_CONDITIONALIZE,
            response_info.cache_entry_status);
}

TEST_F(HttpSplitCacheKeyTest, GetResourceURLFromHttpCacheKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSplitCacheByNetworkIsolationKey);
  MockHttpCache cache;
  std::string urls[] = {"http://www.a.com/", "https://b.com/example.html",
                        "http://example.com/Some Path/Some Leaf?some query"};

  for (const std::string& url : urls) {
    std::string key = ComputeCacheKey(url);
    EXPECT_EQ(GURL(url).spec(), HttpCache::GetResourceURLFromHttpCacheKey(key));
  }
}

TEST_F(HttpCacheTest, GetResourceURLFromHttpCacheKey) {
  const struct {
    std::string input;
    std::string output;
  } kTestCase[] = {
      // Valid input:
      {"0/0/https://a.com/", "https://a.com/"},
      {"0/0/https://a.com/path", "https://a.com/path"},
      {"0/0/https://a.com/?query", "https://a.com/?query"},
      {"0/0/https://a.com/#fragment", "https://a.com/#fragment"},
      {"0/0/_dk_s_ https://a.com/", "https://a.com/"},
      {"0/0/_dk_https://a.com https://b.com https://c.com/", "https://c.com/"},
      {"0/0/_dk_shttps://a.com https://b.com https://c.com/", "https://c.com/"},

      // Invalid input, producing garbage, without crashing.
      {"", ""},
      {"0/a.com", "0/a.com"},
      {"https://a.com/", "a.com/"},
      {"0/https://a.com/", "/a.com/"},
  };

  for (const auto& test : kTestCase) {
    EXPECT_EQ(test.output,
              HttpCache::GetResourceURLFromHttpCacheKey(test.input));
  }
}

class TestCompletionCallbackForHttpCache : public TestCompletionCallbackBase {
 public:
  TestCompletionCallbackForHttpCache() = default;
  ~TestCompletionCallbackForHttpCache() override = default;

  CompletionRepeatingCallback callback() {
    return base::BindRepeating(&TestCompletionCallbackForHttpCache::SetResult,
                               base::Unretained(this));
  }

  const std::vector<int>& results() { return results_; }

 private:
  std::vector<int> results_;

 protected:
  void SetResult(int result) override {
    results_.push_back(result);
    DidSetResult();
  }
};

TEST_F(HttpCacheIOCallbackTest, FailedDoomFollowedByOpen) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to DoomEntry and OpenEntry
  // below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;

  cache.disk_cache()->set_force_fail_callback_later(true);

  // Queue up our operations.
  int rv = DoomEntry(cache.http_cache(), m_transaction.url, transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cache.disk_cache()->set_force_fail_callback_later(false);
  rv = OpenEntry(cache.http_cache(), m_transaction.url, &entry1,
                 transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that DoomEntry failed correctly.
  ASSERT_EQ(cb.results()[0], ERR_CACHE_DOOM_FAILURE);
  // Verify that OpenEntry fails with the same code.
  ASSERT_EQ(cb.results()[1], ERR_CACHE_DOOM_FAILURE);
  ASSERT_EQ(entry1, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, FailedDoomFollowedByCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to DoomEntry and CreateEntry
  // below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;

  cache.disk_cache()->set_force_fail_callback_later(true);

  // Queue up our operations.
  int rv = DoomEntry(cache.http_cache(), m_transaction.url, transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cache.disk_cache()->set_force_fail_callback_later(false);
  rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                   transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that DoomEntry failed correctly.
  ASSERT_EQ(cb.results()[0], ERR_CACHE_DOOM_FAILURE);
  // Verify that CreateEntry requests a restart (CACHE_RACE).
  ASSERT_EQ(cb.results()[1], ERR_CACHE_RACE);
  ASSERT_EQ(entry1, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, FailedDoomFollowedByDoom) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to DoomEntry below require that
  // it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  cache.disk_cache()->set_force_fail_callback_later(true);

  // Queue up our operations.
  int rv = DoomEntry(cache.http_cache(), m_transaction.url, transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cache.disk_cache()->set_force_fail_callback_later(false);
  rv = DoomEntry(cache.http_cache(), m_transaction.url, transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that DoomEntry failed correctly.
  ASSERT_EQ(cb.results()[0], ERR_CACHE_DOOM_FAILURE);
  // Verify that the second DoomEntry requests a restart (CACHE_RACE).
  ASSERT_EQ(cb.results()[1], ERR_CACHE_RACE);
}

TEST_F(HttpCacheIOCallbackTest, FailedOpenFollowedByCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to OpenEntry and CreateEntry
  // below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  cache.disk_cache()->set_force_fail_callback_later(true);

  // Queue up our operations.
  int rv = OpenEntry(cache.http_cache(), m_transaction.url, &entry1,
                     transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cache.disk_cache()->set_force_fail_callback_later(false);
  rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                   transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that OpenEntry failed correctly.
  ASSERT_EQ(cb.results()[0], ERR_CACHE_OPEN_FAILURE);
  ASSERT_EQ(entry1, nullptr);
  // Verify that the CreateEntry requests a restart (CACHE_RACE).
  ASSERT_EQ(cb.results()[1], ERR_CACHE_RACE);
  ASSERT_EQ(entry2, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, FailedCreateFollowedByOpen) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to CreateEntry and OpenEntry
  // below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  cache.disk_cache()->set_force_fail_callback_later(true);

  // Queue up our operations.
  int rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                       transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cache.disk_cache()->set_force_fail_callback_later(false);
  rv = OpenEntry(cache.http_cache(), m_transaction.url, &entry2,
                 transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that CreateEntry failed correctly.
  ASSERT_EQ(cb.results()[0], ERR_CACHE_CREATE_FAILURE);
  ASSERT_EQ(entry1, nullptr);
  // Verify that the OpenEntry requests a restart (CACHE_RACE).
  ASSERT_EQ(cb.results()[1], ERR_CACHE_RACE);
  ASSERT_EQ(entry2, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, FailedCreateFollowedByCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to CreateEntry below require
  // that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  cache.disk_cache()->set_force_fail_callback_later(true);

  // Queue up our operations.
  int rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                       transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cache.disk_cache()->set_force_fail_callback_later(false);
  rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                   transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify the CreateEntry(s) failed.
  ASSERT_EQ(cb.results()[0], ERR_CACHE_CREATE_FAILURE);
  ASSERT_EQ(entry1, nullptr);
  ASSERT_EQ(cb.results()[1], ERR_CACHE_CREATE_FAILURE);
  ASSERT_EQ(entry2, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, CreateFollowedByCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to CreateEntry below require
  // that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  // Queue up our operations.
  int rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                       transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                   transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that the first CreateEntry succeeded.
  ASSERT_EQ(cb.results()[0], OK);
  ASSERT_NE(entry1, nullptr);
  // Verify that the second CreateEntry failed.
  ASSERT_EQ(cb.results()[1], ERR_CACHE_CREATE_FAILURE);
  ASSERT_EQ(entry2, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, OperationFollowedByDoom) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to CreateEntry and DoomEntry
  // below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;

  // Queue up our operations.
  // For this test all we need is some operation followed by a doom, a create
  // fulfills that requirement.
  int rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                       transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  rv = DoomEntry(cache.http_cache(), m_transaction.url, transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that the CreateEntry succeeded.
  ASSERT_EQ(cb.results()[0], OK);
  // Verify that the DoomEntry requests a restart (CACHE_RACE).
  ASSERT_EQ(cb.results()[1], ERR_CACHE_RACE);
}

TEST_F(HttpCacheIOCallbackTest, CreateFollowedByOpenOrCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to CreateEntry and
  // OpenOrCreateEntry below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  // Queue up our operations.
  int rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                       transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  rv = OpenOrCreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                         transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that the CreateEntry succeeded.
  ASSERT_EQ(cb.results()[0], OK);
  ASSERT_NE(entry1, nullptr);
  // Verify that OpenOrCreateEntry succeeded.
  ASSERT_EQ(cb.results()[1], OK);
  ASSERT_NE(entry2, nullptr);
  ASSERT_EQ(entry1->GetEntry(), entry2->GetEntry());
}

TEST_F(HttpCacheIOCallbackTest, FailedCreateFollowedByOpenOrCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to CreateEntry and
  // OpenOrCreateEntry below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  cache.disk_cache()->set_force_fail_callback_later(true);

  // Queue up our operations.
  int rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                       transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cache.disk_cache()->set_force_fail_callback_later(false);
  rv = OpenOrCreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                         transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that CreateEntry failed correctly.
  ASSERT_EQ(cb.results()[0], ERR_CACHE_CREATE_FAILURE);
  ASSERT_EQ(entry1, nullptr);
  // Verify that the OpenOrCreateEntry requests a restart (CACHE_RACE).
  ASSERT_EQ(cb.results()[1], ERR_CACHE_RACE);
  ASSERT_EQ(entry2, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, OpenFollowedByOpenOrCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to OpenEntry and
  // OpenOrCreateEntry below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry0 = nullptr;
  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  // First need to create and entry so we can open it.
  int rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry0,
                       transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), static_cast<size_t>(1));
  ASSERT_EQ(cb.results()[0], OK);
  ASSERT_NE(entry0, nullptr);
  // Manually Deactivate() `entry0` because OpenEntry() fails if there is an
  // existing active entry.
  entry0.reset();

  // Queue up our operations.
  rv = OpenEntry(cache.http_cache(), m_transaction.url, &entry1,
                 transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  rv = OpenOrCreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                         transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 3u);

  // Verify that the OpenEntry succeeded.
  ASSERT_EQ(cb.results()[1], OK);
  ASSERT_NE(entry1, nullptr);
  // Verify that OpenOrCreateEntry succeeded.
  ASSERT_EQ(cb.results()[2], OK);
  ASSERT_NE(entry2, nullptr);
  ASSERT_EQ(entry1->GetEntry(), entry2->GetEntry());
}

TEST_F(HttpCacheIOCallbackTest, FailedOpenFollowedByOpenOrCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to OpenEntry and
  // OpenOrCreateEntry below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  cache.disk_cache()->set_force_fail_callback_later(true);

  // Queue up our operations.
  int rv = OpenEntry(cache.http_cache(), m_transaction.url, &entry1,
                     transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cache.disk_cache()->set_force_fail_callback_later(false);
  rv = OpenOrCreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                         transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that OpenEntry failed correctly.
  ASSERT_EQ(cb.results()[0], ERR_CACHE_OPEN_FAILURE);
  ASSERT_EQ(entry1, nullptr);
  // Verify that the OpenOrCreateEntry requests a restart (CACHE_RACE).
  ASSERT_EQ(cb.results()[1], ERR_CACHE_RACE);
  ASSERT_EQ(entry2, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, OpenOrCreateFollowedByCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to OpenOrCreateEntry and
  // CreateEntry below require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  // Queue up our operations.
  int rv = OpenOrCreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                             transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  rv = CreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                   transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that the OpenOrCreateEntry succeeded.
  ASSERT_EQ(cb.results()[0], OK);
  ASSERT_NE(entry1, nullptr);
  // Verify that CreateEntry failed.
  ASSERT_EQ(cb.results()[1], ERR_CACHE_CREATE_FAILURE);
  ASSERT_EQ(entry2, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, OpenOrCreateFollowedByOpenOrCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to OpenOrCreateEntry below
  // require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  // Queue up our operations.
  int rv = OpenOrCreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                             transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  rv = OpenOrCreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                         transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that the OpenOrCreateEntry succeeded.
  ASSERT_EQ(cb.results()[0], OK);
  ASSERT_NE(entry1, nullptr);
  // Verify that the other succeeded.
  ASSERT_EQ(cb.results()[1], OK);
  ASSERT_NE(entry2, nullptr);
}

TEST_F(HttpCacheIOCallbackTest, FailedOpenOrCreateFollowedByOpenOrCreate) {
  MockHttpCache cache;
  TestCompletionCallbackForHttpCache cb;
  std::unique_ptr<Transaction> transaction =
      std::make_unique<Transaction>(DEFAULT_PRIORITY, cache.http_cache());

  transaction->SetIOCallBackForTest(cb.callback());
  transaction->SetCacheIOCallBackForTest(cb.callback());

  // Create the backend here as our direct calls to OpenOrCreateEntry below
  // require that it exists.
  cache.backend();

  // Need a mock transaction in order to use some of MockHttpCache's
  // functions.
  ScopedMockTransaction m_transaction(kSimpleGET_Transaction);

  scoped_refptr<ActiveEntry> entry1 = nullptr;
  scoped_refptr<ActiveEntry> entry2 = nullptr;

  cache.disk_cache()->set_force_fail_callback_later(true);

  // Queue up our operations.
  int rv = OpenOrCreateEntry(cache.http_cache(), m_transaction.url, &entry1,
                             transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);
  cache.disk_cache()->set_force_fail_callback_later(false);
  rv = OpenOrCreateEntry(cache.http_cache(), m_transaction.url, &entry2,
                         transaction.get());
  ASSERT_EQ(rv, ERR_IO_PENDING);

  // Wait for all the results to arrive.
  cb.GetResult(rv);
  ASSERT_EQ(cb.results().size(), 2u);

  // Verify that the OpenOrCreateEntry failed.
  ASSERT_EQ(cb.results()[0], ERR_CACHE_OPEN_OR_CREATE_FAILURE);
  ASSERT_EQ(entry1, nullptr);
  // Verify that the other failed.
  ASSERT_EQ(cb.results()[1], ERR_CACHE_OPEN_OR_CREATE_FAILURE);
  ASSERT_EQ(entry2, nullptr);
}

TEST_F(HttpCacheTest, DnsAliasesNoRevalidation) {
  MockHttpCache cache;
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.dns_aliases = {"alias1", "alias2"};

  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);
  EXPECT_THAT(response.dns_aliases, testing::ElementsAre("alias1", "alias2"));

  // The second request result in a cache hit and the response used without
  // revalidation. Set the transaction alias list to empty to verify that the
  // cached aliases are being used.
  transaction.dns_aliases = {};
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_TRUE(response.was_cached);
  EXPECT_THAT(response.dns_aliases, testing::ElementsAre("alias1", "alias2"));
}

TEST_F(HttpCacheTest, NoDnsAliasesNoRevalidation) {
  MockHttpCache cache;
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.dns_aliases = {};

  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);
  EXPECT_TRUE(response.dns_aliases.empty());

  // The second request should result in a cache hit and the response used
  // without revalidation. Set the transaction alias list to nonempty to verify
  // that the cached aliases are being used.
  transaction.dns_aliases = {"alias"};
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_TRUE(response.was_cached);
  EXPECT_TRUE(response.dns_aliases.empty());
}

TEST_F(HttpCacheTest, DnsAliasesRevalidation) {
  MockHttpCache cache;
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kTypicalGET_Transaction);
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Cache-Control: max-age=0\n";
  transaction.dns_aliases = {"alias1", "alias2"};

  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);
  EXPECT_THAT(response.dns_aliases, testing::ElementsAre("alias1", "alias2"));

  // On the second request, the cache should be revalidated. Change the aliases
  // to be sure that the new aliases are being used, and have the response be
  // cached for next time.
  transaction.response_headers = "Cache-Control: max-age=10000\n";
  transaction.dns_aliases = {"alias3", "alias4"};
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);
  EXPECT_THAT(response.dns_aliases, testing::ElementsAre("alias3", "alias4"));

  transaction.dns_aliases = {"alias5", "alias6"};
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_TRUE(response.was_cached);
  EXPECT_THAT(response.dns_aliases, testing::ElementsAre("alias3", "alias4"));
}

using HttpCacheFirstPartySetsBypassCacheTest = HttpCacheTest;

TEST_F(HttpCacheFirstPartySetsBypassCacheTest, ShouldBypassNoId) {
  MockHttpCache cache;
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);

  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);

  transaction.fps_cache_filter = {5};
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);
}

TEST_F(HttpCacheFirstPartySetsBypassCacheTest, ShouldBypassIdTooSmall) {
  MockHttpCache cache;
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  const int64_t kBrowserRunId = 4;
  transaction.browser_run_id = {kBrowserRunId};
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);
  EXPECT_TRUE(response.browser_run_id.has_value());
  EXPECT_EQ(kBrowserRunId, response.browser_run_id.value());

  transaction.fps_cache_filter = {5};
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);
}

TEST_F(HttpCacheFirstPartySetsBypassCacheTest, ShouldNotBypass) {
  MockHttpCache cache;
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  const int64_t kBrowserRunId = 5;
  transaction.browser_run_id = {kBrowserRunId};
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);
  EXPECT_TRUE(response.browser_run_id.has_value());
  EXPECT_EQ(kBrowserRunId, response.browser_run_id.value());

  transaction.fps_cache_filter = {5};
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_TRUE(response.was_cached);
}

TEST_F(HttpCacheFirstPartySetsBypassCacheTest, ShouldNotBypassNoFilter) {
  MockHttpCache cache;
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);

  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_FALSE(response.was_cached);

  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);
  EXPECT_TRUE(response.was_cached);
}

TEST_F(HttpCacheTest, SecurityHeadersAreCopiedToConditionalizedResponse) {
  MockHttpCache cache;
  HttpResponseInfo response;
  ScopedMockTransaction transaction(kSimpleGET_Transaction);

  static const Response kNetResponse1 = {
      "HTTP/1.1 200 OK",
      "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
      "Server: server1\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n"
      "Cross-Origin-Resource-Policy: cross-origin\n",
      "body1"};

  static const Response kNetResponse2 = {
      "HTTP/1.1 304 Not Modified",
      "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
      "Server: server2\n"
      "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
      ""};

  kNetResponse1.AssignTo(&transaction);
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);

  // On the second request, the cache is revalidated.
  const char kExtraRequestHeaders[] =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n";
  transaction.request_headers = kExtraRequestHeaders;
  kNetResponse2.AssignTo(&transaction);
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response);

  // Verify that the CORP header was carried over to the response.
  std::string response_corp_header;
  response.headers->GetNormalizedHeader("Cross-Origin-Resource-Policy",
                                        &response_corp_header);

  EXPECT_EQ(304, response.headers->response_code());
  EXPECT_EQ("cross-origin", response_corp_header);
}

}  // namespace net
