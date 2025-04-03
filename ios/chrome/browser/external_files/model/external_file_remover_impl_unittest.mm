// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/external_files/model/external_file_remover_impl.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/sessions/model/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForFileOperationTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

class ExternalFileRemoverImplTest : public PlatformTest {
 public:
  ExternalFileRemoverImplTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                              FakeTabRestoreService::GetTestingFactory());
    profile_ = std::move(builder).Build();
    external_file_remover_ = std::make_unique<ExternalFileRemoverImpl>(
        profile_.get(), tab_restore_service());
  }

  ExternalFileRemover* external_file_remover() {
    return external_file_remover_.get();
  }

  void CreateExternalFile(const std::string& filename) {
    base::FilePath inbox_directory = GetInboxDirectoryPath();
    if (!base::DirectoryExists(inbox_directory)) {
      LOG(WARNING) << "Directory does not exist, creating: "
                   << inbox_directory.value();
      ASSERT_TRUE(base::CreateDirectory(inbox_directory));
    }

    ASSERT_TRUE(base::WriteFile(
        inbox_directory.Append(FILE_PATH_LITERAL(filename)), "data"));
  }

  void VerifyExternalFileExists(const std::string& filename) {
    ASSERT_TRUE(base::PathExists(
        GetInboxDirectoryPath().Append(FILE_PATH_LITERAL(filename))));
  }

  void VerifyExternalFileAbsent(const std::string& filename) {
    ASSERT_FALSE(base::PathExists(
        GetInboxDirectoryPath().Append(FILE_PATH_LITERAL(filename))));
  }

 protected:
  sessions::TabRestoreService* tab_restore_service() {
    return IOSChromeTabRestoreServiceFactory::GetForProfile(profile_.get());
  }

  // Return the Inbox directory in User Documents where external files are
  // stored.
  base::FilePath GetInboxDirectoryPath() {
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                         NSUserDomainMask, YES);
    NSString* documents_directory_path =
        [[paths objectAtIndex:0] stringByAppendingPathComponent:@"Inbox"];
    return base::apple::NSStringToFilePath(documents_directory_path);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;

  std::unique_ptr<ExternalFileRemoverImpl> external_file_remover_;
};

// Tests that an external PDF file that is not in use is removed by the
// background task.
// TODO(crbug.com/408168811): Fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_RemoveDownloadedPDF RemoveDownloadedPDF
#else
#define MAYBE_RemoveDownloadedPDF DISABLED_RemoveDownloadedPDF
#endif
TEST_F(ExternalFileRemoverImplTest, MAYBE_RemoveDownloadedPDF) {
  const std::string& filename = "filename.pdf";
  CreateExternalFile(filename);
  VerifyExternalFileExists(filename);

  base::RunLoop run_loop;
  external_file_remover()->RemoveAfterDelay(base::Seconds(0),
                                            run_loop.QuitClosure());
  run_loop.Run();

  VerifyExternalFileAbsent(filename);
}

// Tests that the closure callback is called after the file removal operation.
TEST_F(ExternalFileRemoverImplTest, CallbackCalledPostRemoval) {
  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);
  base::RunLoop run_loop;
  external_file_remover()->RemoveAfterDelay(
      base::Seconds(0), std::move(closure).Then(run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_TRUE(callback_called);
}
