// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_challenge_tab_helper.h"

#import <memory>
#import <optional>
#import <set>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "components/enterprise/device_trust/core/metrics_utils.h"
#import "components/enterprise/device_trust/core/mock_device_trust_service.h"
#import "components/enterprise/device_trust/prefs.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_connector_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_java_script_feature.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

const char kExampleUrl[] = "https://example.com";
const char kExampleLoginUrl[] = "https://example.com/login";
const char kExampleOtherUrl[] = "https://example.com/other";
const char kAttackerUrl[] = "https://attacker.com";
const char kAttackerLoginUrl[] = "https://attacker.com/login";
const char kWildcardPattern[] = "*";
const char kFunnelHistogram[] = "Enterprise.DeviceTrust.Attestation.Funnel";
const char kPolicyLevelHistogram[] =
    "Enterprise.DeviceTrust.Attestation.PolicyLevel";
const char kHandshakeResultHistogram[] =
    "Enterprise.DeviceTrust.Handshake.Result";
const char kSuccessLatencyHistogram[] =
    "Enterprise.DeviceTrust.Attestation.ResponseLatency.Success";
const char kFailureLatencyHistogram[] =
    "Enterprise.DeviceTrust.Attestation.ResponseLatency.Failure";

class DeviceTrustChallengeTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        DeviceTrustServiceFactoryIOS::GetInstance(),
        base::BindOnce([](ProfileIOS*) -> std::unique_ptr<KeyedService> {
          return std::make_unique<testing::NiceMock<
              enterprise_connectors::test::MockDeviceTrustService>>();
        }));
    profile_ = std::move(builder).Build();
    web::test::OverrideJavaScriptFeatures(
        profile_.get(), {DeviceTrustJavaScriptFeature::GetInstance()});

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());

    // FakeWebState does not provide a FakeWebFramesManager by default.
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = frames_manager.get();
    web_state_->SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                    std::move(frames_manager));

    DeviceTrustChallengeTabHelper::CreateForWebState(web_state_.get(),
                                                     mock_service());
  }

  DeviceTrustChallengeTabHelper* helper() {
    return DeviceTrustChallengeTabHelper::FromWebState(web_state_.get());
  }

  enterprise_connectors::test::MockDeviceTrustService* mock_service() {
    return static_cast<enterprise_connectors::test::MockDeviceTrustService*>(
        DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get()));
  }

  enterprise_connectors::DeviceTrustConnectorService* connector_service() {
    return DeviceTrustConnectorServiceFactoryIOS::GetForProfile(profile_.get());
  }

  void SetAllowlistPatterns(const std::vector<std::string>& patterns) {
    base::ListValue urls;
    for (const auto& pattern : patterns) {
      urls.Append(pattern);
    }
    profile_->GetTestingPrefService()->SetManagedPref(
        enterprise_connectors::kUserContextAwareAccessSignalsAllowlistPref,
        std::move(urls));

    ON_CALL(*mock_service(), IsEnabled()).WillByDefault([this]() {
      auto* connector = connector_service();
      return connector && connector->IsConnectorEnabled();
    });
    ON_CALL(*mock_service(), Watches(testing::_))
        .WillByDefault([this](const GURL& url) {
          auto* connector = connector_service();
          return connector ? connector->Watches(url)
                           : std::set<enterprise_connectors::DTCPolicyLevel>();
        });
  }

  web::FakeWebFrame* SetupMainFrame(const url::Origin& origin,
                                    const GURL& url = GURL()) {
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(origin);
    main_frame->set_browser_state(profile_.get());
    if (url.is_valid()) {
      main_frame->set_url(url);
    }
    web::FakeWebFrame* main_frame_ptr = main_frame.get();
    web_frames_manager_->AddWebFrame(std::move(main_frame));
    return main_frame_ptr;
  }

  web::FakeWebFrame* SetupMainFrame(const GURL& url) {
    return SetupMainFrame(url::Origin::Create(url), url);
  }

  using AttestationCallback =
      DeviceTrustChallengeTabHelper::AttestationCallback;
  using AttestationResult = enterprise_connectors::DeviceTrustResponse;

  AttestationCallback CaptureResponseCallback(
      AttestationResult* out_response,
      int* out_count = nullptr,
      base::OnceClosure quit_closure = {}) {
    return base::BindOnce(
        [](AttestationResult* out, int* count, base::OnceClosure quit,
           const AttestationResult& result) {
          if (count) {
            (*count)++;
          }
          if (out) {
            *out = result;
          }
          if (quit) {
            std::move(quit).Run();
          }
        },
        out_response, out_count, std::move(quit_closure));
  }

  // Makes the service enabled and watching every URL at the user level.
  void WatchAllUrls() {
    ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
    ON_CALL(*mock_service(), Watches(testing::_))
        .WillByDefault(
            testing::Return(std::set<enterprise_connectors::DTCPolicyLevel>{
                enterprise_connectors::DTCPolicyLevel::kUser}));
  }

  // Makes the service reply synchronously with `response`.
  void ReplyWith(const enterprise_connectors::DeviceTrustResponse& response) {
    ON_CALL(*mock_service(),
            BuildChallengeResponse(testing::_, testing::_, testing::_))
        .WillByDefault(
            [response](
                const std::string&,
                const std::set<enterprise_connectors::DTCPolicyLevel>&,
                enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                    callback) { std::move(callback).Run(response); });
  }

  // Sends a request from `origin` and waits for its reply.
  AttestationResult SendRequest(const url::Origin& origin) {
    base::RunLoop run_loop;
    AttestationResult response;
    helper()->BuildChallengeResponse(
        origin, origin.GetURL(), "challenge",
        CaptureResponseCallback(&response, /*out_count=*/nullptr,
                                run_loop.QuitClosure()));
    run_loop.Run();
    return response;
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_ = nullptr;
};

