// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/browser_download_service_factory.h"

#import "ios/chrome/browser/download/browser_download_service.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing BrowserDownloadServiceFactory class.
class BrowserDownloadServiceFactoryTest : public PlatformTest {
 protected:
  BrowserDownloadServiceFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}
  // ChromeBrowserState needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that BrowserDownloadServiceFactory creates BrowserDownloadService and
// sets it as DownloadControllerDelegate.
TEST_F(BrowserDownloadServiceFactoryTest, Delegate) {
  web::DownloadController* download_controller =
      web::DownloadController::FromBrowserState(browser_state_.get());
  ASSERT_TRUE(download_controller);

  BrowserDownloadService* service =
      BrowserDownloadServiceFactory::GetForBrowserState(browser_state_.get());
  EXPECT_EQ(service, download_controller->GetDelegate());
}
