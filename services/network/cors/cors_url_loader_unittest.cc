// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/log/test_net_log_util.h"
#include "net/storage_access_api/status.h"
#include "net/test/gtest_util.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/cookie_manager.h"
#include "services/network/cors/cors_url_loader_test_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_request.mojom-forward.h"
#include "services/network/test/mock_devtools_observer.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network::cors {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Optional;
using ::testing::Pointee;

class CorsURLLoaderTest : public CorsURLLoaderTestBase {};

class BadMessageTestHelper {
 public:
  BadMessageTestHelper()
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &BadMessageTestHelper::OnBadMessage, base::Unretained(this)));
  }

  BadMessageTestHelper(const BadMessageTestHelper&) = delete;
  BadMessageTestHelper& operator=(const BadMessageTestHelper&) = delete;

  ~BadMessageTestHelper() {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  const std::vector<std::string>& bad_message_reports() const {
    return bad_message_reports_;
  }

 private:
  void OnBadMessage(const std::string& reason) {
    bad_message_reports_.push_back(reason);
  }

  std::vector<std::string> bad_message_reports_;

  mojo::Message dummy_message_;
  mojo::internal::MessageDispatchContext context_;
};

TEST_F(CorsURLLoaderTest, NoCorsWithInvalidMethod) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);
  request.method = "GET\r\nHost: other.example.com";

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ElementsAre("CorsURLLoaderFactory: invalid characters in method"));
}

TEST_F(CorsURLLoaderTest, ForbiddenMethods) {
  const struct {
    std::string forbidden_method;
    bool expect_allowed_for_no_cors;
  } kTestCases[] = {
      // CONNECT is never allowed, while TRACE and TRACK are allowed only with
      // RequestMode::kNoCors.
      {"CONNECT", false},
      {"TRACE", true},
      {"TRACK", true},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.forbidden_method);
    for (const mojom::RequestMode mode :
         {mojom::RequestMode::kSameOrigin, mojom::RequestMode::kNoCors,
          mojom::RequestMode::kCors,
          mojom::RequestMode::kCorsWithForcedPreflight,
          mojom::RequestMode::kNavigate}) {
      SCOPED_TRACE(mode);

      const url::Origin default_initiator_origin =
          url::Origin::Create(GURL("https://example.com"));
      ResetFactory(
          url::Origin::Create(GURL("https://example.com")) /* initiator */,
          mojom::kBrowserProcessId);

      bool expect_allowed = (mode == mojom::RequestMode::kNoCors &&
                             test_case.expect_allowed_for_no_cors);

      ResourceRequest request;
      request.mode = mode;
      request.credentials_mode = mojom::CredentialsMode::kInclude;
      request.url = GURL("https://example.com/");
      request.request_initiator = url::Origin::Create(request.url);
      request.method = test_case.forbidden_method;

      BadMessageTestHelper bad_message_helper;
      CreateLoaderAndStart(request);
      if (expect_allowed) {
        RunUntilCreateLoaderAndStartCalled();
        NotifyLoaderClientOnReceiveResponse();
        NotifyLoaderClientOnComplete(net::OK);
      }
      RunUntilComplete();

      EXPECT_EQ(expect_allowed, IsNetworkLoaderStarted());
      EXPECT_FALSE(client().has_received_redirect());
      EXPECT_EQ(expect_allowed, client().has_received_response());
      EXPECT_TRUE(client().has_received_completion());
      if (expect_allowed) {
        EXPECT_THAT(client().completion_status().error_code, net::test::IsOk());
        EXPECT_THAT(bad_message_helper.bad_message_reports(), IsEmpty());
      } else {
        EXPECT_THAT(client().completion_status().error_code,
                    net::test::IsError(net::ERR_INVALID_ARGUMENT));
        EXPECT_THAT(bad_message_helper.bad_message_reports(),
                    ElementsAre("CorsURLLoaderFactory: Forbidden method"));
      }
    }
  }
}

TEST_F(CorsURLLoaderTest, SameOriginWithoutInitiator) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kSameOrigin;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = std::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ::testing::ElementsAre("CorsURLLoaderFactory: cors without initiator"));
}

TEST_F(CorsURLLoaderTest, NoCorsWithoutInitiator) {
  // This test needs to simulate a factory used from the browser process,
  // because only the browser process may start requests with no
  // `request_initiator`.  A renderer process would have run into NOTREACHED and
  // mojo::ReportBadMessage via InitiatorLockCompatibility::kNoInitiator case in
  // CorsURLLoaderFactory::IsValidRequest.
  ResetFactory(std::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = std::nullopt;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CorsWithoutInitiator) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = std::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ElementsAre("CorsURLLoaderFactory: cors without initiator"));
}