// Verifies that the tab helper is created and attached to the WebState.
TEST_F(DeviceTrustChallengeTabHelperTest, CreatesSuccessfully) {
  EXPECT_NE(helper(), nullptr);
}

// Verifies that when a main web frame becomes available (with only its
// security origin set and an empty frame URL), the policy check falls back to
// the security origin URL and executes the Device Trust API setup script if
// allowlisted.
TEST_F(DeviceTrustChallengeTabHelperTest, SetUpAPIForMainFrame) {
  SetAllowlistPatterns({kExampleUrl});
  web::FakeWebFrame* main_frame_ptr =
      SetupMainFrame(url::Origin::Create(GURL(kExampleUrl)));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
}

// Verifies that non-main (child) frames do not trigger the API setup even if
// allowlisted.
TEST_F(DeviceTrustChallengeTabHelperTest, IgnoreChildFrame) {
  SetAllowlistPatterns({kExampleUrl});
  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(GURL(kExampleUrl));
  child_frame->set_browser_state(profile_.get());
  web::FakeWebFrame* child_frame_ptr = child_frame.get();
  web_frames_manager_->AddWebFrame(std::move(child_frame));
  EXPECT_EQ(child_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Verifies that a non-opaque main frame triggers the API setup when a wildcard
// allowlist pattern ("*") is configured.
TEST_F(DeviceTrustChallengeTabHelperTest, SetUpAPIForWildcardAllowlist) {
  SetAllowlistPatterns({kWildcardPattern});
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(GURL(kExampleUrl));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
}

// Verifies that main frames with an opaque origin do not trigger the API setup
// even with a wildcard allowlist, without querying the policy matcher.
TEST_F(DeviceTrustChallengeTabHelperTest, IgnoreOpaqueOrigin) {
  SetAllowlistPatterns({kWildcardPattern});
  EXPECT_CALL(*mock_service(), Watches(testing::_)).Times(0);
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(url::Origin());
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Verifies that when no DeviceTrustService is available, main frames do not
// trigger the API setup.
TEST_F(DeviceTrustChallengeTabHelperTest,
       IgnoreMainFrameWhenServiceUnavailable) {
  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state_.get());
  DeviceTrustChallengeTabHelper::CreateForWebState(
      web_state_.get(), /*device_trust_service=*/nullptr);
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(GURL(kExampleUrl));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Verifies that when DeviceTrustService is disabled, main frames do not
// trigger the API setup even if the origin is allowlisted, without querying
// the policy matcher.
TEST_F(DeviceTrustChallengeTabHelperTest, IgnoreDisabledService) {
  SetAllowlistPatterns({kExampleUrl});
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(false));
  EXPECT_CALL(*mock_service(), Watches(testing::_)).Times(0);
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(GURL(kExampleUrl));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Verifies that when an allowlist pattern is path-scoped, the API is set up on
// a main frame navigating to the matching path.
TEST_F(DeviceTrustChallengeTabHelperTest, SetupAPIForMatchingPath) {
  SetAllowlistPatterns({kExampleLoginUrl});
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(GURL(kExampleLoginUrl));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
}

// Verifies that when an allowlist pattern is path-scoped, the API is not set up
// on a main frame navigating to a non-matching path within the same origin.
TEST_F(DeviceTrustChallengeTabHelperTest, IgnoreNonMatchingPath) {
  SetAllowlistPatterns({kExampleLoginUrl});
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(GURL(kExampleOtherUrl));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Verifies that when an allowlist pattern is path-scoped and the main frame's
// GetUrl() is empty (fallback to security origin URL), the API is not set up
// because the base origin URL does not match the path-scoped pattern.
TEST_F(DeviceTrustChallengeTabHelperTest, IgnorePathScopedWhenUrlNotSet) {
  SetAllowlistPatterns({kExampleLoginUrl});
  // Pass only the security origin so GetUrl() remains empty and policy check
  // falls back to the origin URL ("https://example.com/"), which does not
  // match "/login".
  web::FakeWebFrame* main_frame_ptr =
      SetupMainFrame(url::Origin::Create(GURL(kExampleUrl)));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Verifies that when the main frame's GetUrl() does not match its security
// origin, the candidate URL is ignored and the policy check evaluates the
// security origin. If the security origin is not allowlisted, the API is not
// set up even if GetUrl() would have matched the allowlist.
TEST_F(DeviceTrustChallengeTabHelperTest,
       IgnoreAllowedUrlWhenOriginNotAllowlisted) {
  SetAllowlistPatterns({kExampleUrl});
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(
      url::Origin::Create(GURL(kAttackerUrl)), GURL(kExampleUrl));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Verifies that removing the current main frame does not prevent the API from
// being set up in a replacement main frame.
TEST_F(DeviceTrustChallengeTabHelperTest, SetupAPIAfterMainFrameRemoved) {
  SetAllowlistPatterns({kExampleUrl});
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(GURL(kExampleUrl));
  web_frames_manager_->RemoveWebFrame(main_frame_ptr->GetFrameId());
  web::FakeWebFrame* replacement_frame_ptr = SetupMainFrame(GURL(kExampleUrl));
  EXPECT_EQ(replacement_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
}

// Verifies that when the tab helper is created after an allowlisted main frame
// is already available (e.g. deferred tab helper creation), the constructor
// sets up the API on that existing frame.
TEST_F(DeviceTrustChallengeTabHelperTest, SetupAPIForExistingMainFrame) {
  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state_.get());
  SetAllowlistPatterns({kExampleUrl});
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(GURL(kExampleUrl));
  ASSERT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
  DeviceTrustChallengeTabHelper::CreateForWebState(web_state_.get(),
                                                   mock_service());
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
}

// Verifies that when the tab helper is created after a non-allowlisted main
// frame is already available, the constructor does not set up the API.
TEST_F(DeviceTrustChallengeTabHelperTest,
       IgnoreExistingNonAllowlistedMainFrame) {
  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state_.get());
  SetAllowlistPatterns({kExampleUrl});
  web::FakeWebFrame* main_frame_ptr = SetupMainFrame(GURL(kAttackerUrl));
  DeviceTrustChallengeTabHelper::CreateForWebState(web_state_.get(),
                                                   mock_service());
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Destroying the WebState invokes WebStateDestroyed() on the helper, which
// must detach its WebFramesManager observation without crashing.
TEST_F(DeviceTrustChallengeTabHelperTest, DestroyWebStateDoesNotCrash) {
  web_frames_manager_ = nullptr;
  web_state_.reset();
}

// Verifies that BuildChallengeResponse fails when no DeviceTrustService is
// available.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseFailsWhenNoService) {
  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state_.get());
  DeviceTrustChallengeTabHelper::CreateForWebState(
      web_state_.get(), /*device_trust_service=*/nullptr);
  base::RunLoop run_loop;
  AttestationResult response;
  int count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&response, &count, run_loop.QuitClosure()));
  EXPECT_EQ(count, 0);
  run_loop.Run();
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(response.challenge_response.empty());
  EXPECT_EQ(response.error,
            enterprise_connectors::DeviceTrustError::kServiceUnavailable);
}

// Verifies that BuildChallengeResponse fails when the DeviceTrustService is
// disabled.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseFailsWhenServiceDisabled) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(false));
  base::RunLoop run_loop;
  AttestationResult response;
  int count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&response, &count, run_loop.QuitClosure()));
  EXPECT_EQ(count, 0);
  run_loop.Run();
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(response.challenge_response.empty());
  EXPECT_EQ(response.error,
            enterprise_connectors::DeviceTrustError::kServiceUnavailable);
}

