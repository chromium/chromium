// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/log/test_net_log_util.h"
#include "net/test/gtest_util.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/cors/cors_url_loader.h"
#include "services/network/cors/cors_url_loader_test_util.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_request.mojom-forward.h"
#include "services/network/test/mock_devtools_observer.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that Timing-Allow-Origin check passes when a same-origin redirect
// occurs. The redirect is as follows: [Origin] A -> A -> A.
namespace network::cors {
namespace {

class CorsURLLoaderTAOTest : public CorsURLLoaderTestBase {
 protected:
  void CreateLoaderAndStartNavigation(const GURL& origin, const GURL& url) {
    ResetFactory(std::nullopt /* initiator */, mojom::kBrowserProcessId);
    CreateLoaderAndStart(origin, url, mojom::RequestMode::kNavigate,
                         mojom::RedirectMode::kManual,
                         mojom::CredentialsMode::kInclude);
  }
};

TEST_F(CorsURLLoaderTAOTest, TAOCheckPassOnSameOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

TEST_F(CorsURLLoaderTAOTest, TAOCheckFailOnCrossOriginResource1) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // Note: this testcase will change when we change to the model in which TAO
  // passes whenever CORS is used.
  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

TEST_F(CorsURLLoaderTAOTest, TAOCheckFailOnCrossOriginNav1) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.html");

  CreateLoaderAndStartNavigation(origin, url);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // Note: this testcase will change when we change to the model in which TAO
  // passes whenever CORS is used.
  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

TEST_F(CorsURLLoaderTAOTest, TAOCheckFailOnCrossOriginResource2) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  // null does not work in this case since the tainted origin flag won't be set.
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

TEST_F(CorsURLLoaderTAOTest, TAOCheckFailOnCrossOriginNav2) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.html");

  CreateLoaderAndStartNavigation(origin, url);
  RunUntilCreateLoaderAndStartCalled();
  // null does not work in this case since the tainted origin flag won't be set.
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

TEST_F(CorsURLLoaderTAOTest, TAOCheckPassOnCrossOriginResource) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse(
      {{"Timing-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

TEST_F(CorsURLLoaderTAOTest, TAOCheckPassOnCrossOriginNav) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.html");

  CreateLoaderAndStartNavigation(origin, url);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse(
      {{"Timing-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> A where final redirect does not pass the check.
TEST_F(CorsURLLoaderTAOTest, TAOCheckFailRedirect1) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Timing-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  // This is insufficient: tainted origin flag will be set.
  NotifyLoaderClientOnReceiveResponse(
      {{"Timing-Allow-Origin",
        "https://example.com, https://other.example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> A where final redirect does not pass the check.
TEST_F(CorsURLLoaderTAOTest, TAOCheckFailRedirectNav1) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.html");
  const GURL new_url("https://example.com/bar.html");

  CreateLoaderAndStartNavigation(origin, url);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Timing-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  ClearHasReceivedRedirect();
  FollowRedirect();

  // This is insufficient: tainted origin flag will be set.
  NotifyLoaderClientOnReceiveResponse(
      {{"Timing-Allow-Origin",
        "https://example.com, https://other.example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> A where B does not pass the check.
TEST_F(CorsURLLoaderTAOTest, TAOCheckFailRedirect2) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse({{"Timing-Allow-Origin", "*"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> A where B does not pass the check.
TEST_F(CorsURLLoaderTAOTest, TAOCheckFailRedirectNav2) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.html");
  const GURL new_url("https://example.com/bar.html");

  CreateLoaderAndStartNavigation(origin, url);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse({{"Timing-Allow-Origin", "*"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> A
TEST_F(CorsURLLoaderTAOTest, TAOCheckPassRedirect1) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Timing-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse({{"Timing-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> A
TEST_F(CorsURLLoaderTAOTest, TAOCheckPassRedirectNav1) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.html");
  const GURL new_url("https://example.com/bar.html");

  CreateLoaderAndStartNavigation(origin, url);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Timing-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse({{"Timing-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> C
TEST_F(CorsURLLoaderTAOTest, TAOCheckPassRedirect2) {
  const GURL origin("https://example.com");
  const GURL url("https://other1.com/foo.png");
  const GURL new_url("https://other2.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Timing-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse({{"Timing-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> C
TEST_F(CorsURLLoaderTAOTest, TAOCheckPassRedirectNav2) {
  const GURL origin("https://example.com");
  const GURL url("https://other1.com/foo.html");
  const GURL new_url("https://other2.com/bar.html");

  CreateLoaderAndStartNavigation(origin, url);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Timing-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();
  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse({{"Timing-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

}  // namespace

}  // namespace network::cors
