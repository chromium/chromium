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
#import "base/time/time.h"
#import "base/values.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
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

    DeviceTrustChallengeTabHelper::CreateForWebState(web_state_.get());
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

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_ = nullptr;
};

// Verifies that the tab helper is created and attached to the WebState.
TEST_F(DeviceTrustChallengeTabHelperTest, CreatesSuccessfully) {
  EXPECT_NE(helper(), nullptr);
}

// Verifies that when a main web frame becomes available, the Device Trust API
// setup script is executed on that frame.
TEST_F(DeviceTrustChallengeTabHelperTest, SetUpAPIForMainFrame) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kExampleUrl));
  main_frame->set_browser_state(profile_.get());
  web::FakeWebFrame* main_frame_ptr = main_frame.get();

  web_frames_manager_->AddWebFrame(std::move(main_frame));

  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
  EXPECT_EQ(main_frame_ptr->GetLastJavaScriptCall(),
            u"__gCrWeb.callFunctionInGcrWeb('deviceTrust', "
            u"'setupDeviceTrustAPI', []);");
}

// Verifies that non-main (child) frames do not trigger the API setup.
TEST_F(DeviceTrustChallengeTabHelperTest, IgnoreChildFrame) {
  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(GURL(kExampleUrl));
  child_frame->set_browser_state(profile_.get());
  web::FakeWebFrame* child_frame_ptr = child_frame.get();

  web_frames_manager_->AddWebFrame(std::move(child_frame));

  EXPECT_EQ(child_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Verifies that removing the current main frame does not prevent the API from
// being set up in a replacement main frame.
TEST_F(DeviceTrustChallengeTabHelperTest, SetupAPIAfterMainFrameRemoved) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kExampleUrl));
  main_frame->set_browser_state(profile_.get());
  const std::string frame_id = main_frame->GetFrameId();

  web_frames_manager_->AddWebFrame(std::move(main_frame));
  web_frames_manager_->RemoveWebFrame(frame_id);

  auto replacement_frame =
      web::FakeWebFrame::CreateMainWebFrame(GURL(kExampleUrl));
  replacement_frame->set_browser_state(profile_.get());
  web::FakeWebFrame* replacement_frame_ptr = replacement_frame.get();

  web_frames_manager_->AddWebFrame(std::move(replacement_frame));

  EXPECT_EQ(replacement_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
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
  std::unique_ptr<TestProfileIOS> empty_profile =
      TestProfileIOS::Builder().Build();
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(empty_profile.get());
  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web_state->SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                 std::move(frames_manager));
  DeviceTrustChallengeTabHelper::CreateForWebState(web_state.get());
  DeviceTrustChallengeTabHelper* helper =
      DeviceTrustChallengeTabHelper::FromWebState(web_state.get());

  base::RunLoop run_loop;
  AttestationResult response;
  int count = 0;
  helper->BuildChallengeResponse(
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
  SetAllowlistPatterns({"https://example.com/login"});

  const GURL request_url("https://example.com/login");
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
  SetAllowlistPatterns({"https://example.com/login"});

  const GURL request_url("https://example.com/other");
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
  SetAllowlistPatterns({"https://example.com"});

  const GURL request_url("https://attacker.com/login");
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

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
  SetAllowlistPatterns({"https://example.com"});

  const GURL invalid_url;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

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
  SetAllowlistPatterns({"https://example.com"});

  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

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
  SetAllowlistPatterns({"*"});

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
  EXPECT_EQ(response.error, enterprise_connectors::DeviceTrustError::kUnknown);
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

}  // namespace