// Verifies that BuildChallengeResponse fails when the origin is not watched
// by the DeviceTrustService.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseFailsWhenOriginNotWatched) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  EXPECT_CALL(*mock_service(), Watches(GURL(kExampleUrl)))
      .WillOnce(
          testing::Return(std::set<enterprise_connectors::DTCPolicyLevel>()));
  base::RunLoop run_loop;
  AttestationResult response;
  int count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&response, &count, run_loop.QuitClosure()));
  EXPECT_EQ(count, 0);
  run_loop.Run();
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(response.challenge_response.empty());
  EXPECT_EQ(response.error,
            enterprise_connectors::DeviceTrustError::kUrlNotAllowed);
}

// Verifies that a successful attestation response resolves the callback with
// the signed payload.
TEST_F(DeviceTrustChallengeTabHelperTest, BuildChallengeResponseSuccess) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  EXPECT_CALL(*mock_service(), Watches(GURL(kExampleUrl)))
      .WillOnce(testing::Return(levels));
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse("my_challenge", levels, testing::_))
      .WillOnce(
          [](const std::string&,
             const std::set<enterprise_connectors::DTCPolicyLevel>&,
             enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                 callback) {
            enterprise_connectors::DeviceTrustResponse response;
            response.challenge_response = "signed_payload_123";
            std::move(callback).Run(response);
          });
  AttestationResult response;
  helper()->BuildChallengeResponse(url::Origin::Create(GURL(kExampleUrl)),
                                   GURL(kExampleUrl), "my_challenge",
                                   CaptureResponseCallback(&response));
  EXPECT_FALSE(response.error.has_value());
  EXPECT_EQ(response.challenge_response, "signed_payload_123");
}

// Verifies that a request with a matching path is allowed by the policy
// matcher.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseAllowsMatchingPath) {
  SetAllowlistPatterns({kExampleLoginUrl});
  const GURL request_url(kExampleLoginUrl);
  const url::Origin origin = url::Origin::Create(request_url);
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  // Verify that mock_service() delegates Watches() to the real policy matcher
  // and receives the expected argument.
  EXPECT_CALL(*mock_service(), Watches(request_url))
      .WillOnce([this](const GURL& url) {
        return connector_service()->Watches(url);
      });
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse("my_challenge", levels, testing::_))
      .WillOnce(
          [](const std::string&,
             const std::set<enterprise_connectors::DTCPolicyLevel>&,
             enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                 callback) {
            enterprise_connectors::DeviceTrustResponse response;
            response.challenge_response = "signed_payload_123";
            std::move(callback).Run(response);
          });
  AttestationResult response;
  helper()->BuildChallengeResponse(origin, request_url, "my_challenge",
                                   CaptureResponseCallback(&response));
  EXPECT_FALSE(response.error.has_value());
  EXPECT_EQ(response.challenge_response, "signed_payload_123");
}

