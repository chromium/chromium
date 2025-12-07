// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_deleter_ios.h"

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "base/uuid.h"
#import "components/prefs/json_pref_store.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
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

class ProfileDeleterIOSTest : public PlatformTest {
 public:
  ProfileDeleterIOSTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  const base::FilePath& storage_dir() const {
    return scoped_temp_dir_.GetPath();
  }

  // Creates the storage for a profile with a given name, WebKit storage
  // identifier, including the preference store.
  void CreateProfileStorage(const std::string& profile_name,
                            const base::Uuid& webkit_storage_id) {
    base::FilePath profile_dir = storage_dir().Append(profile_name);
    ASSERT_TRUE(base::CreateDirectory(profile_dir));

    auto pref_store =
        base::MakeRefCounted<JsonPrefStore>(profile_dir.Append("Preferences"));
    pref_store->SetValue(prefs::kBrowserStateStorageIdentifier,
                         base::Value(webkit_storage_id.AsLowercaseString()),
                         JsonPrefStore::DEFAULT_PREF_WRITE_FLAGS);

    base::RunLoop run_loop;
    pref_store->CommitPendingWrite(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Helper to delete a profile and return the result of the operation.
  ProfileDeleterIOS::Result DeleteProfile(const std::string& profile_name) {
    ProfileDeleterIOS deleter;

    base::RunLoop run_loop;
    ProfileDeleterIOS::Result result = ProfileDeleterIOS::Result::kFailure;
    deleter.DeleteProfile(profile_name, storage_dir(),
                          CaptureArgs(&result).Then(run_loop.QuitClosure()));
    run_loop.Run();

    return result;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
};

// Tests that DeleteProfile(...) works correctly.
TEST_F(ProfileDeleterIOSTest, DeleteProfile) {
  const base::Uuid profile_uuid = base::Uuid::GenerateRandomV4();
  const std::string profile_name = profile_uuid.AsLowercaseString();
  const base::FilePath profile_dir = storage_dir().Append(profile_name);

  CreateProfileStorage(profile_name, base::Uuid());
  ASSERT_TRUE(base::DirectoryExists(profile_dir));

  const auto result = DeleteProfile(profile_name);

  // The profile data should have been deleted and the success reported.
  EXPECT_EQ(result, ProfileDeleterIOS::Result::kSuccess);
  EXPECT_FALSE(base::DirectoryExists(profile_dir));
}

// Tests that DeleteProfile(...) works correctly even if the profile has
// never been created.
TEST_F(ProfileDeleterIOSTest, DeleteProfile_NoData) {
  const base::Uuid profile_uuid = base::Uuid::GenerateRandomV4();
  const std::string profile_name = profile_uuid.AsLowercaseString();
  const base::FilePath profile_dir = storage_dir().Append(profile_name);

  ASSERT_FALSE(base::DirectoryExists(profile_dir));

  const auto result = DeleteProfile(profile_name);

  // The operation should be a success, and the directory should not have
  // been created.
  EXPECT_EQ(result, ProfileDeleterIOS::Result::kSuccess);
  EXPECT_FALSE(base::DirectoryExists(profile_dir));
}

// Tests that DeleteProfile(...) works correctly even if there is no
// known WebKit storage identifier (corresponding to default storage).
TEST_F(ProfileDeleterIOSTest, DeleteProfile_DefaultStorage) {
  const base::Uuid profile_uuid;
  const std::string profile_name = "Default";
  const base::FilePath profile_dir = storage_dir().Append(profile_name);

  CreateProfileStorage(profile_name, profile_uuid);
  ASSERT_TRUE(base::DirectoryExists(profile_dir));

  const auto result = DeleteProfile(profile_name);

  // The profile data should have been deleted and the success reported.
  EXPECT_EQ(result, ProfileDeleterIOS::Result::kSuccess);
  EXPECT_FALSE(base::DirectoryExists(profile_dir));
}

// Tests that DeleteProfile(...) works correctly even if the WebKit storage
// identifier is stored in the prefences.
TEST_F(ProfileDeleterIOSTest, DeleteProfile_WebKitStorageIdInPrefs) {
  const base::Uuid profile_uuid = base::Uuid::GenerateRandomV4();
  const std::string profile_name = "Default";
  const base::FilePath profile_dir = storage_dir().Append(profile_name);

  CreateProfileStorage(profile_name, profile_uuid);
  ASSERT_TRUE(base::DirectoryExists(profile_dir));

  const auto result = DeleteProfile(profile_name);

  // The profile data should have been deleted and the success reported.
  EXPECT_EQ(result, ProfileDeleterIOS::Result::kSuccess);
  EXPECT_FALSE(base::DirectoryExists(profile_dir));
}
