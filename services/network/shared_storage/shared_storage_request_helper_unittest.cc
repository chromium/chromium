// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_storage/shared_storage_request_helper.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "services/network/shared_storage/shared_storage_header_utils.h"
#include "services/network/shared_storage/shared_storage_test_url_loader_network_observer.h"
#include "services/network/shared_storage/shared_storage_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

using testing::ElementsAre;

const char kHostname[] = "a.test";
const char kTestRequestOrigin[] = "https://a.test";

[[nodiscard]] GURL GetHttpsRequestURL() {
  return GURL(
      base::StrCat({kTestRequestOrigin, kSharedStoragePathPrefix,
                    kSharedStorageTestPath, kSharedStorageWritePathSuffix}));
}

class PausableTestDelegate : public net::TestDelegate {
 public:
  PausableTestDelegate() { set_on_complete(base::DoNothing()); }

  PausableTestDelegate(const PausableTestDelegate&) = delete;
  PausableTestDelegate& operator=(const PausableTestDelegate&) = delete;

  ~PausableTestDelegate() override = default;

  void OnResponseStarted(net::URLRequest* request, int net_error) override {
    response_started_ = true;
    request_ = request->GetWeakPtr();
    net_error_ = net_error;
    if (loop_ && loop_->running()) {
      loop_->Quit();
    }
  }

  void WaitUntilResponseStarted() {
    DCHECK(!loop_);
    if (response_started_) {
      return;
    }
    loop_ = std::make_unique<base::RunLoop>();
    loop_->Run();
    loop_.reset();
    DCHECK(response_started_);
    response_started_ = false;
  }

  void ResumeOnResponseStarted() {
    if (request_) {
      TestDelegate::OnResponseStarted(request_.get(), net_error_);
    }
  }

  void ResetDelegate() {
    response_started_ = false;
    set_on_complete(base::DoNothing());
  }

 private:
  std::unique_ptr<base::RunLoop> loop_;
  bool response_started_ = false;
  base::WeakPtr<net::URLRequest> request_ = nullptr;
  int net_error_ = net::OK;
};

class SharedStorageRequestHelperTest : public net::TestWithTaskEnvironment {
 public:
  void SetUp() override {
    listener_ = std::make_unique<net::test_server::SimpleConnectionListener>(
        /*expected_connections=*/GetNumExpectedConnections(),
        net::test_server::SimpleConnectionListener::AllowAdditionalConnections::
            FAIL_ON_ADDITIONAL_CONNECTIONS);
    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    observer_ = std::make_unique<SharedStorageTestURLLoaderNetworkObserver>();

    auto context_builder = net::CreateTestURLRequestContextBuilder();
    auto cert_verifier = std::make_unique<net::MockCertVerifier>();
    cert_verifier->set_default_result(net::OK);
    context_builder->SetCertVerifier(std::move(cert_verifier));
    auto host_resolver = std::make_unique<net::MockHostResolver>();
    host_resolver->rules()->AddRule(kHostname, "127.0.0.1");
    context_builder->set_host_resolver(std::move(host_resolver));
    context_ = context_builder->Build();
    RegisterDefaultHandlers(test_server_.get());
    net::TestWithTaskEnvironment::SetUp();
  }

  virtual int GetNumExpectedConnections() const { return 1; }

  net::URLRequestContext& context() { return *context_; }

  void CreateSharedStorageRequestHelper(bool shared_storage_writable_eligible) {
    helper_ = std::make_unique<SharedStorageRequestHelper>(
        shared_storage_writable_eligible, observer_.get());
  }

  [[nodiscard]] mojom::URLLoaderNetworkServiceObserver* MaybeGetHeaderObserver(
      bool get) {
    return get ? observer_.get() : nullptr;
  }

  [[nodiscard]] const GURL GetRequestURLFromServer() {
    DCHECK(test_server_);
    DCHECK(test_server_->Started());
    return test_server_->GetURL(kHostname, MakeSharedStorageTestPath());
  }

  [[nodiscard]] const GURL GetSharedStorageBypassRequestURLFromServer() {
    DCHECK(test_server_);
    DCHECK(test_server_->Started());
    return test_server_->GetURL(kHostname, MakeSharedStorageBypassPath());
  }

  [[nodiscard]] std::unique_ptr<net::URLRequest> CreateTestUrlRequest(
      const GURL& url) {
    auto request =
        context_->CreateRequest(url, net::RequestPriority::DEFAULT_PRIORITY,
                                &test_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);

    return request;
  }