// Verifies that a request with a non-matching path is declined by the policy
// matcher.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseDeclinesNonMatchingPath) {
  SetAllowlistPatterns({kExampleLoginUrl});
  const GURL request_url(kExampleOtherUrl);
  const url::Origin origin = url::Origin::Create(request_url);
  // Verify that mock_service() delegates Watches() to the real policy matcher
  // and receives the expected argument.
  EXPECT_CALL(*mock_service(), Watches(request_url))
      .WillOnce([this](const GURL& url) {
        return connector_service()->Watches(url);
      });
  // BuildChallengeResponse must NOT be called because the real policy matcher
  // excludes https://example.com/other.
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, testing::_, testing::_))
      .Times(0);
  base::RunLoop run_loop;
  AttestationResult response;
  int count = 0;
  helper()->BuildChallengeResponse(
      origin, request_url, "my_challenge",
      CaptureResponseCallback(&response, &count, run_loop.QuitClosure()));
  EXPECT_EQ(count, 0);
  run_loop.Run();
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(response.challenge_response.empty());
  EXPECT_EQ(response.error,
            enterprise_connectors::DeviceTrustError::kUrlNotAllowed);
}

// Verifies that the origin URL is used for policy evaluation when the request
// URL origin differs from the security origin.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseFallsBackWhenOriginDiffersFromUrl) {
  SetAllowlistPatterns({kExampleUrl});
  const GURL request_url(kAttackerLoginUrl);
  const url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  // Because the URL origin differs from the security origin, fallback to
  // origin.GetURL(). The real policy matcher matches origin.GetURL().
  EXPECT_CALL(*mock_service(), Watches(origin.GetURL()))
      .WillOnce([this](const GURL& url) {
        return connector_service()->Watches(url);
      });
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse("my_challenge", levels, testing::_))
      .WillOnce(
          [](const std::string&,
             const std::set<enterprise_connectors::DTCPolicyLevel>&,
             enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                 callback) {
            enterprise_connectors::DeviceTrustResponse response;
            response.challenge_response = "signed_payload_123";
            std::move(callback).Run(response);
          });
  AttestationResult response;
  helper()->BuildChallengeResponse(origin, request_url, "my_challenge",
                                   CaptureResponseCallback(&response));
  EXPECT_FALSE(response.error.has_value());
  EXPECT_EQ(response.challenge_response, "signed_payload_123");
}

// Verifies that the origin URL is used for policy evaluation when the request
// URL is invalid.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseFallsBackWhenUrlIsInvalid) {
  SetAllowlistPatterns({kExampleUrl});
  const GURL invalid_url;
  const url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  // Because the URL is invalid, fallback to origin.GetURL(). The real policy
  // matcher matches origin.GetURL().
  EXPECT_CALL(*mock_service(), Watches(origin.GetURL()))
      .WillOnce([this](const GURL& url) {
        return connector_service()->Watches(url);
      });
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse("my_challenge", levels, testing::_))
      .WillOnce(
          [](const std::string&,
             const std::set<enterprise_connectors::DTCPolicyLevel>&,
             enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                 callback) {
            enterprise_connectors::DeviceTrustResponse response;
            response.challenge_response = "signed_payload_123";
            std::move(callback).Run(response);
          });
  AttestationResult response;
  helper()->BuildChallengeResponse(origin, invalid_url, "my_challenge",
                                   CaptureResponseCallback(&response));
  EXPECT_FALSE(response.error.has_value());
  EXPECT_EQ(response.challenge_response, "signed_payload_123");
}

// Verifies that the origin URL is used for policy evaluation when the request
// URL is nullopt.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseFallsBackWhenUrlIsNullopt) {
  SetAllowlistPatterns({kExampleUrl});
  const url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  // Because request_url is std::nullopt, fallback to origin.GetURL(). The real
  // policy matcher matches origin.GetURL().
  EXPECT_CALL(*mock_service(), Watches(origin.GetURL()))
      .WillOnce([this](const GURL& url) {
        return connector_service()->Watches(url);
      });
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse("my_challenge", levels, testing::_))
      .WillOnce(
          [](const std::string&,
             const std::set<enterprise_connectors::DTCPolicyLevel>&,
             enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                 callback) {
            enterprise_connectors::DeviceTrustResponse response;
            response.challenge_response = "signed_payload_123";
            std::move(callback).Run(response);
          });
  AttestationResult response;
  helper()->BuildChallengeResponse(origin, std::nullopt, "my_challenge",
                                   CaptureResponseCallback(&response));
  EXPECT_FALSE(response.error.has_value());
  EXPECT_EQ(response.challenge_response, "signed_payload_123");
}

// Verifies that an opaque origin is rejected even with a wildcard allowlist,
// without querying the policy matcher or building a challenge response.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseRejectsOpaqueOriginWithWildcardAllowlist) {
  SetAllowlistPatterns({kWildcardPattern});
  const url::Origin opaque_origin;
  ASSERT_TRUE(opaque_origin.opaque());
  // Neither Watches() nor BuildChallengeResponse() should be called for an
  // opaque origin.
  EXPECT_CALL(*mock_service(), Watches(testing::_)).Times(0);
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, testing::_, testing::_))
      .Times(0);
  base::RunLoop run_loop;
  AttestationResult response;
  int count = 0;
  helper()->BuildChallengeResponse(
      opaque_origin, std::nullopt, "my_challenge",
      CaptureResponseCallback(&response, &count, run_loop.QuitClosure()));
  EXPECT_EQ(count, 0);
  run_loop.Run();
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(response.challenge_response.empty());
  EXPECT_EQ(response.error,
            enterprise_connectors::DeviceTrustError::kInvalidOrigin);
}

