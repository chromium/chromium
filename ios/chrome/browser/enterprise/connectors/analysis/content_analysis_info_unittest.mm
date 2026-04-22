// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/analysis_test_utils.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

namespace {

constexpr char kTestProfileEmail[] = "test@example.com";
constexpr char kTestDomain[] = "example.com";
constexpr char16_t kTestTabTitle[] = u"example title";

class FakeBinaryUploadRequest : public BinaryUploadRequest {
 public:
  FakeBinaryUploadRequest(CloudOrLocalAnalysisSettings settings)
      : BinaryUploadRequest(
            base::DoNothing(),
            settings,
            base::BindRepeating(
                []() -> policy::BrowserPolicyConnector* { return nullptr; })) {}
  ~FakeBinaryUploadRequest() override = default;

  void GetRequestData(DataCallback callback) override {}
};

}  // namespace

class ContentAnalysisInfoTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    web_state_->SetCurrentURL(GURL(kTestDomain));
    web_state_->SetTitle(kTestTabTitle);
  }

  TestProfileIOS* profile() { return profile_.get(); }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  web::WebState* web_state() { return web_state_.get(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

TEST_F(ContentAnalysisInfoTest, SetupCloudBinaryUploadRequest) {
  signin::MakePrimaryAccountAvailable(identity_manager(), kTestProfileEmail,
                                      signin::ConsentLevel::kSignin);

  AnalysisSettings settings = std::move(*test::NormalDlpAndMalwareSettings());
  ContentAnalysisRequest::Reason reason =
      ContentAnalysisRequest::NORMAL_DOWNLOAD;
  // Passing settings from test utils.
  ContentAnalysisInfo info(GURL(kTestDomain), std::move(settings), reason,
                           web_state()->GetWeakPtr());
  FakeBinaryUploadRequest request(
      test::NormalDlpAndMalwareSettings()->cloud_or_local_settings);

  info.InitializeRequest(&request, /*include_enterprise_only_fields*/ true);

  // Verify BinaryUploadRequest data.
  EXPECT_EQ(request.device_token(),
            settings.cloud_or_local_settings.dm_token());
  // Tab title should only be set for local analysis.
  EXPECT_NE(request.tab_title(), base::UTF16ToUTF8(kTestTabTitle));
  EXPECT_EQ(request.tab_title(), std::string());
  EXPECT_EQ(request.reason(), reason);
  EXPECT_EQ(request.tab_url(), GURL(kTestDomain));
  EXPECT_EQ(request.per_profile_request(), settings.per_profile);

  // Verify ContentAnalysisInfo data.
  EXPECT_EQ(info.url(), GURL(kTestDomain));
  EXPECT_EQ(info.tab_url(), GURL(kTestDomain));
  EXPECT_EQ(info.user_action_requests_count(), 1);
  EXPECT_EQ(info.email(), kTestProfileEmail);
  EXPECT_EQ(info.reason(), reason);
}

}  // namespace enterprise_connectors