TEST_F(CorsURLLoaderTest, NavigateWithoutInitiator) {
  ResetFactory(std::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = std::nullopt;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, NavigateWithEarlyHints) {
  ResetFactory(std::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = std::nullopt;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveEarlyHints();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_early_hints());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, NavigationFromRenderer) {
  ResetFactory(url::Origin::Create(GURL("https://example.com/")),
               kRendererProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.url = GURL("https://some.other.example.com/");
  request.navigation_redirect_chain.push_back(request.url);
  request.request_initiator = std::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ElementsAre("CorsURLLoaderFactory: lock VS initiator mismatch"));
}

TEST_F(CorsURLLoaderTest, SameOriginRequest) {
  const GURL url("https://example.com/foo.png");
  CreateLoaderAndStart(url.DeprecatedGetOriginAsURL(), url,
                       mojom::RequestMode::kSameOrigin);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, SameOriginRequestWithEarlyHints) {
  const GURL url("https://example.com/foo.png");
  CreateLoaderAndStart(url.DeprecatedGetOriginAsURL(), url,
                       mojom::RequestMode::kSameOrigin);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveEarlyHints();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  // client() should not receive Early Hints since the request is not
  // navigation.
  EXPECT_FALSE(client().has_received_early_hints());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithNoCorsMode) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_FALSE(
      GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithNoCorsModeAndPatchMethod) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = "PATCH";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_EQ(GetRequest().headers.GetHeader(net::HttpRequestHeaders::kOrigin),
            "https://example.com");
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestFetchRequestModeSameOrigin) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kSameOrigin);

  RunUntilComplete();

  // This call never hits the network URLLoader (i.e. the TestURLLoaderFactory)
  // because it is fails right away.
  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kDisallowedByMode,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithCorsModeButMissingCorsHeader) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_EQ(GetRequest().headers.GetHeader(net::HttpRequestHeaders::kOrigin),
            "https://example.com");
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kMissingAllowOriginHeader,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithCorsMode) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest,
       CrossOriginRequestFetchRequestWithCorsModeButMismatchedCorsHeader) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "http://some-other-domain.com"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kAllowOriginMismatch,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CorsURLLoaderTest, CorsEnabledSameCustomSchemeRequest) {
  // Custom scheme should not be permitted by default.
  const GURL origin("my-scheme://foo/index.html");
  const GURL url("my-scheme://bar/baz.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kCorsDisabledScheme,
            client().completion_status().cors_error_status->cors_error);

  // Scheme check can be skipped via the factory params.
  ResetFactoryParams factory_params;
  factory_params.skip_cors_enabled_scheme_check = true;
  ResetFactory(url::Origin::Create(origin), mojom::kBrowserProcessId,
               factory_params);

  // "Access-Control-Allow-Origin: *" accepts the custom scheme.
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse({{"Access-Control-Allow-Origin", "*"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // "Access-Control-Allow-Origin: null" accepts the custom scheme as a custom
  // scheme is an opaque origin.
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, StripUsernameAndPassword) {
  const GURL origin("https://example.com");
  const GURL url("http://foo:bar@other.example.com/foo.png");
  std::string stripped_url = "http://other.example.com/foo.png";
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_EQ(stripped_url, GetRequestedURL().spec());
}

TEST_F(CorsURLLoaderTest, CorsCheckPassOnRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());
}

TEST_F(CorsURLLoaderTest, CorsCheckFailOnRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(client().completion_status().error_code, net::ERR_FAILED);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(client().completion_status().cors_error_status->cors_error,
            mojom::CorsError::kMissingAllowOriginHeader);
}

TEST_F(CorsURLLoaderTest, NetworkLoaderErrorDuringRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  // Underlying network::URLLoader may call OnComplete with an error at anytime.
  NotifyLoaderClientOnComplete(net::ERR_FAILED);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());
}

TEST_F(CorsURLLoaderTest, SameOriginToSameOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, SameOriginToCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://other.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  RunUntilCreateLoaderAndStartCalled();

  // A new loader is created.
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CrossOriginToCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});

  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CrossOriginToOriginalOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse();

  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  // We got redirected back to the original origin, but we need an
  // access-control-allow-origin header, and we don't have it in this test case.
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(client().completion_status().cors_error_status->cors_error,
            mojom::CorsError::kMissingAllowOriginHeader);
}

TEST_F(CorsURLLoaderTest, CrossOriginToAnotherCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  // The request is tained, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest,
       CrossOriginToAnotherCrossOriginRedirectWithPreflight) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest original_request;
  original_request.mode = mojom::RequestMode::kCors;
  original_request.credentials_mode = mojom::CredentialsMode::kOmit;
  original_request.method = "PATCH";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(original_request);
  RunUntilCreateLoaderAndStartCalled();

  // preflight request
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"},
       {"Access-Control-Allow-Methods", "PATCH"}});
  RunUntilCreateLoaderAndStartCalled();

  // the actual request
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "PATCH");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "PATCH", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilCreateLoaderAndStartCalled();

  // the second preflight request
  EXPECT_EQ(3, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");
  ASSERT_TRUE(GetRequest().request_initiator);
  EXPECT_EQ(GetRequest().request_initiator->Serialize(), "https://example.com");

  // The request is tainted, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"},
       {"Access-Control-Allow-Methods", "PATCH"}});
  RunUntilCreateLoaderAndStartCalled();

  // the second actual request
  EXPECT_EQ(4, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);
  EXPECT_EQ(GetRequest().method, "PATCH");
  ASSERT_TRUE(GetRequest().request_initiator);
  EXPECT_EQ(GetRequest().request_initiator->Serialize(), "https://example.com");

  // The request is tainted, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CrossOriginPreflightReceiveRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest original_request;
  original_request.mode = mojom::RequestMode::kCors;
  original_request.credentials_mode = mojom::CredentialsMode::kOmit;
  original_request.method = "GET";
  original_request.headers.SetHeader("Content-type", "application/json");
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(original_request);
  RunUntilCreateLoaderAndStartCalled();

  // preflight request
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "OPTIONS", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"},
       {"Access-Control-Allow-Headers", "Content-type"}});

  RunUntilComplete();
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);

  std::vector<net::NetLogEntry> entries = GetEntries();
  std::vector<net::NetLogEventType> types = GetTypesOfNetLogEntries(entries);
  EXPECT_THAT(types,
              Contains(net::NetLogEventType::CORS_PREFLIGHT_RESULT).Times(0));
  ASSERT_THAT(types,
              Contains(net::NetLogEventType::CORS_PREFLIGHT_ERROR).Times(1));

  const net::NetLogEntry* entry =
      FindEntryByType(entries, net::NetLogEventType::CORS_PREFLIGHT_ERROR);
  EXPECT_THAT(entry->params.FindString("error"), Pointee(Eq("ERR_FAILED")));
  EXPECT_THAT(entry->params.FindInt("cors-error"),
              Optional(Eq(static_cast<int>(
                  mojom::CorsError::kPreflightDisallowedRedirect))));
  EXPECT_THAT(entry->params.FindString("failed-parameter"), IsNull());
}

