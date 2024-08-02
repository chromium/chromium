// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/permissions/permissions_infobar_modal_overlay_mediator.h"

#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/permissions/model/permissions_infobar_delegate.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_info.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

// FakePermissionsConsumer used for testing purpose.
@interface FakePermissionsConsumer : NSObject <PermissionsConsumer>
@property(nonatomic, strong) NSString* permissionsDescription;
@property(nonatomic, strong) PermissionInfo* cameraInfo;
@property(nonatomic, strong) PermissionInfo* microphoneInfo;
@end

@implementation FakePermissionsConsumer

- (void)setPermissionsInfo:(NSArray<PermissionInfo*>*)permissionsInfo {
  for (PermissionInfo* info in permissionsInfo) {
    if (info.permission == web::PermissionCamera) {
      self.cameraInfo = info;
    } else {
      self.microphoneInfo = info;
    }
  }
}

- (void)permissionStateChanged:(PermissionInfo*)info {
  if (info.permission == web::PermissionCamera) {
    self.cameraInfo = info;
  } else {
    self.microphoneInfo = info;
  }
}

- (void)setPermissionsDescription:(NSString*)permissionsDescription {
  _permissionsDescription = permissionsDescription;
}

@end

// Test fixture for PermissionsInfobarModalOverlayMediator.
class PermissionsInfobarModalOverlayMediatorTest : public PlatformTest {
 public:
  PermissionsInfobarModalOverlayMediatorTest() {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    item_ = web::NavigationItem::Create();
    GURL url("http://test.com/");
    item_->SetURL(url);
    navigation_manager->SetVisibleItem(item_.get());
    web_state_.SetNavigationManager(std::move(navigation_manager));
    // First parameter is used for banner; not needed for this test.
    std::unique_ptr<PermissionsInfobarDelegate> delegate =
        std::make_unique<PermissionsInfobarDelegate>([NSArray array],
                                                     &web_state_);
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypePermissions, std::move(delegate));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kModal);
    mediator_ = [[PermissionsInfobarModalOverlayMediator alloc]
        initWithRequest:request_.get()];
  }

  ~PermissionsInfobarModalOverlayMediatorTest() override {
    [mediator_ disconnect];
  }

 protected:
  PermissionsInfobarModalOverlayMediator* mediator_;
  std::unique_ptr<OverlayRequest> request_;
  web::FakeWebState web_state_;
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<web::NavigationItem> item_;
};

// Tests that a PermissionsInfobarModalOverlayMediator correctly sets up its
// consumer.
TEST_F(PermissionsInfobarModalOverlayMediatorTest, SetUpConsumer) {
  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  FakePermissionsConsumer* consumer = [[FakePermissionsConsumer alloc] init];
  mediator_.consumer = consumer;
  EXPECT_EQ(web::PermissionStateNotAccessible, consumer.cameraInfo.state);
  EXPECT_EQ(web::PermissionStateNotAccessible, consumer.microphoneInfo.state);
  NSString* description = l10n_util::GetNSStringF(
      IDS_IOS_PERMISSIONS_INFOBAR_MODAL_DESCRIPTION, u"test.com");
  EXPECT_NSEQ(description, consumer.permissionsDescription);
}

// Tests that the mediator would update its consumer when web state permissions
// change.
TEST_F(PermissionsInfobarModalOverlayMediatorTest, PermissionStatesUpdate) {
  FakePermissionsConsumer* consumer = [[FakePermissionsConsumer alloc] init];
  mediator_.consumer = consumer;
  ASSERT_EQ(web::PermissionStateNotAccessible, consumer.cameraInfo.state);
  ASSERT_EQ(web::PermissionStateNotAccessible, consumer.microphoneInfo.state);
  // Update web state permission directly.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionCamera);
  EXPECT_EQ(web::PermissionStateAllowed, consumer.cameraInfo.state);
  EXPECT_EQ(web::PermissionStateNotAccessible, consumer.microphoneInfo.state);
  web_state_.SetStateForPermission(web::PermissionStateBlocked,
                                   web::PermissionMicrophone);
  EXPECT_EQ(web::PermissionStateAllowed, consumer.cameraInfo.state);
  EXPECT_EQ(web::PermissionStateBlocked, consumer.microphoneInfo.state);
}

// Tests that calling `updateStateForPermission:` updates both the consumer and
// web state permissions.
TEST_F(PermissionsInfobarModalOverlayMediatorTest,
       UpdatePermissionStatesThroughInfobarModal) {
  FakePermissionsConsumer* consumer = [[FakePermissionsConsumer alloc] init];
  mediator_.consumer = consumer;
  ASSERT_EQ(web::PermissionStateNotAccessible, consumer.cameraInfo.state);
  ASSERT_EQ(web::PermissionStateNotAccessible, consumer.microphoneInfo.state);
  // Update web state permission directly.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionCamera);
  ASSERT_EQ(web::PermissionStateAllowed, consumer.cameraInfo.state);

  // Update web state permissions through the infobar modal.
  PermissionInfo* permission_info_1 = [[PermissionInfo alloc] init];
  permission_info_1.permission = web::PermissionMicrophone;
  permission_info_1.state = web::PermissionStateAllowed;
  [mediator_ updateStateForPermission:permission_info_1];
  EXPECT_EQ(web::PermissionStateAllowed,
            web_state_.GetStateForPermission(web::PermissionCamera));
  EXPECT_EQ(web::PermissionStateAllowed, consumer.cameraInfo.state);
  EXPECT_EQ(web::PermissionStateAllowed,
            web_state_.GetStateForPermission(web::PermissionMicrophone));
  EXPECT_EQ(web::PermissionStateAllowed, consumer.microphoneInfo.state);

  PermissionInfo* permission_info_2 = [[PermissionInfo alloc] init];
  permission_info_2.permission = web::PermissionCamera;
  permission_info_2.state = web::PermissionStateBlocked;
  [mediator_ updateStateForPermission:permission_info_2];
  EXPECT_EQ(web::PermissionStateBlocked,
            web_state_.GetStateForPermission(web::PermissionCamera));
  EXPECT_EQ(web::PermissionStateBlocked, consumer.cameraInfo.state);
  EXPECT_EQ(web::PermissionStateAllowed,
            web_state_.GetStateForPermission(web::PermissionMicrophone));
  EXPECT_EQ(web::PermissionStateAllowed, consumer.microphoneInfo.state);
}

// Tests that a PermissionsInfobarModalOverlayMediator correctly removes itself
// as a WebStateObserver when the WebState is destroyed.
TEST_F(PermissionsInfobarModalOverlayMediatorTest, WebStateObserverRemoved) {
  std::unique_ptr<web::FakeNavigationManager> navigation_manager =
      std::make_unique<web::FakeNavigationManager>();
  navigation_manager->SetVisibleItem(item_.get());
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  web_state->SetNavigationManager(std::move(navigation_manager));
  std::unique_ptr<PermissionsInfobarDelegate> delegate =
      std::make_unique<PermissionsInfobarDelegate>([NSArray array],
                                                   web_state.get());
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypePermissions, std::move(delegate));
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
          infobar.get(), InfobarOverlayType::kModal);
  PermissionsInfobarModalOverlayMediator* mediator =
      [[PermissionsInfobarModalOverlayMediator alloc]
          initWithRequest:request.get()];
  FakePermissionsConsumer* consumer = [[FakePermissionsConsumer alloc] init];
  mediator.consumer = consumer;

  // Destroy the WebState. If `mediator_` doesn't remove itself as an observer,
  // the WebState's observer list will hit a DCHECK on destruction.
  web_state.reset();
}
