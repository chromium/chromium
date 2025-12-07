// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_permissions_checker.h"

#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_permissions_cache.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// Very short time used by some tests that want to wait until just after a
// timer triggers.
constexpr base::TimeDelta kTinyTime = base::Microseconds(1);

// Response body that allows everything.
const char kAllowAllResponse[] = R"({
  "joinAdInterestGroup" : true,
  "leaveAdInterestGroup" : true
})";

// Response body that allows nothing.
const char kAllowNoneResponse[] = R"({
  "joinAdInterestGroup" : false,
  "leaveAdInterestGroup" : false
})";

// Single-use helper class to manage a PermissionsCheckCallback and track its
// result.
class BoolCallback {
 public:
  BoolCallback() = default;
  BoolCallback(const BoolCallback&) = delete;
  BoolCallback& operator=(const BoolCallback&) = delete;
  ~BoolCallback() = default;

  InterestGroupPermissionsChecker::PermissionsCheckCallback callback() {
    return base::BindOnce(&BoolCallback::CallbackInvoked,
                          base::Unretained(this));
  }

  // Waits for the callback to be invoked and returns the value passed to it.
  bool GetResult() {
    run_loop_.Run();
    return result_;
  }

  bool has_result() { return run_loop_.AnyQuitCalled(); }

 private:
  void CallbackInvoked(bool result) {
    EXPECT_FALSE(run_loop_.AnyQuitCalled());
    result_ = result;
    run_loop_.Quit();
  }

  bool result_;
  base::RunLoop run_loop_;
};

class InterestGroupPermissionsCheckerTestBase {
 protected:
  // Frame origin used in most tests.
  const url::Origin kFrameOrigin =
      url::Origin::Create(GURL("https://frame.test"));

  // Cross origin group, used by most tests.
  const url::Origin kGroupOrigin =
      url::Origin::Create(GURL("https://group.test"));

  const net::SchemefulSite kFrameSite = net::SchemefulSite(kFrameOrigin);

  // NetworkIsolationKey used in most tests.
  const net::NetworkIsolationKey kNetworkIsolationKey =
      net::NetworkIsolationKey(kFrameSite, kFrameSite);

  // .well-known URL when using `kFrameOrigin` and `kGroupOrigin`.
  const GURL validation_url_ = GURL(
      "https://group.test/.well-known/interest-group/permissions/"
      "?origin=https%3A%2F%2Fframe.test");

  base::test::TaskEnvironment task_environment_ = base::test::TaskEnvironment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  BoolCallback bool_callback_;
  network::TestURLLoaderFactory url_loader_factory_;
  InterestGroupPermissionsChecker interest_group_permissions_checker_;
};

// Some of these tests are paramaterized, some are not.

class InterestGroupPermissionsCheckerTest
    : public InterestGroupPermissionsCheckerTestBase,
      public testing::Test {};

class InterestGroupPermissionsCheckerParamaterizedTest
    : public InterestGroupPermissionsCheckerTestBase,
      public testing::TestWithParam<
          InterestGroupPermissionsChecker::Operation> {
 public:
  InterestGroupPermissionsChecker::Operation GetOperation() const {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    InterestGroupPermissionsCheckerParamaterizedTest,
    testing::Values(InterestGroupPermissionsChecker::Operation::kJoin,
                    InterestGroupPermissionsChecker::Operation::kLeave));

// Same origin operations should be allowed without a .well-known request.
TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, SameOrigin) {
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kFrameOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());

  // The callback should be invoked synchronously in this case, so that a
  // same-origin join followed by a running an auction should immediately
  EXPECT_TRUE(bool_callback_.has_result());

  EXPECT_TRUE(bool_callback_.GetResult());
  EXPECT_EQ(0u, url_loader_factory_.total_requests());
}

// Check a number of parameters set on the ResourceRequest that aren't worth the
// effort of writing integration tests for individually.
TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, RequestParameters) {
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());
  ASSERT_EQ(1u, url_loader_factory_.pending_requests()->size());

  const auto& request = (*url_loader_factory_.pending_requests())[0].request;
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit, request.credentials_mode);
  EXPECT_EQ(network::mojom::RequestMode::kCors, request.mode);
  EXPECT_EQ(kFrameOrigin, request.request_initiator);

  EXPECT_THAT(request.headers.GetHeader("Accept"),
              testing::Optional(std::string("application/json")));
}

TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, HttpError) {
  url_loader_factory_.AddResponse(validation_url_.spec(), kAllowAllResponse,
                                  net::HTTP_NOT_FOUND);
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());
  EXPECT_FALSE(bool_callback_.GetResult());
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, WrongMimeType) {
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, validation_url_,
                                         kAllowAllResponse);
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());
  EXPECT_FALSE(bool_callback_.GetResult());
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