TEST_F(CorsURLLoaderTest, RedirectInfoShouldBeUsed) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://other.example.com/foo.png");

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "POST";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.referrer = url;
  request.referrer_policy =
      net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(url, GetRequest().url);
  EXPECT_EQ("POST", GetRequest().method);
  EXPECT_EQ(url, GetRequest().referrer);
  EXPECT_EQ(net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN,
            GetRequest().referrer_policy);

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(
      303, "GET", new_url, "https://other.example.com",
      net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(new_url, GetRequest().url);
  EXPECT_EQ("GET", GetRequest().method);
  EXPECT_EQ(GURL("https://other.example.com"), GetRequest().referrer);
  EXPECT_EQ(net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN,
            GetRequest().referrer_policy);

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Makes sure that if an intercepted redirect updates the IsolationInfo and the
// SiteForCookies values, the CorsURLLoader respects those changes. The former
// only happens for frames, and the latter for subframes, but should make
// assumptions about whether these need to be updated in CorsURLLoader.
TEST_F(CorsURLLoaderTest,
       InterceptedRedirectChangesIsolationInfoAndSiteForCookies) {
  const GURL url("https://example.com/foo.png");
  const url::Origin url_origin = url::Origin::Create(url);
  const net::SiteForCookies url_site_for_cookies =
      net::SiteForCookies::FromOrigin(url_origin);

  const GURL new_url("https://other.example.com/foo.png");
  const url::Origin new_url_origin = url::Origin::Create(new_url);
  const net::SiteForCookies new_url_site_for_cookies =
      net::SiteForCookies::FromOrigin(new_url_origin);

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(url_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = url;
  request.request_initiator = url_origin;
  request.site_for_cookies = url_site_for_cookies;
  request.update_first_party_url_on_redirect = true;
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame,
      url_origin /* top_frame_origin */, url_origin /* frame_origin */,
      url_site_for_cookies);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(url, GetRequest().url);

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(
      303, "GET", new_url, "" /* referrer */, net::ReferrerPolicy::NO_REFERRER,
      new_url_site_for_cookies));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(new_url, GetRequest().url);
  EXPECT_EQ("GET", GetRequest().method);
  EXPECT_TRUE(
      GetRequest().site_for_cookies.IsEquivalent(new_url_site_for_cookies));
  EXPECT_TRUE(GetRequest().trusted_params->isolation_info.IsEqualForTesting(
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                 new_url_origin /* top_frame_origin */,
                                 new_url_origin /* frame_origin */,
                                 new_url_site_for_cookies)));

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, TooManyRedirects) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();
  for (int i = 0; i < 20; ++i) {
    EXPECT_EQ(1, num_created_loaders());

    GURL new_url(base::StringPrintf("https://example.com/foo.png?%d", i));
    NotifyLoaderClientOnReceiveRedirect(
        CreateRedirectInfo(301, "GET", new_url));

    RunUntilRedirectReceived();
    ASSERT_TRUE(client().has_received_redirect());
    ASSERT_FALSE(client().has_received_response());
    ASSERT_FALSE(client().has_received_completion());

    ClearHasReceivedRedirect();
    FollowRedirect();
  }

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://example.com/bar.png")));
  RunUntilComplete();
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_TOO_MANY_REDIRECTS,
            client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, FollowErrorRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  ResourceRequest original_request;
  original_request.mode = mojom::RequestMode::kCors;
  original_request.credentials_mode = mojom::CredentialsMode::kOmit;
  original_request.redirect_mode = mojom::RedirectMode::kError;
  original_request.method = "GET";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(original_request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CorsExemptHeaderRemovalOnCrossOriginRedirects) {
  ResourceRequest request;
  request.url = GURL("https://example.com/foo.png");
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));
  request.mode = mojom::RequestMode::kCors;
  request.cors_exempt_headers.SetHeader(kTestCorsExemptHeader, "test-value");
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  EXPECT_EQ(1, num_created_loaders());

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(
      301, "GET", GURL("https://other.example.com/bar.png")));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  ASSERT_TRUE(client().has_received_redirect());
  ASSERT_FALSE(client().has_received_response());
  ASSERT_FALSE(client().has_received_completion());
  EXPECT_TRUE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));

  FollowRedirect({kTestCorsExemptHeader});
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(2, num_created_loaders());
  EXPECT_FALSE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));
}

TEST_F(CorsURLLoaderTest, CorsExemptHeaderModificationOnRedirects) {
  ResourceRequest request;
  request.url = GURL("https://example.com/foo.png");
  request.request_initiator = url::Origin::Create(request.url);
  request.cors_exempt_headers.SetHeader(kTestCorsExemptHeader, "test-value");
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  EXPECT_EQ(1, num_created_loaders());

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://example.com/bar.png")));
  RunUntilRedirectReceived();

  ASSERT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_TRUE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));

  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader(kTestCorsExemptHeader, "test-modified");
  FollowRedirect({}, modified_headers);
  RunUntilComplete();

  ASSERT_EQ(1, num_created_loaders());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  ASSERT_TRUE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));
}

// Tests if OriginAccessList is actually used to decide the cors flag.
// Details for the OriginAccessList behaviors are verified in
// OriginAccessListTest, but this test intends to verify if CorsURlLoader calls
// the list properly.
TEST_F(CorsURLLoaderTest, OriginAccessList_Allowed) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  // Adds an entry to allow the cross origin request beyond the CORS
  // rules.
  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head()->response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Tests if CorsURLLoader takes into account