// Verifies that service error codes are forwarded directly in the response.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseForwardsServiceError) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));
  const enterprise_connectors::DeviceTrustError test_cases[] = {
      enterprise_connectors::DeviceTrustError::kTimeout,
      enterprise_connectors::DeviceTrustError::kFailedToParseChallenge,
      enterprise_connectors::DeviceTrustError::kFailedToCreateResponse,
      enterprise_connectors::DeviceTrustError::kUnknown,
  };
  for (const auto& service_error : test_cases) {
    EXPECT_CALL(*mock_service(),
                BuildChallengeResponse("challenge", levels, testing::_))
        .WillOnce(
            [&](const std::string&,
                const std::set<enterprise_connectors::DTCPolicyLevel>&,
                enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                    callback) {
              enterprise_connectors::DeviceTrustResponse response;
              response.error = service_error;
              std::move(callback).Run(response);
            });
    AttestationResult response;
    int count = 0;
    helper()->BuildChallengeResponse(
        url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
        CaptureResponseCallback(&response, &count));
    EXPECT_EQ(count, 1);
    EXPECT_TRUE(response.challenge_response.empty());
    EXPECT_EQ(response.error, service_error);
  }
}

// Verifies that when a request times out, it returns kTimeout, and any
// subsequent response from the service is ignored.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseBrowserSideTimeout) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));
  enterprise_connectors::DeviceTrustService::DeviceTrustCallback saved_callback;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .WillOnce(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { saved_callback = std::move(callback); });
  AttestationResult response;
  int response_count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&response, &response_count));
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(response_count, 0);
  // Fast forward by 25 seconds to trigger the browser-side timeout.
  task_environment_.FastForwardBy(base::Seconds(25));
  EXPECT_EQ(response_count, 1);
  EXPECT_TRUE(response.challenge_response.empty());
  EXPECT_EQ(response.error, enterprise_connectors::DeviceTrustError::kTimeout);
  // Late response from the service is safely ignored.
  enterprise_connectors::DeviceTrustResponse late_response;
  late_response.challenge_response = "late_payload";
  std::move(saved_callback).Run(late_response);
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(response_count, 1);
}

// Verifies that the tab helper enforces kMaxPendingRequests and rejects
// requests that exceed the limit with kTooManyRequests without reaching the
// service.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseEnforcesPendingRequestsLimit) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));
  std::vector<enterprise_connectors::DeviceTrustService::DeviceTrustCallback>
      callbacks;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .Times(DeviceTrustChallengeTabHelper::kMaxPendingRequests)
      .WillRepeatedly(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { callbacks.push_back(std::move(callback)); });
  std::vector<AttestationResult> pending_responses(
      DeviceTrustChallengeTabHelper::kMaxPendingRequests);
  for (size_t i = 0; i < DeviceTrustChallengeTabHelper::kMaxPendingRequests;
       ++i) {
    helper()->BuildChallengeResponse(
        url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
        CaptureResponseCallback(&pending_responses[i]));
  }
  EXPECT_EQ(callbacks.size(),
            DeviceTrustChallengeTabHelper::kMaxPendingRequests);
  // The next request exceeds the limit and must be rejected asynchronously.
  base::RunLoop run_loop;
  AttestationResult rejected_response;
  int rejected_count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&rejected_response, &rejected_count,
                              run_loop.QuitClosure()));
  EXPECT_EQ(rejected_count, 0);
  run_loop.Run();
  EXPECT_EQ(rejected_count, 1);
  EXPECT_TRUE(rejected_response.challenge_response.empty());
  EXPECT_EQ(rejected_response.error,
            enterprise_connectors::DeviceTrustError::kTooManyRequests);
  // Clean up pending callbacks.
  enterprise_connectors::DeviceTrustResponse service_response;
  service_response.challenge_response = "success";
  for (auto& callback : callbacks) {
    std::move(callback).Run(service_response);
  }
}

// Verifies that destroying the WebState drops pending requests without
// invoking their callbacks (since the WKWebView is deallocating), and that any
// subsequent response from the service after teardown produces no reply.
TEST_F(
    DeviceTrustChallengeTabHelperTest,
    BuildChallengeResponseWebStateDestroyedDropsRequestsAndIgnoresLateResponse) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));
  enterprise_connectors::DeviceTrustService::DeviceTrustCallback saved_callback;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .WillOnce(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { saved_callback = std::move(callback); });
  AttestationResult response;
  int response_count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&response, &response_count));
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(response_count, 0);
  // Destroying the WebState drops pending requests without invoking callbacks.
  web_frames_manager_ = nullptr;
  web_state_.reset();
  EXPECT_EQ(response_count, 0);
  // Calling the saved service callback after teardown produces no reply.
  enterprise_connectors::DeviceTrustResponse late_response;
  late_response.challenge_response = "late_payload";
  std::move(saved_callback).Run(late_response);
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(response_count, 0);
}