// Test different response bodies, some using valid JSON, some not.
TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, ResponseBodyHandling) {
  struct {
    const char* response_body;
    std::set<InterestGroupPermissionsChecker::Operation> allowed_operations;
  } kTestCases[] = {
      // Not JSON.
      {"Look Mom, I'm on TV!", {}},

      // Not JSON dictionaries.
      {"42", {}},
      {"\"42\"", {}},
      {"[]", {}},
      {"[42]", {}},

      // JSON dictionaries with unexpected keys.
      {R"({"join": true, "leave":true})", {}},

      // Unexpected capitalization.
      {R"({"JoinAdInterestGroup": true, "leaveadinterestgroup":true})", {}},

      // Empty dictionary allows nothing.
      {"{}", {}},

      {kAllowAllResponse,
       {InterestGroupPermissionsChecker::Operation::kJoin,
        InterestGroupPermissionsChecker::Operation::kLeave}},

      {kAllowNoneResponse, {}},

      // One operation allowed, other not present.
      {R"({"joinAdInterestGroup" : true})",
       {InterestGroupPermissionsChecker::Operation::kJoin}},
      {R"({"leaveAdInterestGroup" : true})",
       {InterestGroupPermissionsChecker::Operation::kLeave}},

      // One operation allowed, other false.
      {R"({"joinAdInterestGroup" : true, "leaveAdInterestGroup" : false})",
       {InterestGroupPermissionsChecker::Operation::kJoin}},
      {R"({"joinAdInterestGroup" : false, "leaveAdInterestGroup" : true})",
       {InterestGroupPermissionsChecker::Operation::kLeave}},

      // One operation allowed, other not present, extra value.
      {R"({"joinAdInterestGroup" : true, "addMilk" : false})",
       {InterestGroupPermissionsChecker::Operation::kJoin}},
      {R"({"leaveAdInterestGroup" : true, "addMilk" : false})",
       {InterestGroupPermissionsChecker::Operation::kLeave}},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.response_body);

    // Since all requests use the same arguments, need to clear the cache
    // between calls.
    interest_group_permissions_checker_.ClearCache();

    BoolCallback bool_callback;
    auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                     test_case.response_body);
    interest_group_permissions_checker_.CheckPermissions(
        GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
        url_loader_factory_, bool_callback.callback());
    EXPECT_EQ(test_case.allowed_operations.count(GetOperation()) > 0u,
              bool_callback.GetResult());
  }
}

// Test the case where requests are merged. Both requests use the same
// Operation.
TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, SameOperationsMerged) {
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());
  BoolCallback bool_callback2;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback2.callback());

  auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                   kAllowAllResponse);

  EXPECT_TRUE(bool_callback_.GetResult());
  EXPECT_TRUE(bool_callback2.GetResult());

  // There should only have been one network request.
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

// Test case where requests are merged. Requests use different Operations.
TEST_F(InterestGroupPermissionsCheckerTest, DifferentOperationsMerged) {
  interest_group_permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kJoin, kFrameOrigin,
      kGroupOrigin, kNetworkIsolationKey, url_loader_factory_,
      bool_callback_.callback());
  BoolCallback bool_callback2;
  interest_group_permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kLeave, kFrameOrigin,
      kGroupOrigin, kNetworkIsolationKey, url_loader_factory_,
      bool_callback2.callback());

  auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                   kAllowAllResponse);

  EXPECT_TRUE(bool_callback_.GetResult());
  EXPECT_TRUE(bool_callback2.GetResult());

  // There should only have been one network request.
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

// Test case where requests are merged, with different Operations and different
// permissions.
TEST_F(InterestGroupPermissionsCheckerTest,
       DifferentOperationsMergedDifferentResults) {
  interest_group_permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kJoin, kFrameOrigin,
      kGroupOrigin, kNetworkIsolationKey, url_loader_factory_,
      bool_callback_.callback());
  BoolCallback bool_callback2;
  interest_group_permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kLeave, kFrameOrigin,
      kGroupOrigin, kNetworkIsolationKey, url_loader_factory_,
      bool_callback2.callback());

  auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                   R"({"joinAdInterestGroup" : true})");

  EXPECT_TRUE(bool_callback_.GetResult());
  EXPECT_FALSE(bool_callback2.GetResult());

  // There should only have been one network request.
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