  void RegisterSharedStorageHandlerAndStartServer(
      std::string shared_storage_write) {
    DCHECK(test_server_);
    DCHECK(!test_server_->Started());
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &HandleSharedStorageRequestSimple, std::move(shared_storage_write)));
    test_server_->SetConnectionListener(listener_.get());
    ASSERT_TRUE(test_server_->Start());
  }

  void RegisterSharedStorageMultipleHandlerAndStartServer(
      std::vector<std::string> shared_storage_write_headers) {
    DCHECK(test_server_);
    DCHECK(!test_server_->Started());
    SharedStorageRequestCount::Reset();
    test_server_->RegisterRequestHandler(
        base::BindRepeating(&HandleSharedStorageRequestMultiple,
                            std::move(shared_storage_write_headers)));
    test_server_->SetConnectionListener(listener_.get());
    ASSERT_TRUE(test_server_->Start());
  }

  void RunProcessOutgoingRequest(net::URLRequest* request) {
    ASSERT_TRUE(request);
    ASSERT_TRUE(helper_);
    helper_->ProcessOutgoingRequest(*request);
  }

  void RunProcessIncomingResponse(net::URLRequest* request,
                                  bool expect_success) {
    ASSERT_TRUE(request);
    ASSERT_TRUE(helper_);
    base::test::TestFuture<void> future;
    EXPECT_EQ(expect_success,
              helper_->ProcessIncomingResponse(*request, future.GetCallback()));
    if (expect_success) {
      EXPECT_TRUE(future.Wait());
    }
  }

  [[nodiscard]] bool HasSharedStorageWriteResponseHeader(
      net::URLRequest* request,
      const std::string& expected_value) {
    std::string actual_value;
    bool has_header = request->response_headers() &&
                      request->response_headers()->GetNormalizedHeader(
                          kSharedStorageWriteHeader, &actual_value);
    if (has_header && actual_value != expected_value) {
      LOG(ERROR) << "actual header value: " << actual_value;
      LOG(ERROR) << "expected header value: " << expected_value;
    }
    return has_header && (actual_value == expected_value);
  }

 protected:
  std::unique_ptr<SharedStorageTestURLLoaderNetworkObserver> observer_;
  std::unique_ptr<SharedStorageRequestHelper> helper_;
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::test_server::SimpleConnectionListener> listener_;
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  PausableTestDelegate test_delegate_;
};

}  // namespace

TEST_F(SharedStorageRequestHelperTest,
       SharedStorageWritableHeaderRemoved_EligibityRemoved) {
  GURL request_url = GetHttpsRequestURL();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/true);
  EXPECT_TRUE(helper_->shared_storage_writable_eligible());

  std::vector<std::string> removed_headers(
      {std::string(kSecSharedStorageWritableHeader.data(),
                   kSecSharedStorageWritableHeader.size())});
  helper_->UpdateSharedStorageWritableEligible(removed_headers,
                                               /*modified_headers=*/{});
  EXPECT_FALSE(helper_->shared_storage_writable_eligible());
}

TEST_F(SharedStorageRequestHelperTest,
       SharedStorageWritableHeaderNotRemoved_EligibityNotRemoved) {
  GURL request_url = GetHttpsRequestURL();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/true);
  EXPECT_TRUE(helper_->shared_storage_writable_eligible());

  helper_->UpdateSharedStorageWritableEligible(/*removed_headers=*/{},
                                               /*modified_headers=*/{});
  EXPECT_TRUE(helper_->shared_storage_writable_eligible());
}

TEST_F(SharedStorageRequestHelperTest,
       SharedStorageWritableHeaderAdded_EligibityRestored) {
  GURL request_url = GetHttpsRequestURL();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/false);
  EXPECT_FALSE(helper_->shared_storage_writable_eligible());

  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader(kSecSharedStorageWritableHeader, "?1");
  helper_->UpdateSharedStorageWritableEligible(/*removed_headers=*/{},
                                               modified_headers);
  EXPECT_TRUE(helper_->shared_storage_writable_eligible());
}

TEST_F(SharedStorageRequestHelperTest,
       SharedStorageWritableHeaderNotAdded_EligibityNotRestored) {
  GURL request_url = GetHttpsRequestURL();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/false);
  EXPECT_FALSE(helper_->shared_storage_writable_eligible());

  helper_->UpdateSharedStorageWritableEligible(/*removed_headers=*/{},
                                               /*modified_headers=*/{});
  EXPECT_FALSE(helper_->shared_storage_writable_eligible());
}