// ResourceRequest::isolated_world_origin when consulting OriginAccessList.
TEST_F(CorsURLLoaderTest, OriginAccessList_IsolatedWorldOrigin) {
  const url::Origin main_world_origin =
      url::Origin::Create(GURL("http://main-world.example.com"));
  const url::Origin isolated_world_origin =
      url::Origin::Create(GURL("http://isolated-world.example.com"));
  const GURL url("http://other.example.com/foo.png");

  ResetFactoryParams factory_params;
  factory_params.ignore_isolated_world_origin = false;
  ResetFactory(main_world_origin, kRendererProcessId, factory_params);

  AddAllowListEntryForOrigin(isolated_world_origin, url.scheme(), url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = main_world_origin;
  request.isolated_world_origin = isolated_world_origin;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  ASSERT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head()->response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Tests if CorsURLLoader takes into account
// ResourceRequest::isolated_world_origin when consulting OriginAccessList
// after redirects.
TEST_F(CorsURLLoaderTest, OriginAccessList_IsolatedWorldOrigin_Redirect) {
  const url::Origin main_world_origin =
      url::Origin::Create(GURL("http://main-world.example.com"));
  const url::Origin isolated_world_origin =
      url::Origin::Create(GURL("http://isolated-world.example.com"));
  const GURL url("http://other.example.com/foo.png");
  // `new_url` is same-origin as `url` to avoid tainting the response
  // in CorsURLLoader::OnReceiveRedirect.
  const GURL new_url("http://other.example.com/bar.png");

  ResetFactoryParams factory_params;
  factory_params.ignore_isolated_world_origin = false;
  ResetFactory(main_world_origin, kRendererProcessId, factory_params);

  AddAllowListEntryForOrigin(isolated_world_origin, url.scheme(), url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);
  AddAllowListEntryForOrigin(isolated_world_origin, new_url.scheme(),
                             new_url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  ResourceRequest request;
  // Using no-cors to force opaque response (unless the allowlist entry added
  // above is taken into account).
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = main_world_origin;
  request.isolated_world_origin = isolated_world_origin;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  FollowRedirect();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_redirect());
  ASSERT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head()->response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Tests if CorsURLLoader takes ignores ResourceRequest::isolated_world_origin
// when URLLoaderFactoryParams::ignore_isolated_world_origin is set to true.
TEST_F(CorsURLLoaderTest, OriginAccessList_IsolatedWorldOriginIgnored) {
  const url::Origin main_world_origin =
      url::Origin::Create(GURL("http://main-world.example.com"));
  const url::Origin isolated_world_origin =
      url::Origin::Create(GURL("http://isolated-world.example.com"));
  const GURL url("http://other.example.com/foo.png");

  ResetFactory(main_world_origin, kRendererProcessId);

  AddAllowListEntryForOrigin(isolated_world_origin, url.scheme(), url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = main_world_origin;
  request.isolated_world_origin = isolated_world_origin;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

// Check if higher-priority block list wins.
TEST_F(CorsURLLoaderTest, OriginAccessList_Blocked) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);
  AddBlockListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

// Tests if OriginAccessList is actually used to decide response tainting.
TEST_F(CorsURLLoaderTest, OriginAccessList_NoCors) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  // Adds an entry to allow the cross origin request without using
  // CORS.
  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head()->response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, OriginAccessList_POST) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  // Adds an entry to allow the cross origin request beyond the CORS
  // rules.
  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "POST";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  // preflight request
  ASSERT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "POST");
  EXPECT_EQ(GetRequest().headers.GetHeader("origin"),
            url::Origin::Create(origin).Serialize());
}

TEST_F(CorsURLLoaderTest, 304ForSimpleRevalidation) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.headers.SetHeader("If-Modified-Since", "x");
  request.headers.SetHeader("If-None-Match", "y");
  request.headers.SetHeader("Cache-Control", "z");
  request.is_revalidating = true;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  // No preflight, no CORS response headers.
  NotifyLoaderClientOnReceiveResponse(304, {});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, 304ForSimpleGet) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  // No preflight, no CORS response headers.
  NotifyLoaderClientOnReceiveResponse(304, {});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, 200ForSimpleRevalidation) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.headers.SetHeader("If-Modified-Since", "x");
  request.headers.SetHeader("If-None-Match", "y");
  request.headers.SetHeader("Cache-Control", "z");
  request.is_revalidating = true;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  // No preflight, no CORS response headers.
  NotifyLoaderClientOnReceiveResponse(200, {});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, RevalidationAndPreflight) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest original_request;
  original_request.mode = mojom::RequestMode::kCors;
  original_request.credentials_mode = mojom::CredentialsMode::kOmit;
  original_request.method = "GET";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  original_request.headers.SetHeader("If-Modified-Since", "x");
  original_request.headers.SetHeader("If-None-Match", "y");
  original_request.headers.SetHeader("Cache-Control", "z");
  original_request.headers.SetHeader("foo", "bar");
  original_request.is_revalidating = true;
  CreateLoaderAndStart(original_request);
  RunUntilCreateLoaderAndStartCalled();

  // preflight request
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");
  EXPECT_EQ(GetRequest().headers.GetHeader("access-control-request-headers"),
            "foo");

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"},
       {"Access-Control-Allow-Headers", "foo"}});
  RunUntilCreateLoaderAndStartCalled();

  // the actual request
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Keep this in sync with the CalculateResponseTainting test in
// Blink's cors_test.cc.
TEST(CorsURLLoaderTaintingTest, CalculateResponseTainting) {
  using mojom::FetchResponseType;
  using mojom::RequestMode;

  const GURL same_origin_url("https://example.com/");
  const GURL cross_origin_url("https://example2.com/");
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  const std::optional<url::Origin> no_origin;

  OriginAccessList origin_access_list;

  // CORS flag is false, same-origin request
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kSameOrigin, origin, std::nullopt,
                false, false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kNoCors, origin, std::nullopt,
                false, false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kCors, origin, std::nullopt,
                false, false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                std::nullopt, false, false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kNavigate, origin, std::nullopt,
                false, false, origin_access_list));

  // CORS flag is false, cross-origin request
  EXPECT_EQ(FetchResponseType::kOpaque,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                cross_origin_url, RequestMode::kNoCors, origin, std::nullopt,
                false, false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                cross_origin_url, RequestMode::kNavigate, origin, std::nullopt,
                false, false, origin_access_list));

  // CORS flag is true, same-origin request
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kCors, origin, std::nullopt, true,
                false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                std::nullopt, true, false, origin_access_list));

  // CORS flag is true, cross-origin request
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                cross_origin_url, RequestMode::kCors, origin, std::nullopt,
                true, false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                cross_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                std::nullopt, true, false, origin_access_list));

  // Origin is not provided.
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kNoCors, no_origin, std::nullopt,
                false, false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kNavigate, no_origin,
                std::nullopt, false, false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                cross_origin_url, RequestMode::kNoCors, no_origin, std::nullopt,
                false, false, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                cross_origin_url, RequestMode::kNavigate, no_origin,
                std::nullopt, false, false, origin_access_list));

  // Tainted origin.
  EXPECT_EQ(FetchResponseType::kOpaque,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kNoCors, origin, std::nullopt,
                false, true, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                std::nullopt, false, true, origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTaintingForTesting(
                same_origin_url, RequestMode::kNavigate, origin, std::nullopt,
                false, true, origin_access_list));
}

