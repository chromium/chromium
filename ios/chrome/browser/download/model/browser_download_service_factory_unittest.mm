// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/browser_download_service_factory.h"

#import "ios/chrome/browser/download/model/browser_download_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for testing BrowserDownloadServiceFactory class.
class BrowserDownloadServiceFactoryTest : public PlatformTest {
 protected:
  BrowserDownloadServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}
  // ProfileIOS needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that BrowserDownloadServiceFactory creates BrowserDownloadService and
// sets it as DownloadControllerDelegate.
TEST_F(BrowserDownloadServiceFactoryTest, Delegate) {
  web::DownloadController* download_controller =
      web::DownloadController::FromBrowserState(profile_.get());
  ASSERT_TRUE(download_controller);

  BrowserDownloadService* service =
      BrowserDownloadServiceFactory::GetForProfile(profile_.get());
  EXPECT_EQ(service, download_controller->GetDelegate());
}
