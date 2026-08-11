// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_modal_host.h"

#import <UIKit/UIKit.h>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/download/coordinator/download_list_coordinator.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/save_to_photos/ui_bundled/save_to_photos_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"
#import "url/origin.h"

// Test fixture for BrowserModalHost testing.
class BrowserModalHostTest : public PlatformTest {
 protected:
  BrowserModalHostTest() {
    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[SceneState alloc] init];
    LayoutGuideSceneAgent* layout_guide_scene_agent =
        [[LayoutGuideSceneAgent alloc] init];
    [scene_state_ addAgent:layout_guide_scene_agent];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    base_view_controller_ = [[UIViewController alloc] init];
    modal_host_ = [[BrowserModalHost alloc] initWithBrowser:browser_.get()];
    [modal_host_ setBaseViewControllerForModals:base_view_controller_];
    [modal_host_ startHostingCommandProtocols];
  }

  ~BrowserModalHostTest() override {
    [modal_host_ stopHostingCommandProtocols];
  }

  void InsertWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  web::WebState* GetActiveWebState() {
    return browser_->GetWebStateList()->GetActiveWebState();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  BrowserModalHost* modal_host_;
};

// Tests that BrowserModalHost starts and stops the SaveToPhotosCoordinator
// properly when SaveToPhotosCommands are issued.
TEST_F(BrowserModalHostTest, StartsAndStopsSaveToPhotosCoordinator) {
  // Mock the SaveToPhotosCoordinator class.
  id mockSaveToPhotosCoordinator =
      OCMStrictClassMock([SaveToPhotosCoordinator class]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<SaveToPhotosCommands> handler =
      HandlerForProtocol(dispatcher, SaveToPhotosCommands);

  // Insert a web state into the Browser.
  InsertWebState();

  GURL fakeImageURL("http://www.example.com/image.jpg");
  web::Referrer fakeImageReferrer;
  web::WebState* webState = GetActiveWebState();
  SaveImageToPhotosCommand* command = [[SaveImageToPhotosCommand alloc]
      initWithImageURL:fakeImageURL
              referrer:fakeImageReferrer
              webState:webState
               frameID:"fake_frame_id"
           frameOrigin:url::Origin::Create(GURL("http://chromium.test/"))];

  // Tests that -[BrowserModalHost saveImageToPhotos:] starts the
  // SaveToPhotosCoordinator.
  OCMExpect([mockSaveToPhotosCoordinator alloc])
      .andReturn(mockSaveToPhotosCoordinator);
  OCMExpect([[mockSaveToPhotosCoordinator ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()
                                  imageURL:command.imageURL
                                  referrer:command.referrer
                                  webState:command.webState.get()
                                   frameID:command.frameID
                               frameOrigin:command.frameOrigin])
      .andReturn(mockSaveToPhotosCoordinator);
  OCMExpect([(SaveToPhotosCoordinator*)mockSaveToPhotosCoordinator start]);
  [handler saveImageToPhotos:command];
  EXPECT_OCMOCK_VERIFY(mockSaveToPhotosCoordinator);

  // Tests that -[BrowserModalHost stopSaveToPhotos:] stops the
  // SaveToPhotosCoordinator.
  OCMExpect([mockSaveToPhotosCoordinator stop]);
  [handler stopSaveToPhotos];
  EXPECT_OCMOCK_VERIFY(mockSaveToPhotosCoordinator);
}

// Tests that `-showShareSheet` is leaving fullscreen and starting the share
// coordinator.
TEST_F(BrowserModalHostTest, ShowShareSheet) {
  TestFullscreenController::CreateForBrowser(browser_.get());
  TestFullscreenController* controller =
      TestFullscreenController::FromBrowser(browser_.get());

  controller->EnterFullscreen();
  ASSERT_EQ(0.0, controller->GetProgress());

  UIView* source = [[UIView alloc] init];

  id classMock = OCMClassMock([SharingCoordinator class]);
  SharingCoordinator* mockSharingCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:[OCMArg any]
                                   browser:browser_.get()
                                    params:[OCMArg any]
                                sourceItem:source])
      .andReturn(mockSharingCoordinator);
  OCMExpect([mockSharingCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<ActivityServiceCommands> handler =
      HandlerForProtocol(dispatcher, ActivityServiceCommands);

  [handler showShareSheetFromShareButton:source];

  // Check that fullscreen is exited.
  EXPECT_EQ(1.0, controller->GetProgress());

  // Check that -start has been called.
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showShareSheetForChromeApp` is instantiating the
// SharingCoordinator with SharingParams where scenario is ShareChrome, leaving
// fullscreen and starting the share coordinator.
TEST_F(BrowserModalHostTest, ShowShareSheetForChromeApp) {
  TestFullscreenController::CreateForBrowser(browser_.get());
  TestFullscreenController* controller =
      TestFullscreenController::FromBrowser(browser_.get());

  controller->EnterFullscreen();
  ASSERT_EQ(0.0, controller->GetProgress());

  id expectShareChromeScenarioArg =
      [OCMArg checkWithBlock:^BOOL(SharingParams* params) {
        return params.scenario == SharingScenario::ShareChrome;
      }];

  id classMock = OCMClassMock([SharingCoordinator class]);
  SharingCoordinator* mockSharingCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:[OCMArg any]
                                   browser:browser_.get()
                                    params:expectShareChromeScenarioArg
                                sourceItem:[OCMArg any]])
      .andReturn(mockSharingCoordinator);
  OCMExpect([mockSharingCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<ActivityServiceCommands> handler =
      HandlerForProtocol(dispatcher, ActivityServiceCommands);

  [handler showShareSheetForChromeApp];

  // Check that fullscreen is exited.
  EXPECT_EQ(1.0, controller->GetProgress());

  // Check that -start has been called.
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showDownloadList` starts the DownloadListCoordinator.
TEST_F(BrowserModalHostTest, ShowDownloadList) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kDownloadList);

  id classMock = OCMClassMock([DownloadListCoordinator class]);
  DownloadListCoordinator* mockCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()])
      .andReturn(mockCoordinator);
  OCMExpect([mockCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<DownloadListCommands> handler =
      HandlerForProtocol(dispatcher, DownloadListCommands);

  [handler showDownloadList];

  EXPECT_OCMOCK_VERIFY(classMock);
}
