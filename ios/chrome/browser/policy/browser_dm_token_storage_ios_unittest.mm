// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/browser_dm_token_storage_ios.h"

#import <Foundation/Foundation.h>

#include "base/base64url.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_runner_util.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

class TestStoreDMTokenDelegate {
 public:
  TestStoreDMTokenDelegate() : called_(false), success_(true) {}
  ~TestStoreDMTokenDelegate() {}

  void OnDMTokenStored(bool success) {
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
  path_override.reset(new base::ScopedPathOverride(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath()));

  TestStoreDMTokenDelegate callback_delegate;
  BrowserDMTokenStorageIOS storage_delegate;
  auto task = storage_delegate.SaveDMTokenTask(kDMToken,
                                               storage_delegate.InitClientId());
  auto reply = base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenStored,
                              base::Unretained(&callback_delegate));
  base::PostTaskAndReplyWithResult(
      storage_delegate.SaveDMTokenTaskRunner().get(), FROM_HERE,
      std::move(task), std::move(reply));

  callback_delegate.Wait();
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