// Verifies that removing the tab helper directly (without WebStateDestroyed)
// acts as a backstop, dropping pending requests without invoking callbacks
// and ignoring late service responses.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseDestructorDropsRequestsAndIgnoresLateResponse) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));
  enterprise_connectors::DeviceTrustService::DeviceTrustCallback saved_callback;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .WillOnce(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { saved_callback = std::move(callback); });
  AttestationResult response;
  int response_count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&response, &response_count));
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(response_count, 0);
  // Removing the tab helper directly triggers the destructor backstop,
  // dropping pending requests without invoking callbacks.
  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state_.get());
  EXPECT_EQ(response_count, 0);
  // Calling the saved service callback after destruction produces no reply.
  enterprise_connectors::DeviceTrustResponse late_response;
  late_response.challenge_response = "late_payload";
  std::move(saved_callback).Run(late_response);
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(response_count, 0);
}

// Verifies that if the WebState is destroyed before an early rejection callback
// runs, the weak pointer cancels the task and the callback is not invoked.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseEarlyRejectionDroppedIfWebStateDestroyed) {
  AttestationResult response;
  int count = 0;
  helper()->BuildChallengeResponse(url::Origin::Create(GURL(kExampleUrl)),
                                   GURL(kExampleUrl), "challenge",
                                   CaptureResponseCallback(&response, &count));
  EXPECT_EQ(count, 0);
  // Destroy the WebState before the posted error callback runs.
  web_frames_manager_ = nullptr;
  web_state_.reset();
  // Flush any tasks posted to the sequence to verify the callback was canceled.
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(count, 0);
}

// Verifies that if the tab helper is removed before an early rejection callback
// runs, the weak pointer cancels the task and the callback is not invoked.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseEarlyRejectionDroppedIfHelperDestroyed) {
  AttestationResult response;
  int count = 0;
  helper()->BuildChallengeResponse(url::Origin::Create(GURL(kExampleUrl)),
                                   GURL(kExampleUrl), "challenge",
                                   CaptureResponseCallback(&response, &count));
  EXPECT_EQ(count, 0);
  // Remove the tab helper directly before the posted error callback runs.
  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state_.get());
  // Flush any tasks posted to the sequence to verify the callback was canceled.
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(count, 0);
}

// Verifies that setting up the API on an allowlisted main frame logs the
// start of the attestation flow.
TEST_F(DeviceTrustChallengeTabHelperTest, SetupAPILogsAttestationFlowStarted) {
  SetAllowlistPatterns({kExampleUrl});
  SetupMainFrame(GURL(kExampleUrl));
  histogram_tester_.ExpectUniqueSample(
      kFunnelHistogram,
      enterprise_connectors::DTAttestationFunnelStep::kAttestationFlowStarted,
      1);
}

// Verifies that a main frame that does not get the API does not log the start
// of the attestation flow.
TEST_F(DeviceTrustChallengeTabHelperTest,
       IgnoredMainFrameDoesNotLogAttestationFlowStarted) {
  SetAllowlistPatterns({kExampleUrl});
  SetupMainFrame(GURL(kAttackerUrl));
  histogram_tester_.ExpectTotalCount(kFunnelHistogram, 0);
}

// Verifies the metrics logged for a successful attestation.
TEST_F(DeviceTrustChallengeTabHelperTest, SuccessfulResponseLogsMetrics) {
  WatchAllUrls();
  enterprise_connectors::DeviceTrustResponse service_response;
  service_response.challenge_response = "signed_payload_123";
  ReplyWith(service_response);
  AttestationResult response =
      SendRequest(url::Origin::Create(GURL(kExampleUrl)));
  EXPECT_FALSE(response.error.has_value());
  histogram_tester_.ExpectBucketCount(
      kFunnelHistogram,
      enterprise_connectors::DTAttestationFunnelStep::kChallengeReceived, 1);
  histogram_tester_.ExpectBucketCount(
      kFunnelHistogram,
      enterprise_connectors::DTAttestationFunnelStep::kChallengeResponseSent,
      1);
  histogram_tester_.ExpectUniqueSample(
      kPolicyLevelHistogram,
      enterprise_connectors::DTAttestationPolicyLevel::kUser, 1);
  histogram_tester_.ExpectUniqueSample(
      kHandshakeResultHistogram,
      enterprise_connectors::DTHandshakeResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kSuccessLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kFailureLatencyHistogram, 0);
}

// Verifies the metrics logged when the service fails to build a response.
TEST_F(DeviceTrustChallengeTabHelperTest, ServiceErrorLogsHandshakeFailure) {
  WatchAllUrls();
  enterprise_connectors::DeviceTrustResponse service_response;
  service_response.error =
      enterprise_connectors::DeviceTrustError::kFailedToCreateResponse;
  ReplyWith(service_response);
  SendRequest(url::Origin::Create(GURL(kExampleUrl)));
  histogram_tester_.ExpectUniqueSample(
      kHandshakeResultHistogram,
      enterprise_connectors::DTHandshakeResult::kFailedToCreateResponse, 1);
  histogram_tester_.ExpectTotalCount(kFailureLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kSuccessLatencyHistogram, 0);
  histogram_tester_.ExpectBucketCount(
      kFunnelHistogram,
      enterprise_connectors::DTAttestationFunnelStep::kChallengeResponseSent,
      0);
}

