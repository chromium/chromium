// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/coordinator/page_info_permissions_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_info.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {
constexpr char kTestUrl[] = "https://example.com/";
}  // namespace

// Tests for Permissions mediator for the page info.
class PageInfoPermissionsTest : public PlatformTest {
 protected:
  PageInfoPermissionsTest() {}

  ~PageInfoPermissionsTest() override { [mediator_ disconnect]; }

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    fake_web_state_->SetCurrentURL(GURL(kTestUrl));
    web::WebState* web_state_ = fake_web_state_.get();

    // Initialize camera state to Allowed but keeps microphone state
    // NotAccessible.
    web_state_->SetStateForPermission(web::PermissionStateAllowed,
                                      web::PermissionCamera);
    web_state_->SetStateForPermission(web::PermissionStateNotAccessible,
                                      web::PermissionMicrophone);

    mediator_ =
        [[PageInfoPermissionsMediator alloc] initWithWebState:web_state_
                                       hostContentSettingsMap:settings_map()];
  }

  PageInfoPermissionsMediator* mediator() { return mediator_; }

  web::WebState* web_state() { return fake_web_state_.get(); }

  HostContentSettingsMap* settings_map() {
    return ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
        .get();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  PageInfoPermissionsMediator* mediator_;
};

// Test that `updatePermissionInfo:` updates correctly the web state
// permission.
TEST_F(PageInfoPermissionsTest, TestUpdateStateForPermission) {
  PermissionInfo* permissionDescription = [[PermissionInfo alloc] init];
  permissionDescription.permission = web::PermissionCamera;
  permissionDescription.state = web::PermissionStateBlocked;

  [mediator() updatePermissionInfo:permissionDescription];
  ASSERT_EQ(web_state()->GetStateForPermission(web::PermissionCamera),
            web::PermissionStateBlocked);
}

// Test that `updatePermissionInfo:` persists Always Allow, Never Allow,
// and Allow Once to `HostContentSettingsMap` and updates the web state when
// `kDomainLevelSitePermissions` is enabled.
TEST_F(PageInfoPermissionsTest, TestUpdateSettingForPermission) {
  base::test::ScopedFeatureList feature_list(kDomainLevelSitePermissions);
  GURL url(kTestUrl);

  // Select Always Allow for Camera.
  PermissionInfo* cameraAlwaysAllow = [[PermissionInfo alloc] init];
  cameraAlwaysAllow.permission = web::PermissionCamera;
  cameraAlwaysAllow.state = web::PermissionStateAllowed;
  cameraAlwaysAllow.setting = SitePermissionSetting::kAlwaysAllow;
  [mediator() updatePermissionInfo:cameraAlwaysAllow];

  EXPECT_EQ(web_state()->GetStateForPermission(web::PermissionCamera),
            web::PermissionStateAllowed);
  EXPECT_EQ(settings_map()->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA),
            CONTENT_SETTING_ALLOW);

  // Select Never Allow for Camera.
  PermissionInfo* cameraNeverAllow = [[PermissionInfo alloc] init];
  cameraNeverAllow.permission = web::PermissionCamera;
  cameraNeverAllow.state = web::PermissionStateNotAccessible;
  cameraNeverAllow.setting = SitePermissionSetting::kNeverAllow;
  [mediator() updatePermissionInfo:cameraNeverAllow];

  EXPECT_EQ(web_state()->GetStateForPermission(web::PermissionCamera),
            web::PermissionStateNotAccessible);
  EXPECT_EQ(settings_map()->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA),
            CONTENT_SETTING_BLOCK);

  // Select Allow Once for Camera.
  PermissionInfo* cameraAllowOnce = [[PermissionInfo alloc] init];
  cameraAllowOnce.permission = web::PermissionCamera;
  cameraAllowOnce.state = web::PermissionStateAllowed;
  cameraAllowOnce.setting = SitePermissionSetting::kAllowOnce;
  [mediator() updatePermissionInfo:cameraAllowOnce];

  EXPECT_EQ(web_state()->GetStateForPermission(web::PermissionCamera),
            web::PermissionStateAllowed);
  EXPECT_EQ(settings_map()->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA),
            CONTENT_SETTING_ASK);
}

// Test that setting a consumer when `kDomainLevelSitePermissions` is enabled
// dispatches the resolved `SitePermissionSetting` for accessible permissions.
TEST_F(PageInfoPermissionsTest, TestDispatchInitialPermissionSettings) {
  base::test::ScopedFeatureList feature_list(kDomainLevelSitePermissions);
  GURL url(kTestUrl);
  settings_map()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  id consumer = OCMProtocolMock(@protocol(PermissionsConsumer));
  OCMExpect([consumer
      setPermissionsInfo:[OCMArg checkWithBlock:^BOOL(
                                     NSArray<PermissionInfo*>* infos) {
        if (infos.count != 1) {
          return NO;
        }
        PermissionInfo* info = infos.firstObject;
        return info.permission == web::PermissionCamera &&
               info.state == web::PermissionStateAllowed &&
               info.setting == SitePermissionSetting::kAlwaysAllow;
      }]]);

  mediator().consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);
}