TEST_F(SharedStorageRequestHelperTest,
       IncorrectSharedStorageWritableHeaderAdded_EligibityNotRestored) {
  GURL request_url = GetHttpsRequestURL();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/false);
  EXPECT_FALSE(helper_->shared_storage_writable_eligible());

  net::HttpRequestHeaders modified_headers;
  // Unparsable.
  modified_headers.SetHeader(kSecSharedStorageWritableHeader, "can=not=parse");
  helper_->UpdateSharedStorageWritableEligible(/*removed_headers=*/{},
                                               modified_headers);
  EXPECT_FALSE(helper_->shared_storage_writable_eligible());

  // Parses to a token item instead of a boolean item.
  modified_headers.SetHeader(kSecSharedStorageWritableHeader, "hello");
  helper_->UpdateSharedStorageWritableEligible(/*removed_headers=*/{},
                                               modified_headers);
  EXPECT_FALSE(helper_->shared_storage_writable_eligible());

  // Wrong boolean value.
  modified_headers.SetHeader(kSecSharedStorageWritableHeader, "?0");
  helper_->UpdateSharedStorageWritableEligible(/*removed_headers=*/{},
                                               modified_headers);
  EXPECT_FALSE(helper_->shared_storage_writable_eligible());
}

TEST_F(SharedStorageRequestHelperTest,
       ProcessOutgoingRequest_Eligible_RequestHeaderAdded) {
  GURL request_url = GetHttpsRequestURL();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/true);
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequest(request_url);
  RunProcessOutgoingRequest(request.get());

  EXPECT_THAT(request->extra_request_headers().GetHeader(
                  kSecSharedStorageWritableHeader),
              testing::Optional((kSecSharedStorageWritableValue)));
}

TEST_F(SharedStorageRequestHelperTest,
       ProcessOutgoingRequest_NotEligible_RequestHeaderNotAdded) {
  GURL request_url = GetHttpsRequestURL();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/false);
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequest(request_url);
  RunProcessOutgoingRequest(request.get());

  EXPECT_EQ(request->extra_request_headers().GetHeader(
                kSecSharedStorageWritableHeader),
            std::nullopt);
}

TEST_F(SharedStorageRequestHelperTest,
       ProcessIncomingResponse_NoHeader_NotEligible_NoResponseProcessed) {
  const std::string kHeader(
      "\"clear\", set;key=\"a\";value=val;ignore_if_present");
  RegisterSharedStorageHandlerAndStartServer(kHeader);

  const GURL kUrl = GetRequestURLFromServer();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/false);
  auto r = CreateTestUrlRequest(kUrl);
  r->Start();
  EXPECT_TRUE(r->is_pending());

  test_delegate_.WaitUntilResponseStarted();
  EXPECT_FALSE(HasSharedStorageWriteResponseHeader(r.get(), kHeader));

  RunProcessIncomingResponse(r.get(), /*expect_success=*/false);
  test_delegate_.ResumeOnResponseStarted();

  EXPECT_EQ(1, test_delegate_.response_started_count());
  EXPECT_TRUE(test_delegate_.data_received().empty());
  EXPECT_TRUE(observer_->headers_received().empty());
}

TEST_F(SharedStorageRequestHelperTest,
       ProcessIncomingResponse_NoHeader_Eligible_NoResponseProcessed) {
  const std::string kHeader(
      "\"clear\", set;key=\"a\";value=val;ignore_if_present");
  RegisterSharedStorageHandlerAndStartServer(kHeader);

  const GURL kUrl = GetRequestURLFromServer();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/true);
  auto r = CreateTestUrlRequest(kUrl);
  r->Start();
  EXPECT_TRUE(r->is_pending());

  test_delegate_.WaitUntilResponseStarted();
  EXPECT_FALSE(HasSharedStorageWriteResponseHeader(r.get(), kHeader));

  RunProcessIncomingResponse(r.get(), /*expect_success=*/false);
  test_delegate_.ResumeOnResponseStarted();

  EXPECT_EQ(1, test_delegate_.response_started_count());
  EXPECT_TRUE(test_delegate_.data_received().empty());
  EXPECT_TRUE(observer_->headers_received().empty());
}

TEST_F(SharedStorageRequestHelperTest,
       ProcessIncomingResponse_WithHeader_NotEligible_NoResponseProcessed) {
  const std::string kHeader(
      "\"clear\", set;key=\"a\";value=val;ignore_if_present");
  RegisterSharedStorageHandlerAndStartServer(kHeader);

  // Get a request whose response will have the Shared-Storage-Write header even
  // though we don't have a helper.
  const GURL kUrl = GetSharedStorageBypassRequestURLFromServer();
  CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/false);
  auto r = CreateTestUrlRequest(kUrl);
  r->Start();
  EXPECT_TRUE(r->is_pending());

  test_delegate_.WaitUntilResponseStarted();
  EXPECT_TRUE(HasSharedStorageWriteResponseHeader(r.get(), kHeader));

  RunProcessIncomingResponse(r.get(), /*expect_success=*/false);

  // Header has been removed.
  EXPECT_FALSE(HasSharedStorageWriteResponseHeader(r.get(), kHeader));
  test_delegate_.ResumeOnResponseStarted();

  EXPECT_EQ(1, test_delegate_.response_started_count());
  EXPECT_TRUE(observer_->headers_received().empty());
}