// Verifies that an empty service response without an error is logged as an
// unknown handshake failure.
TEST_F(DeviceTrustChallengeTabHelperTest, EmptyResponseLogsUnknownFailure) {
  WatchAllUrls();
  ReplyWith(enterprise_connectors::DeviceTrustResponse());
  SendRequest(url::Origin::Create(GURL(kExampleUrl)));
  histogram_tester_.ExpectUniqueSample(
      kHandshakeResultHistogram,
      enterprise_connectors::DTHandshakeResult::kUnknown, 1);
  histogram_tester_.ExpectTotalCount(kFailureLatencyHistogram, 1);
}

// Verifies that a timeout is logged once with its latency, and that the late
// service reply is not logged again.
TEST_F(DeviceTrustChallengeTabHelperTest, TimeoutLogsHandshakeTimeout) {
  WatchAllUrls();
  enterprise_connectors::DeviceTrustService::DeviceTrustCallback saved_callback;
  ON_CALL(*mock_service(),
          BuildChallengeResponse(testing::_, testing::_, testing::_))
      .WillByDefault(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { saved_callback = std::move(callback); });
  helper()->BuildChallengeResponse(url::Origin::Create(GURL(kExampleUrl)),
                                   GURL(kExampleUrl), "challenge",
                                   CaptureResponseCallback(nullptr));
  task_environment_.FastForwardBy(base::Seconds(25));
  histogram_tester_.ExpectUniqueSample(
      kHandshakeResultHistogram,
      enterprise_connectors::DTHandshakeResult::kTimeout, 1);
  // The 25s timeout exceeds the 10s range of UmaHistogramTimes and lands in the
  // overflow bucket, so only the count is meaningful.
  histogram_tester_.ExpectTotalCount(kFailureLatencyHistogram, 1);
  enterprise_connectors::DeviceTrustResponse late_response;
  late_response.challenge_response = "late_payload";
  std::move(saved_callback).Run(late_response);
  histogram_tester_.ExpectTotalCount(kHandshakeResultHistogram, 1);
  histogram_tester_.ExpectTotalCount(kSuccessLatencyHistogram, 0);
}

// Verifies that requests rejected before reaching the service are neither
// counted as received challenges nor logged as handshake outcomes.
TEST_F(DeviceTrustChallengeTabHelperTest,
       EarlyRejectionsDoNotLogHandshakeMetrics) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(false));
  EXPECT_EQ(SendRequest(url::Origin::Create(GURL(kExampleUrl))).error,
            enterprise_connectors::DeviceTrustError::kServiceUnavailable);
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  EXPECT_EQ(SendRequest(url::Origin()).error,
            enterprise_connectors::DeviceTrustError::kInvalidOrigin);
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(
          testing::Return(std::set<enterprise_connectors::DTCPolicyLevel>()));
  EXPECT_EQ(SendRequest(url::Origin::Create(GURL(kExampleUrl))).error,
            enterprise_connectors::DeviceTrustError::kUrlNotAllowed);
  histogram_tester_.ExpectTotalCount(kFunnelHistogram, 0);
  histogram_tester_.ExpectTotalCount(kPolicyLevelHistogram, 0);
  histogram_tester_.ExpectTotalCount(kHandshakeResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSuccessLatencyHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFailureLatencyHistogram, 0);
}

// Verifies that a request rejected with TOO_MANY_REQUESTS is neither counted
// as a received challenge nor logged as a handshake outcome.
TEST_F(DeviceTrustChallengeTabHelperTest,
       TooManyRequestsDoesNotLogHandshakeMetrics) {
  WatchAllUrls();
  std::vector<enterprise_connectors::DeviceTrustService::DeviceTrustCallback>
      callbacks;
  ON_CALL(*mock_service(),
          BuildChallengeResponse(testing::_, testing::_, testing::_))
      .WillByDefault(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { callbacks.push_back(std::move(callback)); });
  for (size_t i = 0; i < DeviceTrustChallengeTabHelper::kMaxPendingRequests;
       ++i) {
    helper()->BuildChallengeResponse(url::Origin::Create(GURL(kExampleUrl)),
                                     GURL(kExampleUrl), "challenge",
                                     CaptureResponseCallback(nullptr));
  }
  EXPECT_EQ(SendRequest(url::Origin::Create(GURL(kExampleUrl))).error,
            enterprise_connectors::DeviceTrustError::kTooManyRequests);
  histogram_tester_.ExpectUniqueSample(
      kFunnelHistogram,
      enterprise_connectors::DTAttestationFunnelStep::kChallengeReceived,
      DeviceTrustChallengeTabHelper::kMaxPendingRequests);
  histogram_tester_.ExpectUniqueSample(
      kPolicyLevelHistogram,
      enterprise_connectors::DTAttestationPolicyLevel::kUser,
      DeviceTrustChallengeTabHelper::kMaxPendingRequests);
  histogram_tester_.ExpectTotalCount(kHandshakeResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFailureLatencyHistogram, 0);
}