// Test that permission checks with different frame origins can't be merged, and
// are cached separately.
TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, DifferentFrameOrigin) {
  // The only way two permissions checks from different frame origins can share
  // a NetworkIsolationKey is if they are same-site. So use an origin that's
  // same-site to kFrameOrigin, and check that they have the same
  // NetworkIsolationKey.
  const url::Origin kOtherFrameOrigin =
      url::Origin::Create(GURL("https://other.frame.test"));
  const net::SchemefulSite kOtherFrameSite =
      net::SchemefulSite(GURL("https://other.frame.test"));
  ASSERT_EQ(net::NetworkIsolationKey(kOtherFrameSite, kOtherFrameSite),
            kNetworkIsolationKey);
  const GURL kOtherValidationUrl(
      "https://group.test/.well-known/interest-group/permissions/"
      "?origin=https%3A%2F%2Fother.frame.test");

  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());
  BoolCallback bool_callback2;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kOtherFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback2.callback());

  auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                   kAllowAllResponse);
  auction_worklet::AddJsonResponse(&url_loader_factory_, kOtherValidationUrl,
                                   kAllowNoneResponse);

  EXPECT_TRUE(bool_callback_.GetResult());
  EXPECT_FALSE(bool_callback2.GetResult());

  // There should have been one network request for each frame owner.
  EXPECT_EQ(2u, url_loader_factory_.total_requests());

  // Repeat checks. Results should be the same, but with no new network
  // requests.
  BoolCallback bool_callback3;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback3.callback());
  BoolCallback bool_callback4;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kOtherFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback4.callback());
  EXPECT_TRUE(bool_callback3.GetResult());
  EXPECT_FALSE(bool_callback4.GetResult());
  EXPECT_EQ(2u, url_loader_factory_.total_requests());
}

// Test that permission checks with different interest group owners can't be
// merged, and are cached separately.
TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, DifferentOwner) {
  const url::Origin kOtherGroupOrigin =
      url::Origin::Create(GURL("https://group2.test"));
  const GURL kOtherValidationUrl(
      "https://group2.test/.well-known/interest-group/permissions/"
      "?origin=https%3A%2F%2Fframe.test");

  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());
  BoolCallback bool_callback2;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kOtherGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback2.callback());

  auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                   kAllowAllResponse);
  auction_worklet::AddJsonResponse(&url_loader_factory_, kOtherValidationUrl,
                                   kAllowNoneResponse);

  EXPECT_TRUE(bool_callback_.GetResult());
  EXPECT_FALSE(bool_callback2.GetResult());

  // There should have been one network request for each origin.
  EXPECT_EQ(2u, url_loader_factory_.total_requests());

  // Repeat checks. Results should be the same, but with no new network
  // requests.
  BoolCallback bool_callback3;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback3.callback());
  BoolCallback bool_callback4;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kOtherGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback4.callback());
  EXPECT_TRUE(bool_callback3.GetResult());
  EXPECT_FALSE(bool_callback4.GetResult());
  EXPECT_EQ(2u, url_loader_factory_.total_requests());
}

// Test that permission checks with different NetworkIsolationKeys can't be
// merged, and are cached separately.
TEST_P(InterestGroupPermissionsCheckerParamaterizedTest,
       DifferentNetworkIsolationKey) {
  const net::NetworkIsolationKey kOtherNetworkIsolationKey(
      net::SchemefulSite(GURL("https://top-frame.test")), kFrameSite);

  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());
  BoolCallback bool_callback2;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kOtherNetworkIsolationKey,
      url_loader_factory_, bool_callback2.callback());

  // There should be two pending network requests.
  ASSERT_EQ(2u, url_loader_factory_.pending_requests()->size());

  // Make the first response grant permissions and the second refuse
  // them. Since there's a single URLLoaderFactory pipe, the requests should be
  // in the order of the CheckPermissions calls above.
  for (int i = 0; i < 2; ++i) {
    auto& pending_request = (*url_loader_factory_.pending_requests())[i];
    EXPECT_EQ(validation_url_, pending_request.request.url);

    auto head = network::mojom::URLResponseHead::New();
    head->mime_type = "application/json";
    head->headers =
        net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n\r\n");
    ASSERT_TRUE(head->headers);

    mojo::ScopedDataPipeProducerHandle producer_handle;
    std::string response_body = i == 0 ? kAllowAllResponse : kAllowNoneResponse;
    mojo::ScopedDataPipeConsumerHandle body;
    ASSERT_EQ(mojo::CreateDataPipe(response_body.size(), producer_handle, body),
              MOJO_RESULT_OK);
    ASSERT_EQ(MOJO_RESULT_OK,
              producer_handle->WriteAllData(base::as_byte_span(response_body)));

    pending_request.client->OnReceiveResponse(std::move(head), std::move(body),
                                              std::nullopt);

    auto status = network::URLLoaderCompletionStatus();
    status.decoded_body_length = response_body.size();
    pending_request.client->OnComplete(status);
  }

  EXPECT_TRUE(bool_callback_.GetResult());
  EXPECT_FALSE(bool_callback2.GetResult());

  // Repeat checks. Results should be the same, but with no new network
  // requests.
  BoolCallback bool_callback3;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback3.callback());
  BoolCallback bool_callback4;
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kOtherNetworkIsolationKey,
      url_loader_factory_, bool_callback4.callback());
  EXPECT_TRUE(bool_callback3.GetResult());
  EXPECT_FALSE(bool_callback4.GetResult());
  EXPECT_EQ(2u, url_loader_factory_.total_requests());
}