namespace {

// Tests `ProcessIncomingResponse()` when both the "Shared-Storage-Write"
// header is present and the `SharedStorageRequestHelper` is non-null.
class SharedStorageRequestHelperProcessHeaderTest
    : public SharedStorageRequestHelperTest {
 public:
  [[nodiscard]] std::unique_ptr<net::URLRequest> CreateSharedStorageRequest() {
    CreateSharedStorageRequestHelper(/*shared_storage_writable_eligible=*/true);
    GURL request_url = GetRequestURLFromServer();
    request_origin_ = url::Origin::Create(request_url);
    auto request = CreateTestUrlRequest(request_url);
    RunProcessOutgoingRequest(request.get());

    EXPECT_THAT(request->extra_request_headers().GetHeader(
                    kSecSharedStorageWritableHeader),
                testing::Optional(kSecSharedStorageWritableValue));
    return request;
  }

  void StartRequestAndProcessHeader(net::URLRequest* request,
                                    const std::string& expected_header,
                                    int expected_response_count = 1,
                                    bool expect_success = true) {
    request->Start();
    DCHECK(request->is_pending());
    test_delegate_.WaitUntilResponseStarted();
    EXPECT_TRUE(HasSharedStorageWriteResponseHeader(request, expected_header));
    RunProcessIncomingResponse(request, expect_success);

    // Header has been parsed and removed.
    EXPECT_FALSE(HasSharedStorageWriteResponseHeader(request, expected_header));
    test_delegate_.ResumeOnResponseStarted();

    EXPECT_EQ(test_delegate_.response_started_count(), expected_response_count);
  }

  void WaitForHeadersReceived(size_t expected_total) {
    observer_->FlushReceivers();
    observer_->WaitForHeadersReceived(expected_total);
  }

 protected:
  url::Origin request_origin_;
};

}  // namespace

TEST_F(SharedStorageRequestHelperProcessHeaderTest,
       ClearSetAppendDelete_TokensStringsBytes_HeaderReceived) {
  const std::string kHeader(
      "\"clear\", set;key=\"a\";value=val;ignore_if_present, "
      "append;key=a;value=:aGVsbG8=:, :ZGVsZXRl:;key=:YQ==:");
  RegisterSharedStorageHandlerAndStartServer(kHeader);

  auto r = CreateSharedStorageRequest();
  StartRequestAndProcessHeader(r.get(), kHeader);
  WaitForHeadersReceived(1);

  EXPECT_EQ(observer_->headers_received().size(), 1u);
  EXPECT_EQ(observer_->headers_received().front().first, request_origin_);
  EXPECT_THAT(
      observer_->headers_received().front().second,
      ElementsAre(std::make_tuple(mojom::SharedStorageOperationType::kClear,
                                  /*key=*/std::nullopt, /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt),
                  std::make_tuple(mojom::SharedStorageOperationType::kSet,
                                  /*key=*/"a", /*value=*/"val",
                                  /*ignore_if_present=*/true),
                  std::make_tuple(mojom::SharedStorageOperationType::kAppend,
                                  /*key=*/"a", /*value=*/"hello",
                                  /*ignore_if_present=*/std::nullopt),
                  std::make_tuple(mojom::SharedStorageOperationType::kDelete,
                                  /*key=*/"a", /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt)));
}

