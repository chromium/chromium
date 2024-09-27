// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_tab_helper.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/drive/model/upload_task.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";

}  // namespace

// DriveTabHelper unit tests.
class DriveTabHelperTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kIOSSaveToDrive);
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    download_task_ =
        std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
    download_task_->SetWebState(web_state_.get());
    helper_ = DriveTabHelper::GetOrCreateForWebState(web_state_.get());
  }

  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<web::FakeDownloadTask> download_task_;
  raw_ptr<DriveTabHelper> helper_;
};

// Tests that upon `DownloadTask` being destroyed, the `DriveTabHelper` stops
// observing it i.e. removes itself from observers and resets its state. The
// "removes itself from observers" bit is checked implicitly when the
// `DownloadTask` destructor is called, since neglecting to remove itself as
// observer would lead to a crash.
TEST_F(DriveTabHelperTest, StopsObservingDestroyedDownloadTask) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  helper_->AddDownloadToSaveToDrive(download_task_.get(), identity);
  UploadTask* upload_task =
      helper_->GetUploadTaskForDownload(download_task_.get());
  ASSERT_NE(nullptr, upload_task);
  EXPECT_EQ(identity, upload_task->GetIdentity());
  download_task_.reset();
  EXPECT_EQ(nullptr, helper_->GetUploadTaskForDownload(download_task_.get()));
}
