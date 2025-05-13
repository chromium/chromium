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
#import "components/sessions/core/mock_tab_restore_service.h"
#import "ios/chrome/browser/sessions/model/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
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
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    BrowserListFactory::GetForProfile(profile_.get())
        ->AddBrowser(browser_.get());

#if TARGET_IPHONE_SIMULATOR
    external_file_remover_ = std::make_unique<ExternalFileRemoverImpl>(
        profile_.get(), tab_restore_service());
#else
    // System file permission access is not accessible on device tests, set up
    // a custom external file directory.
    external_file_remover_ = std::make_unique<ExternalFileRemoverImpl>(
        profile_.get(), tab_restore_service(),
        base::apple::FilePathToNSString(GetInboxDirectoryPath()));
#endif
  }

  ExternalFileRemoverImpl* external_file_remover() {
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
    NSString* inbox_directory_path;
#if TARGET_IPHONE_SIMULATOR
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                         NSUserDomainMask, YES);
    inbox_directory_path =
        [[paths objectAtIndex:0] stringByAppendingPathComponent:@"Inbox"];
#else
    NSString* documents_directory_path =
        base::apple::FilePathToNSString(base::apple::GetUserLibraryPath());
    inbox_directory_path =
        [documents_directory_path stringByAppendingPathComponent:@"Inbox"];
#endif
    return base::apple::NSStringToFilePath(inbox_directory_path);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;

  std::unique_ptr<ExternalFileRemoverImpl> external_file_remover_;
};

// Tests that an external PDF file that is not in use is removed by the
// background task.
TEST_F(ExternalFileRemoverImplTest, RemoveDownloadedPDF) {
  const std::string& filename = "filename.pdf";
  CreateExternalFile(filename);
  VerifyExternalFileExists(filename);

  base::RunLoop run_loop;
  external_file_remover()->RemoveAfterDelay(base::Seconds(0),
                                            run_loop.QuitClosure());
  run_loop.Run();

  VerifyExternalFileAbsent(filename);
}

// Tests that an external PDF that is still referenced in a browser tab is not
// removed in a background task.
TEST_F(ExternalFileRemoverImplTest, DoNotRemoveReferencedPDF) {
  const std::string& filename = "filename.pdf";
  CreateExternalFile(filename);
  VerifyExternalFileExists(filename);

  const GURL kExternalFileUrl("chrome://external-file/filename.pdf");
  auto passed_web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state = passed_web_state.get();
  web_state->SetCurrentURL(kExternalFileUrl);
  web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  browser_->GetWebStateList()->InsertWebState(
      std::move(passed_web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  base::RunLoop run_loop;
  external_file_remover()->RemoveAfterDelay(base::Seconds(0),
                                            run_loop.QuitClosure());
  run_loop.Run();

  VerifyExternalFileExists(filename);
}

// Tests that deallocating the tab restore service while external file deletion
// is in progress does not crash the app.
// Regression test for crbug.com/406568566.
TEST_F(ExternalFileRemoverImplTest, TabRestoreServiceNotLoaded) {
  const std::string& filename = "filename.pdf";
  CreateExternalFile(filename);
  VerifyExternalFileExists(filename);

  MockTabRestoreService service;
  ON_CALL(service, IsLoaded()).WillByDefault(testing::Return(false));
  external_file_remover()->TabRestoreServiceChanged(&service);

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