TEST_F(SharedStorageRequestHelperProcessHeaderTest,
       UnrecognizedAndExtraParams_HeaderReceived) {
  const std::string kHeader(
      "\"clear\";key=b,"
      "set;value=val;key=\"a\";ignore_if_present=?1;unknown=none;value=\"new "
      "value\","
      "append;key=a;value=:aGVsbG8=:;key=extra/key,"
      ":ZGVsZXRl:;key=:YQ==:;ignore_if_present=?0, will/skip;unknown=1,clear");
  RegisterSharedStorageHandlerAndStartServer(kHeader);

  auto r = CreateSharedStorageRequest();
  StartRequestAndProcessHeader(r.get(), kHeader);
  WaitForHeadersReceived(1);

  // The token, "will/skip;unknown=1", parses to a valid Structured Header
  // Parameterized List Item, but does not yield a valid operation type. The
  // `SharedStorageRequestHelper` skips over it and sends the other valid
  // operations it finds.
  EXPECT_EQ(observer_->headers_received().size(), 1u);
  EXPECT_EQ(observer_->headers_received().front().first, request_origin_);

  EXPECT_THAT(observer_->headers_received().front().second,
              ElementsAre(
                  // Recognized but superfluous parameters are included in the
                  // mojom struct, e.g. `key` for the first call to `clear`.
                  // They will be ignored in the browser process.
                  std::make_tuple(mojom::SharedStorageOperationType::kClear,
                                  /*key=*/"b", /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt),
                  // The unrecognized parameter `unknown` is omitted.
                  std::make_tuple(mojom::SharedStorageOperationType::kSet,
                                  /*key=*/"a", /*value=*/"new value",
                                  /*ignore_if_present=*/true),
                  // The second instance of `key` parameter is used.
                  std::make_tuple(mojom::SharedStorageOperationType::kAppend,
                                  /*key=*/"extra/key", /*value=*/"hello",
                                  /*ignore_if_present=*/std::nullopt),
                  std::make_tuple(mojom::SharedStorageOperationType::kDelete,
                                  /*key=*/"a", /*value=*/std::nullopt,
                                  /*ignore_if_present=*/false),
                  std::make_tuple(mojom::SharedStorageOperationType::kClear,
                                  /*key=*/std::nullopt, /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt)));
}

namespace {

class SharedStorageRequestHelperProcessHeaderMultiConnectionTest
    : public SharedStorageRequestHelperProcessHeaderTest {
 public:
  int GetNumExpectedConnections() const override { return 4; }
};

}  // namespace

TEST_F(SharedStorageRequestHelperProcessHeaderMultiConnectionTest,
       InvalidHeader_NoHeaderReceived) {
  std::vector<std::string> invalid_headers({
      "clear, invalid?item",       // Unparsable item
      "append;key=key=a;value=b",  // Unparsable parameter
      "set=a/dict, delete=a/key",  // Dictionary
      "",                          // Empty header
  });

  RegisterSharedStorageMultipleHandlerAndStartServer(invalid_headers);

  for (size_t i = 0; i < invalid_headers.size(); ++i) {
    test_delegate_.ResetDelegate();
    const std::string& expected_header = invalid_headers[i];
    auto r = CreateSharedStorageRequest();
    StartRequestAndProcessHeader(r.get(), expected_header,
                                 /*expected_response_count=*/i + 1,
                                 /*expect_success=*/false);
    observer_->FlushReceivers();

    // When parsing fails or the results of parsing are empty, nothing is sent
    // to the `observer_`.
    EXPECT_TRUE(observer_->headers_received().empty());
  }
}

TEST_F(SharedStorageRequestHelperProcessHeaderMultiConnectionTest,
       SkipInvalidMiddleItem_HeaderReceived) {
  std::vector<std::string> invalid_items({
      "unrecognized/operation",  // Item not recognized as valid
                                 // `mojom::SharedStorageOperationType`
      "(\"clear\" \"delete\")",  // Inner list
      "42",                      // Integer
      "3.14",                    // Decimal
  });

  std::vector<std::string> headers;
  for (const auto& item : invalid_items) {
    headers.emplace_back(
        base::StrCat({"set;key=x;value=y,", item, ",delete;key=z"}));
  }

  RegisterSharedStorageMultipleHandlerAndStartServer(headers);

  for (size_t i = 0; i < headers.size(); ++i) {
    test_delegate_.ResetDelegate();
    const std::string& expected_header = headers[i];
    auto r = CreateSharedStorageRequest();
    StartRequestAndProcessHeader(r.get(), expected_header,
                                 /*expected_response_count=*/i + 1);
    WaitForHeadersReceived(i + 1);

    EXPECT_EQ(observer_->headers_received().size(), i + 1);
    EXPECT_EQ(observer_->headers_received()[i].first, request_origin_);

    EXPECT_THAT(
        observer_->headers_received()[i].second,
        ElementsAre(std::make_tuple(mojom::SharedStorageOperationType::kSet,
                                    /*key=*/"x", /*value=*/"y",
                                    /*ignore_if_present=*/std::nullopt),
                    std::make_tuple(mojom::SharedStorageOperationType::kDelete,
                                    /*key=*/"z", /*value=*/std::nullopt,
                                    /*ignore_if_present=*/std::nullopt)));
  }
}

}  // namespace network
