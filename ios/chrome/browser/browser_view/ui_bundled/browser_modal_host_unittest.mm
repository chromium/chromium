// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_modal_host.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/save_to_photos/ui_bundled/save_to_photos_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
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
    browser_ = std::make_unique<TestBrowser>(profile_.get());
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