// Check the case the same parameters are repeatedly fed into
// CheckPermissions(), both before and after a cache entry expires.
TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, CacheExpires) {
  // Set up a response that denies permissions, and send a request. Permissions
  // should be denied.
  auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                   kAllowNoneResponse);
  interest_group_permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kJoin, kFrameOrigin,
      kGroupOrigin, kNetworkIsolationKey, url_loader_factory_,
      bool_callback_.callback());
  EXPECT_FALSE(bool_callback_.GetResult());
  EXPECT_EQ(1u, url_loader_factory_.total_requests());

  // Make future responses allow permissions.
  auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                   kAllowAllResponse);

  // Wait until just before the cache entry expired. The original cached
  // response should be returned.
  task_environment_.FastForwardBy(
      InterestGroupPermissionsCache::kCacheDuration);
  BoolCallback bool_callback2;
  interest_group_permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kJoin, kFrameOrigin,
      kGroupOrigin, kNetworkIsolationKey, url_loader_factory_,
      bool_callback2.callback());
  EXPECT_FALSE(bool_callback2.GetResult());
  EXPECT_EQ(1u, url_loader_factory_.total_requests());

  // Wait until the cache entry expires and check permissions again. The result
  // should change, and there should be a new network request.
  task_environment_.FastForwardBy(kTinyTime);
  BoolCallback bool_callback3;
  interest_group_permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kJoin, kFrameOrigin,
      kGroupOrigin, kNetworkIsolationKey, url_loader_factory_,
      bool_callback3.callback());
  EXPECT_TRUE(bool_callback3.GetResult());
  EXPECT_EQ(2u, url_loader_factory_.total_requests());
}

TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, NonDefaultPorts) {
  const url::Origin kFrameOrigin =
      url::Origin::Create(GURL("https://frame.test:123"));
  const url::Origin kGroupOrigin =
      url::Origin::Create(GURL("https://group.test:456"));
  const GURL kValidationUrl = GURL(
      "https://group.test:456/.well-known/interest-group/permissions/"
      "?origin=https%3A%2F%2Fframe.test%3A123");

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kValidationUrl,
                                         kAllowAllResponse);

  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin,
      net::NetworkIsolationKey(kFrameSite, kFrameSite), url_loader_factory_,
      bool_callback_.callback());
  EXPECT_FALSE(bool_callback_.GetResult());
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, HttpTimeout) {
  const base::TimeDelta tiny_time = base::Milliseconds(1);
  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());
  EXPECT_FALSE(bool_callback_.has_result());

  task_environment_.FastForwardBy(
      InterestGroupPermissionsChecker::kRequestTimeout - tiny_time);
  EXPECT_FALSE(bool_callback_.has_result());
  EXPECT_EQ(1u, url_loader_factory_.total_requests());

  task_environment_.FastForwardBy(tiny_time);
  EXPECT_TRUE(bool_callback_.has_result());
  EXPECT_FALSE(bool_callback_.GetResult());
}

TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, MaxSize) {
  std::string response = kAllowAllResponse;
  response += std::string(
      InterestGroupPermissionsChecker::kMaxBodySize - response.size(), ' ');
  auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                   response);

  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());

  EXPECT_TRUE(bool_callback_.GetResult());
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_P(InterestGroupPermissionsCheckerParamaterizedTest, MaxSizeExceeded) {
  std::string response = kAllowAllResponse;
  response += std::string(
      InterestGroupPermissionsChecker::kMaxBodySize - response.size() + 1, ' ');
  auction_worklet::AddJsonResponse(&url_loader_factory_, validation_url_,
                                   response);

  interest_group_permissions_checker_.CheckPermissions(
      GetOperation(), kFrameOrigin, kGroupOrigin, kNetworkIsolationKey,
      url_loader_factory_, bool_callback_.callback());

  EXPECT_FALSE(bool_callback_.GetResult());
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

}  // namespace
}  // namespace content
