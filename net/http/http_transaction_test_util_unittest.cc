// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_transaction_test_util.h"

#include <string>
#include <string_view>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/base/test_completion_callback.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Default transaction.
const MockTransaction kBasicTransaction = {
    .url = "http://www.example.com/",
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "",
    .load_flags = LOAD_NORMAL,
    .transport_info = TransportInfo(TransportType::kDirect,
                                    IPEndPoint(IPAddress::IPv4Localhost(), 80),
                                    /*accept_ch_frame_arg=*/"",
                                    /*cert_is_issued_by_known_root=*/false,
                                    kProtoUnknown),
    .status = "HTTP/1.1 200 OK",
    .response_headers = "Cache-Control: max-age=10000\n",
    .response_time = base::Time(),
    .data = "<html><body>Hello world!</body></html>",
    .dns_aliases = {},
    .fps_cache_filter = std::nullopt,
    .browser_run_id = std::nullopt,
    .test_mode = TEST_MODE_NORMAL,
    .handler = MockTransactionHandler(),
    .read_handler = MockTransactionReadHandler(),
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = OK,
    .read_return_code = OK,
};
const size_t kDefaultBufferSize = 1024;

}  // namespace

class MockNetworkTransactionTest : public ::testing::Test {
 public:
  MockNetworkTransactionTest()
      : network_layer_(std::make_unique<MockNetworkLayer>()) {}
  ~MockNetworkTransactionTest() override = default;

  MockNetworkTransactionTest(const MockNetworkTransactionTest&) = delete;
  MockNetworkTransactionTest& operator=(const MockNetworkTransactionTest&) =
      delete;

 protected:
  std::unique_ptr<HttpTransaction> CreateNetworkTransaction() {
    std::unique_ptr<HttpTransaction> network_transaction;
    network_layer_->CreateTransaction(DEFAULT_PRIORITY, &network_transaction);
    return network_transaction;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  MockNetworkLayer& network_layer() { return *network_layer_.get(); }

 private:
  std::unique_ptr<MockNetworkLayer> network_layer_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(MockNetworkTransactionTest, Basic) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  EXPECT_FALSE(transaction->GetResponseInfo()->was_cached);
  EXPECT_TRUE(transaction->GetResponseInfo()->network_accessed);
  EXPECT_EQ(mock_transaction.transport_info.endpoint,
            transaction->GetResponseInfo()->remote_endpoint);
  EXPECT_FALSE(transaction->GetResponseInfo()->WasFetchedViaProxy());

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction->Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  ASSERT_THAT(read_result, std::string_view(mock_transaction.data).size());
  EXPECT_EQ(std::string_view(mock_transaction.data),
            std::string_view(buf->data(), read_result));
}

TEST_F(MockNetworkTransactionTest, SyncNetStart) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  mock_transaction.test_mode = TEST_MODE_SYNC_NET_START;
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction->Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  ASSERT_THAT(read_result, std::string_view(mock_transaction.data).size());
  EXPECT_EQ(std::string_view(mock_transaction.data),
            std::string_view(buf->data(), read_result));
}

TEST_F(MockNetworkTransactionTest, AsyncNetStartFailure) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  mock_transaction.start_return_code = ERR_NETWORK_ACCESS_DENIED;
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(),
              test::IsError(ERR_NETWORK_ACCESS_DENIED));
}

TEST_F(MockNetworkTransactionTest, SyncNetStartFailure) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  mock_transaction.test_mode = TEST_MODE_SYNC_NET_START;
  mock_transaction.start_return_code = ERR_NETWORK_ACCESS_DENIED;
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_NETWORK_ACCESS_DENIED));
}