TEST_F(CorsURLLoaderTest, RequestWithHostHeaderFails) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("https://example.com/path");
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));
  request.headers.SetHeader(net::HttpRequestHeaders::kHost,
                            "other.example.com");
  CreateLoaderAndStart(request);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, RequestWithProxyAuthorizationHeaderFails) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("https://example.com/path");
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));
  request.headers.SetHeader(net::HttpRequestHeaders::kProxyAuthorization,
                            "Basic Zm9vOmJhcg==");
  CreateLoaderAndStart(request);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, SetHostHeaderOnRedirectFails) {
  CreateLoaderAndStart(GURL("https://example.com/"),
                       GURL("https://example.com/path"),
                       mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://redirect.test/")));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());

  ClearHasReceivedRedirect();
  // This should cause the request to fail.
  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader(net::HttpRequestHeaders::kHost, "bar.test");
  FollowRedirect({} /* removed_headers */, modified_headers);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, SetProxyAuthorizationHeaderOnRedirectFails) {
  CreateLoaderAndStart(GURL("https://example.com/"),
                       GURL("https://example.com/path"),
                       mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://redirect.test/")));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());

  ClearHasReceivedRedirect();
  // This should cause the request to fail.
  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader(net::HttpRequestHeaders::kProxyAuthorization,
                             "Basic Zm9vOmJhcg==");
  FollowRedirect({} /* removed_headers */, modified_headers);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, SameOriginCredentialsModeWithoutInitiator) {
  // This test needs to simulate a factory used from the browser process,
  // because only the browser process may start requests with no
  // `request_initiator`.  A renderer process would have run into NOTREACHED and
  // mojo::ReportBadMessage via InitiatorLockCompatibility::kNoInitiator case in
  // CorsURLLoaderFactory::IsValidRequest.
  ResetFactory(std::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kSameOrigin;
  request.url = GURL("https://example.com/");
  request.request_initiator = std::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ElementsAre("CorsURLLoaderFactory: same-origin "
                          "credentials mode without initiator"));
}

TEST_F(CorsURLLoaderTest, SameOriginCredentialsModeOnNavigation) {
  ResetFactory(std::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.credentials_mode = mojom::CredentialsMode::kSameOrigin;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ElementsAre(
          "CorsURLLoaderFactory: unsupported credentials mode on navigation"));
}

TEST_F(CorsURLLoaderTest, OmitCredentialsModeOnNavigation) {
  ResetFactory(std::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ElementsAre(
          "CorsURLLoaderFactory: unsupported credentials mode on navigation"));
}

// Make sure than when a request is failed due to having `trusted_params` set
// and being sent to an untrusted URLLoaderFactory, no CORS request is made.
TEST_F(CorsURLLoaderTest, TrustedParamsWithUntrustedFactoryFailsBeforeCORS) {
  url::Origin initiator = url::Origin::Create(GURL("https://example.com"));

  // Run the test with a trusted URLLoaderFactory as well, to make sure a CORS
  // request is in fact made when using a trusted factory.
  for (bool is_trusted : {false, true}) {
    ResetFactoryParams factory_params;
    factory_params.is_trusted = is_trusted;
    ResetFactory(initiator, kRendererProcessId, factory_params);

    BadMessageTestHelper bad_message_helper;

    ResourceRequest request;
    request.mode = mojom::RequestMode::kCors;
    request.credentials_mode = mojom::CredentialsMode::kOmit;
    request.method = net::HttpRequestHeaders::kGetMethod;
    request.url = GURL("http://other.example.com/foo.png");
    request.request_initiator = initiator;
    request.trusted_params = ResourceRequest::TrustedParams();
    CreateLoaderAndStart(request);

    if (!is_trusted) {
      RunUntilComplete();
      EXPECT_FALSE(IsNetworkLoaderStarted());
      EXPECT_FALSE(client().has_received_redirect());
      EXPECT_FALSE(client().has_received_response());
      EXPECT_TRUE(client().has_received_completion());
      EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
                client().completion_status().error_code);
      EXPECT_THAT(
          bad_message_helper.bad_message_reports(),
          ElementsAre(
              "CorsURLLoaderFactory: Untrusted caller making trusted request"));
    } else {
      RunUntilCreateLoaderAndStartCalled();
      NotifyLoaderClientOnReceiveResponse(
          {{"Access-Control-Allow-Origin", "https://example.com"}});
      NotifyLoaderClientOnComplete(net::OK);

      RunUntilComplete();

      EXPECT_TRUE(IsNetworkLoaderStarted());
      EXPECT_TRUE(client().has_received_response());
      EXPECT_TRUE(client().has_received_completion());
      EXPECT_EQ(net::OK, client().completion_status().error_code);
      EXPECT_TRUE(
          GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
    }
  }
}

// Test that when a request has LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME and a
// NetworkAnonymizationKey, CorsURLLoaderFactory does not reject the request.
TEST_F(CorsURLLoaderTest, RestrictedPrefetchSucceedsWithNIK) {
  url::Origin initiator = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator, kRendererProcessId, factory_params);

  BadMessageTestHelper bad_message_helper;

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("http://other.example.com/foo.png");
  request.request_initiator = initiator;
  request.load_flags |= net::LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  request.trusted_params = ResourceRequest::TrustedParams();

  // Fill up the `trusted_params` NetworkAnonymizationKey member.
  url::Origin request_origin = url::Origin::Create(request.url);
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, request_origin, request_origin,
      net::SiteForCookies());

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_TRUE(GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
}

// Test that when a request has LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME but no
// NetworkAnonymizationKey, CorsURLLoaderFactory rejects the request. This is
// because the LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME flag must only appear on
// requests that make use of their TrustedParams' `isolation_info`.
TEST_F(CorsURLLoaderTest, RestrictedPrefetchFailsWithoutNIK) {
  url::Origin initiator = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator, kRendererProcessId, factory_params);

  BadMessageTestHelper bad_message_helper;

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("http://other.example.com/foo.png");
  request.request_initiator = initiator;
  request.load_flags |= net::LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  request.trusted_params = ResourceRequest::TrustedParams();

  CreateLoaderAndStart(request);

  RunUntilComplete();
  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ElementsAre(
          "CorsURLLoaderFactory: Request with "
          "LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME flag is not trusted"));
}

TEST_F(CorsURLLoaderTest, DevToolsObserverOnCorsErrorCallback) {
  const GURL origin("https://example.com");
  const url::Origin initiator_origin = url::Origin::Create(origin);

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  const GURL url("http://other.example.com/foo.png");
  MockDevToolsObserver devtools_observer;
  set_devtools_observer_for_next_request(&devtools_observer);
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kSameOrigin);

  RunUntilComplete();

  // This call never hits the network URLLoader (i.e. the TestURLLoaderFactory)
  // because it is fails right away.
  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kDisallowedByMode,
            client().completion_status().cors_error_status->cors_error);
  devtools_observer.WaitUntilCorsError();
  EXPECT_TRUE(devtools_observer.cors_error_params());
  const network::MockDevToolsObserver::OnCorsErrorParams& params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(mojom::CorsError::kDisallowedByMode, params.status->cors_error);
  EXPECT_EQ(initiator_origin, params.initiator_origin);
  EXPECT_EQ(url, params.url);
}

