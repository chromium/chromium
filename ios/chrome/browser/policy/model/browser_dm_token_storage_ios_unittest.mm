// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/browser_dm_token_storage_ios.h"

#import <Foundation/Foundation.h>

#import "base/base64url.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/hash/sha1.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_path_override.h"
#import "base/test/task_environment.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace policy {

namespace {

const char kDMToken[] = "fake-dm-token";
const char kDmTokenBaseDir[] =
    FILE_PATH_LITERAL("Google/Chrome Cloud Enrollment/");
const char kEnrollmentToken[] = "fake-enrollment-token";
const char kEnrollmentTokenPolicyName[] = "CloudManagementEnrollmentToken";

}  // namespace

class BrowserDMTokenStorageIOSTest : public PlatformTest {
 protected:
  BrowserDMTokenStorageIOSTest() {
    // Make sure there is no pre-existing policy present.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
  }
  ~BrowserDMTokenStorageIOSTest() override {
    // Cleanup any policies left from the test.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kPolicyLoaderIOSConfigurationKey];

    // Cleanup registerDefaults usage.
    [[NSUserDefaults standardUserDefaults]
        setVolatileDomain:@{}
                  forName:NSRegistrationDomain];
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

class TestStoreDMTokenDelegate {
 public:
  TestStoreDMTokenDelegate() : called_(false), success_(true) {}
  ~TestStoreDMTokenDelegate() {}

  void OnDMTokenUpdated(bool success) {
    run_loop_.Quit();
    called_ = true;
    success_ = success;
  }

  bool WasCalled() {
    bool was_called = called_;
    called_ = false;
    return was_called;
  }

  bool success() { return success_; }

  void Wait() { run_loop_.Run(); }

 private:
  bool called_;
  bool success_;
  base::RunLoop run_loop_;
};

TEST_F(BrowserDMTokenStorageIOSTest, InitClientId) {
  BrowserDMTokenStorageIOS storage;
  EXPECT_FALSE(storage.InitClientId().empty());
}

TEST_F(BrowserDMTokenStorageIOSTest, InitEnrollmentToken) {
  BrowserDMTokenStorageIOS storage;
  EXPECT_TRUE(storage.InitEnrollmentToken().empty());

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSDictionary* testing_policies = @{
    base::SysUTF8ToNSString(kEnrollmentTokenPolicyName) :
        base::SysUTF8ToNSString(kEnrollmentToken)
  };
  NSDictionary* registration_defaults =
      @{kPolicyLoaderIOSConfigurationKey : testing_policies};
  [defaults registerDefaults:registration_defaults];

  EXPECT_EQ(kEnrollmentToken, storage.InitEnrollmentToken());
}

TEST_F(BrowserDMTokenStorageIOSTest, StoreAndLoadDMToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override = std::make_unique<base::ScopedPathOverride>(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath());

  TestStoreDMTokenDelegate callback_delegate;
  BrowserDMTokenStorageIOS storage_delegate;
  auto task = storage_delegate.SaveDMTokenTask(kDMToken,
                                               storage_delegate.InitClientId());
  auto reply = base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenUpdated,
                              base::Unretained(&callback_delegate));
  storage_delegate.SaveDMTokenTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(task), std::move(reply));

  callback_delegate.Wait();
  ASSERT_TRUE(callback_delegate.WasCalled());
  ASSERT_TRUE(callback_delegate.success());

  base::FilePath app_data_dir_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &app_data_dir_path));
  base::FilePath dm_token_dir_path = app_data_dir_path.Append(kDmTokenBaseDir);

  std::string filename;
  base::Base64UrlEncode(base::SHA1HashString(storage_delegate.InitClientId()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);

  base::FilePath dm_token_file_path = dm_token_dir_path.Append(filename);

  std::string dm_token;
  ASSERT_TRUE(base::ReadFileToString(dm_token_file_path, &dm_token));
  EXPECT_EQ(kDMToken, dm_token);
  EXPECT_EQ(kDMToken, storage_delegate.InitDMToken());
}

TEST_F(BrowserDMTokenStorageIOSTest, DeleteDMToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override = std::make_unique<base::ScopedPathOverride>(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath());

  // Creating the DMToken file.
  base::FilePath app_data_dir_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &app_data_dir_path));
  base::FilePath dm_token_dir_path = app_data_dir_path.Append(kDmTokenBaseDir);
  ASSERT_TRUE(base::CreateDirectory(dm_token_dir_path));

  std::string filename;
  BrowserDMTokenStorageIOS storage_delegate;
  base::Base64UrlEncode(base::SHA1HashString(storage_delegate.InitClientId()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);
  base::FilePath dm_token_file_path = dm_token_dir_path.Append(filename);
  ASSERT_TRUE(base::WriteFile(base::FilePath(dm_token_file_path), kDMToken));
  ASSERT_TRUE(base::PathExists(dm_token_file_path));

  // Deleting the saved DMToken.
  TestStoreDMTokenDelegate delete_callback_delegate;
  auto delete_task =
      storage_delegate.DeleteDMTokenTask(storage_delegate.InitClientId());
  auto delete_reply =
      base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenUpdated,
                     base::Unretained(&delete_callback_delegate));
  storage_delegate.SaveDMTokenTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(delete_task), std::move(delete_reply));

  delete_callback_delegate.Wait();
  ASSERT_TRUE(delete_callback_delegate.WasCalled());
  ASSERT_TRUE(delete_callback_delegate.success());

  ASSERT_FALSE(base::PathExists(dm_token_file_path));
}

TEST_F(BrowserDMTokenStorageIOSTest, DeleteEmptyDMToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override = std::make_unique<base::ScopedPathOverride>(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath());

  BrowserDMTokenStorageIOS storage_delegate;
  base::FilePath app_data_dir_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &app_data_dir_path));
  base::FilePath dm_token_dir_path = app_data_dir_path.Append(kDmTokenBaseDir);
  std::string filename;
  base::Base64UrlEncode(base::SHA1HashString(storage_delegate.InitClientId()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);
  base::FilePath dm_token_file_path = dm_token_dir_path.Append(filename);

  ASSERT_FALSE(base::PathExists(dm_token_file_path));

  TestStoreDMTokenDelegate callback_delegate;
  auto delete_task =
      storage_delegate.DeleteDMTokenTask(storage_delegate.InitClientId());
  auto delete_reply =
      base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenUpdated,
                     base::Unretained(&callback_delegate));
  storage_delegate.SaveDMTokenTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(delete_task), std::move(delete_reply));

  callback_delegate.Wait();
  ASSERT_TRUE(callback_delegate.WasCalled());
  ASSERT_TRUE(callback_delegate.success());

  ASSERT_FALSE(base::PathExists(dm_token_file_path));
}

TEST_F(BrowserDMTokenStorageIOSTest, InitDMTokenWithoutDirectory) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override.reset(new base::ScopedPathOverride(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath()));

  TestStoreDMTokenDelegate delegate;
  BrowserDMTokenStorageIOS storage;

  base::FilePath dm_token_dir_path =
      fake_app_data_dir.GetPath().Append(kDmTokenBaseDir);

  EXPECT_EQ(std::string(), storage.InitDMToken());
  EXPECT_FALSE(base::PathExists(dm_token_dir_path));
}

}  // namespace policy