// Verifies that a committed cross-document navigation drops pending page reply
// callbacks, cancels their timeout timers, and ignores any subsequent service
// response.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseCommittedCrossDocumentNavigationDropsReply) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));
  enterprise_connectors::DeviceTrustService::DeviceTrustCallback saved_callback;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .WillOnce(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { saved_callback = std::move(callback); });
  AttestationResult response;
  int response_count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&response, &response_count));
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(response_count, 0);
  // A committed cross-document navigation occurs.
  web::FakeNavigationContext context;
  context.SetWebState(web_state_.get());
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_->OnNavigationFinished(&context);
  EXPECT_EQ(response_count, 0);
  // Fast forward past timeout duration to verify the timer was cancelled.
  task_environment_.FastForwardBy(base::Seconds(25));
  EXPECT_EQ(response_count, 0);
  // Late response from the service produces no reply.
  enterprise_connectors::DeviceTrustResponse service_response;
  service_response.challenge_response = "late_payload";
  std::move(saved_callback).Run(service_response);
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(response_count, 0);
}

// Verifies that a same-document navigation preserves pending page replies,
// allowing the eventual service response to resolve the callback.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseSameDocumentNavigationPreservesReply) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));
  enterprise_connectors::DeviceTrustService::DeviceTrustCallback saved_callback;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .WillOnce(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { saved_callback = std::move(callback); });
  AttestationResult response;
  int response_count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&response, &response_count));
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(response_count, 0);
  // A same-document navigation occurs.
  web::FakeNavigationContext context;
  context.SetWebState(web_state_.get());
  context.SetHasCommitted(true);
  context.SetIsSameDocument(true);
  web_state_->OnNavigationFinished(&context);
  EXPECT_EQ(response_count, 0);
  // Service response arrives and resolves the preserved page reply.
  enterprise_connectors::DeviceTrustResponse service_response;
  service_response.challenge_response = "success_payload";
  std::move(saved_callback).Run(service_response);
  EXPECT_EQ(response_count, 1);
  EXPECT_FALSE(response.error.has_value());
  EXPECT_EQ(response.challenge_response, "success_payload");
}

// Verifies that an uncommitted navigation preserves pending page replies,
// allowing the eventual service response to resolve the callback.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseUncommittedNavigationPreservesReply) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));
  enterprise_connectors::DeviceTrustService::DeviceTrustCallback saved_callback;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .WillOnce(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { saved_callback = std::move(callback); });
  AttestationResult response;
  int response_count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl), "challenge",
      CaptureResponseCallback(&response, &response_count));
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(response_count, 0);
  // An uncommitted navigation occurs.
  web::FakeNavigationContext context;
  context.SetWebState(web_state_.get());
  context.SetHasCommitted(false);
  context.SetIsSameDocument(false);
  web_state_->OnNavigationFinished(&context);
  EXPECT_EQ(response_count, 0);
  // Service response arrives and resolves the preserved page reply.
  enterprise_connectors::DeviceTrustResponse service_response;
  service_response.challenge_response = "success_payload";
  std::move(saved_callback).Run(service_response);
  EXPECT_EQ(response_count, 1);
  EXPECT_FALSE(response.error.has_value());
  EXPECT_EQ(response.challenge_response, "success_payload");
}

// Verifies that after a committed cross-document navigation clears pending
// replies, a new request on the new document is accepted and can reach
// DeviceTrustService.
TEST_F(DeviceTrustChallengeTabHelperTest,
       BuildChallengeResponseAfterNavigationNewRequestAccepted) {
  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));
  // Fill pending requests up to the limit.
  std::vector<enterprise_connectors::DeviceTrustService::DeviceTrustCallback>
      callbacks;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .Times(DeviceTrustChallengeTabHelper::kMaxPendingRequests)
      .WillRepeatedly(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { callbacks.push_back(std::move(callback)); });
  std::vector<AttestationResult> old_responses(
      DeviceTrustChallengeTabHelper::kMaxPendingRequests);
  for (size_t i = 0; i < DeviceTrustChallengeTabHelper::kMaxPendingRequests;
       ++i) {
    helper()->BuildChallengeResponse(
        url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl),
        "old_challenge", CaptureResponseCallback(&old_responses[i]));
  }
  EXPECT_EQ(callbacks.size(),
            DeviceTrustChallengeTabHelper::kMaxPendingRequests);
  // Committed cross-document navigation occurs.
  web::FakeNavigationContext context;
  context.SetWebState(web_state_.get());
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_->OnNavigationFinished(&context);
  // A new request on the new document is accepted and reaches the service.
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse("new_challenge", levels, testing::_))
      .WillOnce(
          [](const std::string&,
             const std::set<enterprise_connectors::DTCPolicyLevel>&,
             enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                 callback) {
            enterprise_connectors::DeviceTrustResponse response;
            response.challenge_response = "new_success_payload";
            std::move(callback).Run(response);
          });
  AttestationResult new_response;
  int new_count = 0;
  helper()->BuildChallengeResponse(
      url::Origin::Create(GURL(kExampleUrl)), GURL(kExampleUrl),
      "new_challenge", CaptureResponseCallback(&new_response, &new_count));
  EXPECT_EQ(new_count, 1);
  EXPECT_FALSE(new_response.error.has_value());
  EXPECT_EQ(new_response.challenge_response, "new_success_payload");
  // Running old callbacks from the previous document produces no replies.
  enterprise_connectors::DeviceTrustResponse old_late_response;
  old_late_response.challenge_response = "old_late_payload";
  for (auto& callback : callbacks) {
    std::move(callback).Run(old_late_response);
  }
  for (size_t i = 0; i < DeviceTrustChallengeTabHelper::kMaxPendingRequests;
       ++i) {
    EXPECT_TRUE(old_responses[i].challenge_response.empty());
  }
}

}  // namespace