// Tests if CheckRedirectLocation detects kCorsDisabledScheme and
// kRedirectContainsCredentials errors correctly.
TEST_F(CorsURLLoaderTest, CheckRedirectLocation) {
  struct TestCase {
    GURL url;
    mojom::RequestMode request_mode;
    bool cors_flag;
    bool tainted;
    std::optional<CorsErrorStatus> expectation;
  };

  const auto kCors = mojom::RequestMode::kCors;
  const auto kCorsWithForcedPreflight =
      mojom::RequestMode::kCorsWithForcedPreflight;
  const auto kNoCors = mojom::RequestMode::kNoCors;

  const url::Origin origin = url::Origin::Create(GURL("http://example.com/"));
  const GURL same_origin_url("http://example.com/");
  const GURL cross_origin_url("http://example2.com/");
  const GURL data_url("data:,Hello");
  const GURL same_origin_url_with_user("http://yukari@example.com/");
  const GURL same_origin_url_with_pass("http://:tamura@example.com/");
  const GURL cross_origin_url_with_user("http://yukari@example2.com/");
  const GURL cross_origin_url_with_pass("http://:tamura@example2.com/");
  const auto ok = std::nullopt;
  const CorsErrorStatus kCorsDisabledScheme(
      mojom::CorsError::kCorsDisabledScheme);
  const CorsErrorStatus kRedirectContainsCredentials(
      mojom::CorsError::kRedirectContainsCredentials);

  TestCase cases[] = {
      // "cors", no credentials information
      {same_origin_url, kCors, false, false, ok},
      {cross_origin_url, kCors, false, false, ok},
      {data_url, kCors, false, false, ok},
      {same_origin_url, kCors, true, false, ok},
      {cross_origin_url, kCors, true, false, ok},
      {data_url, kCors, true, false, ok},
      {same_origin_url, kCors, false, true, ok},
      {cross_origin_url, kCors, false, true, ok},
      {data_url, kCors, false, true, ok},
      {same_origin_url, kCors, true, true, ok},
      {cross_origin_url, kCors, true, true, ok},
      {data_url, kCors, true, true, ok},

      // "cors" with forced preflight, no credentials information
      {same_origin_url, kCorsWithForcedPreflight, false, false, ok},
      {cross_origin_url, kCorsWithForcedPreflight, false, false, ok},
      {data_url, kCorsWithForcedPreflight, false, false, ok},
      {same_origin_url, kCorsWithForcedPreflight, true, false, ok},
      {cross_origin_url, kCorsWithForcedPreflight, true, false, ok},
      {data_url, kCorsWithForcedPreflight, true, false, ok},
      {same_origin_url, kCorsWithForcedPreflight, false, true, ok},
      {cross_origin_url, kCorsWithForcedPreflight, false, true, ok},
      {data_url, kCorsWithForcedPreflight, false, true, ok},
      {same_origin_url, kCorsWithForcedPreflight, true, true, ok},
      {cross_origin_url, kCorsWithForcedPreflight, true, true, ok},
      {data_url, kCorsWithForcedPreflight, true, true, ok},

      // "no-cors", no credentials information
      {same_origin_url, kNoCors, false, false, ok},
      {cross_origin_url, kNoCors, false, false, ok},
      {data_url, kNoCors, false, false, ok},
      {same_origin_url, kNoCors, false, true, ok},
      {cross_origin_url, kNoCors, false, true, ok},
      {data_url, kNoCors, false, true, ok},

      // with credentials information (same origin)
      {same_origin_url_with_user, kCors, false, false, ok},
      {same_origin_url_with_user, kCors, true, false,
       kRedirectContainsCredentials},
      {same_origin_url_with_user, kCors, true, true,
       kRedirectContainsCredentials},
      {same_origin_url_with_user, kNoCors, false, false, ok},
      {same_origin_url_with_user, kNoCors, false, true, ok},
      {same_origin_url_with_pass, kCors, false, false, ok},
      {same_origin_url_with_pass, kCors, true, false,
       kRedirectContainsCredentials},
      {same_origin_url_with_pass, kCors, true, true,
       kRedirectContainsCredentials},
      {same_origin_url_with_pass, kNoCors, false, false, ok},
      {same_origin_url_with_pass, kNoCors, false, true, ok},

      // with credentials information (cross origin)
      {cross_origin_url_with_user, kCors, false, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kCors, true, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kCors, true, true,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kNoCors, false, true, ok},
      {cross_origin_url_with_user, kNoCors, false, false, ok},
      {cross_origin_url_with_pass, kCors, false, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kCors, true, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kCors, true, true,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kNoCors, false, true, ok},
      {cross_origin_url_with_pass, kNoCors, false, false, ok},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "url: " << test.url
                 << ", request mode: " << test.request_mode
                 << ", origin: " << origin << ", cors_flag: " << test.cors_flag
                 << ", tainted: " << test.tainted);

    EXPECT_EQ(test.expectation, CorsURLLoader::CheckRedirectLocationForTesting(
                                    test.url, test.request_mode, origin,
                                    test.cors_flag, test.tainted));
  }
}

TEST_F(CorsURLLoaderTest, NetLogBasic) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  // Set customized header to make preflight required request instead of simple
  // request.
  request.headers.SetHeader("Apple", "red");
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  // Prepare a preflight response.
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"},
       {"Access-Control-Allow-Headers", "Apple"},
       {"Access-Control-Allow-Methods", "GET"}});

  // Continue the actual request.
  RunUntilCreateLoaderAndStartCalled();

  // Prepare an actual response.
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  std::vector<net::NetLogEntry> entries = GetEntries();
  EXPECT_EQ(5UL, entries.size());
  EXPECT_TRUE(net::LogContainsBeginEvent(entries, 0,
                                         net::NetLogEventType::CORS_REQUEST));
  EXPECT_TRUE(net::LogContainsEvent(
      entries, 1, net::NetLogEventType::CHECK_CORS_PREFLIGHT_REQUIRED,
      net::NetLogEventPhase::NONE));
  EXPECT_TRUE(net::GetBooleanValueFromParams(entries[1], "preflight_required"));
  EXPECT_EQ(
      net::GetStringValueFromParams(entries[1], "preflight_required_reason"),
      "disallowed_header");
  EXPECT_TRUE(net::LogContainsEvent(
      entries, 2, net::NetLogEventType::CHECK_CORS_PREFLIGHT_CACHE,
      net::NetLogEventPhase::NONE));
  EXPECT_EQ(net::GetStringValueFromParams(entries[2], "status"), "miss");
  EXPECT_TRUE(net::LogContainsEvent(entries, 3,
                                    net::NetLogEventType::CORS_PREFLIGHT_RESULT,
                                    net::NetLogEventPhase::NONE));
  EXPECT_EQ(
      net::GetStringValueFromParams(entries[3], "access-control-allow-methods"),
      "GET");
  EXPECT_EQ(
      net::GetStringValueFromParams(entries[3], "access-control-allow-headers"),
      "apple");
  EXPECT_TRUE(
      net::LogContainsEndEvent(entries, 4, net::NetLogEventType::CORS_REQUEST));
}

TEST_F(CorsURLLoaderTest, NetLogSameOriginRequest) {
  const GURL url("https://example.com/foo.png");
  CreateLoaderAndStart(url.DeprecatedGetOriginAsURL(), url,
                       mojom::RequestMode::kSameOrigin);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();
  std::vector<net::NetLogEntry> entries = GetEntries();
  EXPECT_EQ(3UL, entries.size());
  for (const auto& net_log_entry : entries) {
    if (net_log_entry.type !=
        net::NetLogEventType::CHECK_CORS_PREFLIGHT_REQUIRED) {
      continue;
    }
    EXPECT_FALSE(
        net::GetBooleanValueFromParams(net_log_entry, "preflight_required"));
    return;
  }
  ADD_FAILURE() << "Log entry not found.";
}

TEST_F(CorsURLLoaderTest, NetLogCrossOriginSimpleRequest) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  CreateLoaderAndStart(origin.DeprecatedGetOriginAsURL(), url,
                       mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();
  std::vector<net::NetLogEntry> entries = GetEntries();
  EXPECT_EQ(3UL, entries.size());
  for (const auto& net_log_entry : entries) {
    if (net_log_entry.type !=
        net::NetLogEventType::CHECK_CORS_PREFLIGHT_REQUIRED) {
      continue;
    }
    EXPECT_FALSE(
        net::GetBooleanValueFromParams(net_log_entry, "preflight_required"));
    return;
  }
  ADD_FAILURE() << "Log entry not found.";
}