TEST_F(MockNetworkTransactionTest, BeforeNetworkStartCallback) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  bool before_network_start_callback_called = false;
  transaction->SetBeforeNetworkStartCallback(base::BindLambdaForTesting(
      [&](bool* defer) { before_network_start_callback_called = true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
  EXPECT_TRUE(before_network_start_callback_called);
}

TEST_F(MockNetworkTransactionTest, BeforeNetworkStartCallbackDeferAndResume) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  bool before_network_start_callback_called = false;
  transaction->SetBeforeNetworkStartCallback(
      base::BindLambdaForTesting([&](bool* defer) {
        before_network_start_callback_called = true;
        *defer = true;
      }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_TRUE(before_network_start_callback_called);
  RunUntilIdle();
  EXPECT_FALSE(start_callback.have_result());
  transaction->ResumeNetworkStart();
  EXPECT_FALSE(start_callback.have_result());
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
}

TEST_F(MockNetworkTransactionTest, AsyncConnectedCallback) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  bool connected_callback_called = false;
  CompletionOnceCallback callback_for_connected_callback;
  transaction->SetConnectedCallback(base::BindLambdaForTesting(
      [&](const TransportInfo& info, CompletionOnceCallback callback) -> int {
        EXPECT_EQ(mock_transaction.transport_info, info);
        connected_callback_called = true;
        callback_for_connected_callback = std::move(callback);
        return ERR_IO_PENDING;
      }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_TRUE(connected_callback_called);
  EXPECT_FALSE(start_callback.have_result());
  std::move(callback_for_connected_callback).Run(OK);
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
}

TEST_F(MockNetworkTransactionTest, AsyncConnectedCallbackFailure) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  bool connected_callback_called = false;
  CompletionOnceCallback callback_for_connected_callback;
  transaction->SetConnectedCallback(base::BindLambdaForTesting(
      [&](const TransportInfo& info, CompletionOnceCallback callback) -> int {
        EXPECT_EQ(mock_transaction.transport_info, info);
        connected_callback_called = true;
        callback_for_connected_callback = std::move(callback);
        return ERR_IO_PENDING;
      }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_TRUE(connected_callback_called);
  EXPECT_FALSE(start_callback.have_result());
  std::move(callback_for_connected_callback).Run(ERR_INSUFFICIENT_RESOURCES);
  EXPECT_THAT(start_callback.WaitForResult(),
              test::IsError(ERR_INSUFFICIENT_RESOURCES));
}

TEST_F(MockNetworkTransactionTest, SyncConnectedCallback) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  bool connected_callback_called = false;
  transaction->SetConnectedCallback(base::BindLambdaForTesting(
      [&](const TransportInfo& info, CompletionOnceCallback callback) -> int {
        EXPECT_EQ(mock_transaction.transport_info, info);
        connected_callback_called = true;
        return OK;
      }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_TRUE(connected_callback_called);
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
}

TEST_F(MockNetworkTransactionTest, SyncConnectedCallbackFailure) {
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  bool connected_callback_called = false;
  transaction->SetConnectedCallback(base::BindLambdaForTesting(
      [&](const TransportInfo& info, CompletionOnceCallback callback) -> int {
        EXPECT_EQ(mock_transaction.transport_info, info);
        connected_callback_called = true;
        return ERR_INSUFFICIENT_RESOURCES;
      }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_TRUE(connected_callback_called);
  EXPECT_THAT(start_callback.WaitForResult(),
              test::IsError(ERR_INSUFFICIENT_RESOURCES));
}

TEST_F(MockNetworkTransactionTest, ModifyRequestHeadersCallback) {
  const std::string kTestResponseData = "hello";
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  mock_transaction.request_headers = "Foo: Bar\r\n";

  bool transaction_handler_called = false;
  mock_transaction.handler = base::BindLambdaForTesting(
      [&](const HttpRequestInfo* request, std::string* response_status,
          std::string* response_headers, std::string* response_data) {
        EXPECT_EQ("Foo: Bar\r\nHoge: Piyo\r\n\r\n",
                  request->extra_headers.ToString());
        *response_data = kTestResponseData;
        transaction_handler_called = true;
      });
  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  bool modify_request_headers_callback_called_ = false;
  transaction->SetModifyRequestHeadersCallback(
      base::BindLambdaForTesting([&](HttpRequestHeaders* request_headers) {
        modify_request_headers_callback_called_ = true;
        request_headers->SetHeader("Hoge", "Piyo");
      }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
  EXPECT_TRUE(modify_request_headers_callback_called_);
  EXPECT_TRUE(transaction_handler_called);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction->Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  ASSERT_THAT(read_result, kTestResponseData.size());
  EXPECT_EQ(kTestResponseData, std::string_view(buf->data(), read_result));
}

TEST_F(MockNetworkTransactionTest, CallbackOrder) {
  const std::string kTestResponseData = "hello";
  ScopedMockTransaction mock_transaction(kBasicTransaction);
  mock_transaction.request_headers = "Foo: Bar\r\n";

  bool before_network_start_callback_called = false;
  bool connected_callback_called = false;
  bool modify_request_headers_callback_called_ = false;
  bool transaction_handler_called = false;

  mock_transaction.handler = base::BindLambdaForTesting(
      [&](const HttpRequestInfo* request, std::string* response_status,
          std::string* response_headers, std::string* response_data) {
        EXPECT_TRUE(before_network_start_callback_called);
        EXPECT_TRUE(connected_callback_called);
        EXPECT_TRUE(modify_request_headers_callback_called_);
        EXPECT_FALSE(transaction_handler_called);

        *response_data = kTestResponseData;
        transaction_handler_called = true;
      });

  HttpRequestInfo request = MockHttpRequest(mock_transaction);

  auto transaction = CreateNetworkTransaction();
  transaction->SetBeforeNetworkStartCallback(
      base::BindLambdaForTesting([&](bool* defer) {
        EXPECT_FALSE(before_network_start_callback_called);
        EXPECT_FALSE(connected_callback_called);
        EXPECT_FALSE(modify_request_headers_callback_called_);
        EXPECT_FALSE(transaction_handler_called);

        before_network_start_callback_called = true;
        *defer = true;
      }));

  CompletionOnceCallback callback_for_connected_callback;
  transaction->SetConnectedCallback(base::BindLambdaForTesting(
      [&](const TransportInfo& info, CompletionOnceCallback callback) -> int {
        EXPECT_TRUE(before_network_start_callback_called);
        EXPECT_FALSE(connected_callback_called);
        EXPECT_FALSE(modify_request_headers_callback_called_);
        EXPECT_FALSE(transaction_handler_called);

        connected_callback_called = true;
        callback_for_connected_callback = std::move(callback);
        return ERR_IO_PENDING;
      }));

  transaction->SetModifyRequestHeadersCallback(
      base::BindLambdaForTesting([&](HttpRequestHeaders* request_headers) {
        EXPECT_TRUE(before_network_start_callback_called);
        EXPECT_TRUE(connected_callback_called);
        EXPECT_FALSE(modify_request_headers_callback_called_);
        EXPECT_FALSE(transaction_handler_called);

        modify_request_headers_callback_called_ = true;
      }));

  EXPECT_FALSE(before_network_start_callback_called);
  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));

  EXPECT_TRUE(before_network_start_callback_called);

  EXPECT_FALSE(connected_callback_called);
  transaction->ResumeNetworkStart();
  RunUntilIdle();
  EXPECT_TRUE(connected_callback_called);

  EXPECT_FALSE(modify_request_headers_callback_called_);
  std::move(callback_for_connected_callback).Run(OK);
  EXPECT_TRUE(modify_request_headers_callback_called_);
  EXPECT_TRUE(transaction_handler_called);

  EXPECT_TRUE(start_callback.have_result());
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
}

}  // namespace net