// Test that when web state permission is blocked, the domain setting resolves
// to `kNeverAllow` even if HostContentSettingsMap has `CONTENT_SETTING_ALLOW`.
TEST_F(PageInfoPermissionsTest, TestBlockedWebStateWithAllowContentSetting) {
  base::test::ScopedFeatureList feature_list(kDomainLevelSitePermissions);
  GURL url(kTestUrl);
  settings_map()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);
  web_state()->SetStateForPermission(web::PermissionStateBlocked,
                                     web::PermissionCamera);

  id consumer = OCMProtocolMock(@protocol(PermissionsConsumer));
  OCMExpect([consumer
      setPermissionsInfo:[OCMArg checkWithBlock:^BOOL(
                                     NSArray<PermissionInfo*>* infos) {
        if (infos.count != 1) {
          return NO;
        }
        PermissionInfo* info = infos.firstObject;
        return info.permission == web::PermissionCamera &&
               info.state == web::PermissionStateBlocked &&
               info.setting == SitePermissionSetting::kNeverAllow;
      }]]);

  mediator().consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);
}

// Test that when web state permission is blocked and HostContentSettingsMap
// is default/ask, the domain setting resolves to `kNeverAllow`.
TEST_F(PageInfoPermissionsTest, TestBlockedWebStateWithAskContentSetting) {
  base::test::ScopedFeatureList feature_list(kDomainLevelSitePermissions);
  GURL url(kTestUrl);
  settings_map()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ASK);
  web_state()->SetStateForPermission(web::PermissionStateBlocked,
                                     web::PermissionCamera);

  id consumer = OCMProtocolMock(@protocol(PermissionsConsumer));
  OCMExpect([consumer
      setPermissionsInfo:[OCMArg checkWithBlock:^BOOL(
                                     NSArray<PermissionInfo*>* infos) {
        if (infos.count != 1) {
          return NO;
        }
        PermissionInfo* info = infos.firstObject;
        return info.permission == web::PermissionCamera &&
               info.state == web::PermissionStateBlocked &&
               info.setting == SitePermissionSetting::kNeverAllow;
      }]]);

  mediator().consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);
}

// Test that an invalid URL safely defaults to `kAllowOnce` and does not crash
// or modify `HostContentSettingsMap`.
TEST_F(PageInfoPermissionsTest, TestInvalidUrlHandling) {
  base::test::ScopedFeatureList feature_list(kDomainLevelSitePermissions);
  static_cast<web::FakeWebState*>(web_state())->SetCurrentURL(GURL());

  id consumer = OCMProtocolMock(@protocol(PermissionsConsumer));
  OCMExpect([consumer
      setPermissionsInfo:[OCMArg checkWithBlock:^BOOL(
                                     NSArray<PermissionInfo*>* infos) {
        if (infos.count != 1) {
          return NO;
        }
        PermissionInfo* info = infos.firstObject;
        return info.permission == web::PermissionCamera &&
               info.setting == SitePermissionSetting::kAllowOnce;
      }]]);

  mediator().consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);

  PermissionInfo* cameraAlwaysAllow = [[PermissionInfo alloc] init];
  cameraAlwaysAllow.permission = web::PermissionCamera;
  cameraAlwaysAllow.setting = SitePermissionSetting::kAlwaysAllow;
  // Should not crash when persisting with an invalid URL.
  [mediator() updatePermissionInfo:cameraAlwaysAllow];
}

// Test that a permission with `PermissionStateNotAccessible` is still
// dispatched when a site-specific content setting exists.
TEST_F(PageInfoPermissionsTest,
       TestNotAccessibleWebStateWithSiteExceptionDispatchesPermissionInfo) {
  base::test::ScopedFeatureList feature_list(kDomainLevelSitePermissions);
  GURL url(kTestUrl);
  web_state()->SetStateForPermission(web::PermissionStateNotAccessible,
                                     web::PermissionCamera);
  settings_map()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_BLOCK);

  id consumer = OCMProtocolMock(@protocol(PermissionsConsumer));
  OCMExpect([consumer
      setPermissionsInfo:[OCMArg checkWithBlock:^BOOL(
                                     NSArray<PermissionInfo*>* infos) {
        if (infos.count != 1) {
          return NO;
        }
        PermissionInfo* info = infos.firstObject;
        return info.permission == web::PermissionCamera &&
               info.state == web::PermissionStateNotAccessible &&
               info.setting == SitePermissionSetting::kNeverAllow;
      }]]);

  mediator().consumer = consumer;
  EXPECT_OCMOCK_VERIFY(consumer);
}