TEST_F(CorsURLLoaderTest, NetLogPreflightMissingAllowOrigin) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  std::vector<net::NetLogEntry> entries = GetEntries();
  std::vector<net::NetLogEventType> types = GetTypesOfNetLogEntries(entries);
  EXPECT_THAT(types,
              Contains(net::NetLogEventType::CORS_PREFLIGHT_RESULT).Times(0));
  ASSERT_THAT(types,
              Contains(net::NetLogEventType::CORS_PREFLIGHT_ERROR).Times(1));

  const net::NetLogEntry* entry =
      FindEntryByType(entries, net::NetLogEventType::CORS_PREFLIGHT_ERROR);
  EXPECT_THAT(entry->params.FindString("error"), Pointee(Eq("ERR_FAILED")));
  EXPECT_THAT(entry->params.FindInt("cors-error"),
              Optional(Eq(static_cast<int>(
                  mojom::CorsError::kPreflightMissingAllowOriginHeader))));
  EXPECT_THAT(entry->params.FindString("failed-parameter"), IsNull());
}

TEST_F(CorsURLLoaderTest, NetLogPreflightMethodDisallowed) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://foo.example"},
      {"Access-Control-Allow-Methods", "GET"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  std::vector<net::NetLogEntry> entries = GetEntries();
  std::vector<net::NetLogEventType> types = GetTypesOfNetLogEntries(entries);
  ASSERT_THAT(types,
              Contains(net::NetLogEventType::CORS_PREFLIGHT_RESULT).Times(1));
  ASSERT_THAT(types,
              Contains(net::NetLogEventType::CORS_PREFLIGHT_ERROR).Times(1));

  const net::NetLogEntry* entry =
      FindEntryByType(entries, net::NetLogEventType::CORS_PREFLIGHT_RESULT);
  EXPECT_THAT(entry->params.FindString("access-control-allow-methods"),
              Pointee(Eq("GET")));

  entry = FindEntryByType(entries, net::NetLogEventType::CORS_PREFLIGHT_ERROR);
  EXPECT_THAT(entry->params.FindString("error"), Pointee(Eq("ERR_FAILED")));
  EXPECT_THAT(entry->params.FindInt("cors-error"),
              Optional(Eq(static_cast<int>(
                  mojom::CorsError::kMethodDisallowedByPreflightResponse))));
  EXPECT_THAT(entry->params.FindString("failed-parameter"), Pointee(Eq("PUT")));
}

TEST_F(CorsURLLoaderTest, NetLogPreflightNetError) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(net::ERR_INVALID_ARGUMENT);
  RunUntilComplete();

  std::vector<net::NetLogEntry> entries = GetEntries();
  const auto type = net::NetLogEventType::CORS_PREFLIGHT_ERROR;
  ASSERT_THAT(GetTypesOfNetLogEntries(entries), Contains(type).Times(1));

  const net::NetLogEntry* entry = FindEntryByType(entries, type);
  EXPECT_THAT(entry->params.FindString("error"),
              Pointee(Eq("ERR_INVALID_ARGUMENT")));
  EXPECT_THAT(entry->params.FindInt("cors-error"), Eq(std::nullopt));
  EXPECT_THAT(entry->params.FindString("failed-parameter"), IsNull());
}

TEST_F(CorsURLLoaderTest, PreflightMissingAllowOrigin) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code, net::ERR_FAILED);
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(CorsErrorStatus(
                  mojom::CorsError::kPreflightMissingAllowOriginHeader)));
}

TEST_F(CorsURLLoaderTest, NonBrowserNavigationRedirect) {
  BadMessageTestHelper bad_message_helper;

  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNavigate,
                       mojom::RedirectMode::kManual,
                       mojom::CredentialsMode::kInclude);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  FollowRedirect();

  RunUntilComplete();
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ElementsAre("CorsURLLoader: navigate from non-browser-process "
                          "should not call FollowRedirect"));
}

TEST_F(CorsURLLoaderTest, PrivateNetworkAccessTargetAddressSpaceCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPrivateNetworkAccessPermissionPrompt);

  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  factory_params.client_security_state = mojom::ClientSecurityState::New();
  factory_params.client_security_state->is_web_secure_context = true;
  ResetFactory(initiator, mojom::kBrowserProcessId, factory_params);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.required_ip_address_space = mojom::IPAddressSpace::kPrivate;
  request.target_ip_address_space = mojom::IPAddressSpace::kPrivate;
  request.url = GURL("http://foo.example/");
  request.request_initiator = initiator;
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->client_security_state =
      mojom::ClientSecurityState::New();
  request.trusted_params->client_security_state->is_web_secure_context = true;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(client().completion_status().error_code, net::OK);
}

class StorageAccessHeadersCorsURLLoaderTest : public CorsURLLoaderTest {
 public:
  StorageAccessHeadersCorsURLLoaderTest() : CorsURLLoaderTest() {
    feature_list_.InitAndEnableFeature(
        network::features::kStorageAccessHeaders);

    ResetFactoryParams factory_params;
    factory_params.is_trusted = true;
    ResetFactory(kInitiator, kRendererProcessId, factory_params);
  }

  std::optional<net::cookie_util::StorageAccessStatus>
  ComputeStorageAccessStatus(const ResourceRequest& request) {
    return network_context()
        ->cookie_manager()
        ->cookie_settings()
        .GetStorageAccessStatus(
            request.url, request.site_for_cookies,
            request.trusted_params->isolation_info.top_frame_origin(),
            URLLoader::CalculateCookieSettingOverrides(
                net::CookieSettingOverrides(), request));
  }

