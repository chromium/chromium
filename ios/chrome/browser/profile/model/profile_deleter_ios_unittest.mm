// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_deleter_ios.h"

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/browser_state_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Returns a callback that captures the value it has been invoked with and
// stores it into `captured_value`. The returned callback must not outlive
// the pointer.
template <typename... Args>
base::OnceCallback<void(Args...)> CaptureArgs(Args*... captured_value) {
  return base::BindOnce([](Args*... captured_value,
                           Args... value) { ((*captured_value = value), ...); },
                        captured_value...);
}

}  // namespace

using ProfileDeleterIOSTest = PlatformTest;

// Tests that DeleteProfile(...) works correctly.
TEST_F(ProfileDeleterIOSTest, DeleteProfile) {
  base::test::TaskEnvironment env;
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath storage_dir = scoped_temp_dir.GetPath();

  const base::Uuid profile_uuid = base::Uuid::GenerateRandomV4();
  const std::string profile_name = profile_uuid.AsLowercaseString();
  const base::FilePath profile_dir = storage_dir.AppendASCII(profile_name);
  ASSERT_FALSE(base::DirectoryExists(profile_dir));

  std::unique_ptr<TestProfileIOS> profile;
  {
    TestProfileIOS::Builder builder;
    builder.SetName(profile_name);
    builder.SetWebkitStorageId(profile_uuid);
    profile = std::move(builder).Build(storage_dir);
  }

  ASSERT_TRUE(base::DirectoryExists(profile_dir));

  bool success = false;
  std::string deleted_name;
  base::RunLoop run_loop;

  ProfileDeleterIOS::DeleteProfile(
      std::move(profile),
      CaptureArgs(&deleted_name, &success).Then(run_loop.QuitClosure()));
  run_loop.Run();

  // The profile data should have been deleted and the success reported.
  EXPECT_FALSE(base::DirectoryExists(profile_dir));
  EXPECT_EQ(deleted_name, profile_name);
  EXPECT_TRUE(success);
}