  ResourceRequest CreateNoCorsResourceRequest(
      const GURL& url,
      const url::Origin& top_frame_origin,
      base::optional_ref<const url::Origin> initiator =
          base::optional_ref<const url::Origin>(std::nullopt)) const {
    const url::Origin url_origin = url::Origin::Create(url);

    net::SiteForCookies site_for_cookies = net::SiteForCookies::FromUrl(url);
    site_for_cookies.CompareWithFrameTreeOriginAndRevise(top_frame_origin);

    ResourceRequest request;
    request.mode = mojom::RequestMode::kNoCors;
    request.credentials_mode = mojom::CredentialsMode::kInclude;
    request.method = "GET";
    request.site_for_cookies = site_for_cookies;
    request.url = url;
    request.request_initiator =
        initiator.has_value() ? initiator.value() : kInitiator;
    request.trusted_params = ResourceRequest::TrustedParams();

    // From a privacy and security standpoint, there are three parties
    // represented here: the top-level origin, the request destination's origin,
    // and the initiator origin. The initiator origin may not be the same as the
    // request destination's origin (especially if the request destination is
    // under attack via CSRF or similar), so we ensure that the SUT does not
    // conflate the three parties by supplying different origins for each one.
    request.trusted_params->isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, top_frame_origin, url_origin,
        request.site_for_cookies);
    return request;
  }

  void CreateLoaderAndRunToSuccessfulCompletion(
      const ResourceRequest& request) {
    CreateLoaderAndStart(request);
    RunUntilCreateLoaderAndStartCalled();

    NotifyLoaderClientOnReceiveResponse();
    NotifyLoaderClientOnComplete(net::OK);

    RunUntilComplete();

    ASSERT_FALSE(client().has_received_redirect());
    ASSERT_TRUE(client().has_received_response());
    ASSERT_TRUE(client().has_received_completion());
    ASSERT_EQ(net::OK, client().completion_status().error_code);
  }

 protected:
  const url::Origin kInitiator =
      url::Origin::Create(GURL("https://origin.com"));
  const GURL kUrl = GURL("https://example.com/foo.png");
  const url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://top.com"));

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(StorageAccessHeadersCorsURLLoaderTest, OmitsOriginWhenStatusIsOmitted) {
  ResourceRequest request = CreateNoCorsResourceRequest(
      kUrl, /*top_frame_origin=*/url::Origin::Create(kUrl));

  // The status is nullopt because this isn't a cross-site context.
  network_context()->cookie_manager()->BlockThirdPartyCookies(true);
  ASSERT_EQ(ComputeStorageAccessStatus(request), std::nullopt);

  CreateLoaderAndRunToSuccessfulCompletion(request);

  EXPECT_FALSE(
      GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
}

TEST_F(StorageAccessHeadersCorsURLLoaderTest, OmitsOriginWhenStatusIsNone) {
  ResourceRequest request = CreateNoCorsResourceRequest(kUrl, kTopFrameOrigin);

  network_context()->cookie_manager()->BlockThirdPartyCookies(true);
  // The status is "none" because cross-site cookies are blocked.
  ASSERT_EQ(ComputeStorageAccessStatus(request),
            net::cookie_util::StorageAccessStatus::kNone);

  CreateLoaderAndRunToSuccessfulCompletion(request);

  EXPECT_FALSE(
      GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
}

TEST_F(StorageAccessHeadersCorsURLLoaderTest,
       IncludesOriginWhenStatusIsInactive) {
  network_context()->cookie_manager()->BlockThirdPartyCookies(true);
  base::test::TestFuture<void> future;
  network_context()->cookie_manager()->SetContentSettings(
      ContentSettingsType::STORAGE_ACCESS,
      {
          ContentSettingPatternSource(
              ContentSettingsPattern::FromURLToSchemefulSitePattern(kUrl),
              ContentSettingsPattern::FromURLToSchemefulSitePattern(
                  kTopFrameOrigin.GetURL()),
              base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
              content_settings::ProviderType::kDefaultProvider,
              /*incognito=*/false),
      },
      future.GetCallback());
  ASSERT_TRUE(future.Wait());

  ResourceRequest request = CreateNoCorsResourceRequest(kUrl, kTopFrameOrigin);

  // The status is inactive because this is a cross-site context and cross-site
  // cookies are blocked, but there's a matching STORAGE_ACCESS grant that could
  // allow access.
  ASSERT_EQ(ComputeStorageAccessStatus(request),
            net::cookie_util::StorageAccessStatus::kInactive);

  CreateLoaderAndRunToSuccessfulCompletion(request);

  EXPECT_EQ(GetRequest().headers.GetHeader(net::HttpRequestHeaders::kOrigin),
            "https://origin.com");
}

// Regression test for https://crbug.com/371011222. This sends a request that
// would have "Sec-Fetch-Storage-Access: inactive", and therefore include an
// "Origin" header (to allow for safe upgrades to "active"), if the
// ResourceRequest's StorageAccessApiStatus weren't taken into account. However,
// its status of kAccessViaAPI means that the "Sec-Fetch-Storage-Access" value
// should be "active", and thus no Origin header should be sent. These tests
// mock out lower layers (including URLLoader and URLRequest) which add the
// "Sec-Fetch-Storage-Access" header, so this test only checks that there's no
// "Origin" header.
TEST_F(StorageAccessHeadersCorsURLLoaderTest,
       ResourceRequestParamsActivateAccess) {
  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  url::Origin initiator = url::Origin::Create(GURL("https://sub.example.com"));
  ResetFactory(initiator, kRendererProcessId, factory_params);
  network_context()->cookie_manager()->BlockThirdPartyCookies(true);
  base::test::TestFuture<void> future;
  network_context()->cookie_manager()->SetContentSettings(
      ContentSettingsType::STORAGE_ACCESS,
      {
          ContentSettingPatternSource(
              ContentSettingsPattern::FromURLToSchemefulSitePattern(kUrl),
              ContentSettingsPattern::FromURLToSchemefulSitePattern(
                  kTopFrameOrigin.GetURL()),
              base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
              content_settings::ProviderType::kDefaultProvider,
              /*incognito=*/false),
      },
      future.GetCallback());
  ASSERT_TRUE(future.Wait());

  ResourceRequest request =
      CreateNoCorsResourceRequest(kUrl, kTopFrameOrigin, initiator);
  request.storage_access_api_status =
      net::StorageAccessApiStatus::kAccessViaAPI;

  // The status is active because this is a cross-site context, cross-site
  // cookies are blocked, there's a matching STORAGE_ACCESS grant that could
  // allow access, the caller is opting to use that permission via
  // `storage_access_api_status`, *and* the request initiator is same-site with
  // the target URL.
  ASSERT_EQ(ComputeStorageAccessStatus(request),
            net::cookie_util::StorageAccessStatus::kActive);

  CreateLoaderAndRunToSuccessfulCompletion(request);

  EXPECT_FALSE(
      GetRequest().headers.GetHeader(net::HttpRequestHeaders::kOrigin));
}

TEST_F(StorageAccessHeadersCorsURLLoaderTest, OmitsOriginWhenStatusIsActive) {
  ResourceRequest request = CreateNoCorsResourceRequest(kUrl, kTopFrameOrigin);

  // The status is active because this is a cross-site context, but cross-site
  // cookies aren't blocked.
  ASSERT_EQ(ComputeStorageAccessStatus(request),
            net::cookie_util::StorageAccessStatus::kActive);

  CreateLoaderAndRunToSuccessfulCompletion(request);

  EXPECT_FALSE(
      GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
}

}  // namespace
}  // namespace network::cors
