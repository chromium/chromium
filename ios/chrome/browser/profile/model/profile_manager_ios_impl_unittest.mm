// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_manager_ios_impl.h"

#import "base/containers/contains.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/scoped_observation.h"
#import "base/threading/thread_restrictions.h"
#import "base/uuid.h"
#import "ios/chrome/browser/profile/model/test_with_profile.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"
#import "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

// Profile names.
const char kProfileName1[] = "be7293b8-6309-436a-a5ab-37742a6e1a3a";
const char kProfileName2[] = "8f5d037a-44ff-4e80-bfa3-9bd507a258e2";

// A scoped ProfileManagerObserverIOS which records which events have been
// received.
class ScopedTestProfileManagerObserverIOS final
    : public ProfileManagerObserverIOS {
 public:
  explicit ScopedTestProfileManagerObserverIOS(ProfileManagerIOS& manager) {
    scoped_observation_.Observe(&manager);
  }

  ~ScopedTestProfileManagerObserverIOS() final = default;

  // Accessor for the booleans used to store which method has been called.
  bool on_profile_created_called() const { return on_profile_created_called_; }

  bool on_profile_loaded_called() const { return on_profile_loaded_called_; }

  bool on_profile_unloaded_called() const {
    return on_profile_unloaded_called_;
  }

  bool on_profile_marked_for_permanent_deletion_called() const {
    return on_profile_marked_for_permanent_deletion_called_;
  }

  // ProfileManagerObserverIOS implementation:
  void OnProfileManagerWillBeDestroyed(ProfileManagerIOS* manager) final {
    // Nothing to do.
  }

  void OnProfileManagerDestroyed(ProfileManagerIOS* manager) final {
    DCHECK(scoped_observation_.IsObservingSource(manager));
    scoped_observation_.Reset();
  }

  void OnProfileCreated(ProfileManagerIOS* manager, ProfileIOS* profile) final {
    DCHECK(scoped_observation_.IsObservingSource(manager));
    on_profile_created_called_ = true;
  }

  void OnProfileLoaded(ProfileManagerIOS* manager, ProfileIOS* profile) final {
    DCHECK(scoped_observation_.IsObservingSource(manager));
    on_profile_loaded_called_ = true;
  }

  void OnProfileUnloaded(ProfileManagerIOS* manager,
                         ProfileIOS* profile) final {
    DCHECK(scoped_observation_.IsObservingSource(manager));
    on_profile_unloaded_called_ = true;
  }

  void OnProfileMarkedForPermanentDeletion(ProfileManagerIOS* manager,
                                           ProfileIOS* profile) final {
    DCHECK(scoped_observation_.IsObservingSource(manager));
    on_profile_marked_for_permanent_deletion_called_ = true;
  }

 private:
  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      scoped_observation_{this};

  bool on_profile_created_called_ = false;
  bool on_profile_loaded_called_ = false;
  bool on_profile_unloaded_called_ = false;
  bool on_profile_marked_for_permanent_deletion_called_ = false;
};

// Returns a callback that fail the current test if invoked.
template <typename... Args>
base::OnceCallback<void(Args...)> FailCallback() {
  return base::BindOnce([](Args...) { GTEST_FAIL(); });
}

// Returns a callback taking a single parameter and storing it in `output`.
// The `output` must outlive the returned callback as it is captured by copy.
template <typename T>
base::OnceCallback<void(T)> CaptureParam(T* output) {
  return base::BindOnce([](T* output, T value) { *output = std::move(value); },
                        output);
}

}  // namespace

class ProfileManagerIOSImplTest : public TestWithProfile {
 public:
  // Returns the name of the loaded Profiles.
  std::set<std::string> GetLoadedProfileNames() {
    std::set<std::string> profile_names;
    for (ProfileIOS* profile : profile_manager().GetLoadedProfiles()) {
      CHECK(profile);

      const std::string& profile_name = profile->GetProfileName();

      CHECK(!base::Contains(profile_names, profile_name));
      profile_names.insert(profile_name);
    }
    return profile_names;
  }
};

// Tests that LoadProfileAsync(...) correctly loads a known Profile, and that
// the load is not blocking the main thread.
TEST_F(ProfileManagerIOSImplTest, LoadProfileAsync) {
  // Reserve a new profile name and mark it as existing.
  const std::string profile_name = profile_manager().ReserveNewProfileName();
  attributes_storage().UpdateAttributesForProfileWithName(
      profile_name, base::BindOnce([](ProfileAttributesIOS& attrs) {
        attrs.ClearIsNewProfile();
      }));

  base::RunLoop run_loop;
  ScopedProfileKeepAliveIOS created_keep_alive;
  ScopedProfileKeepAliveIOS loaded_keep_alive;

  // Load the Profile asynchronously while disallowing blocking on the current
  // sequence (to ensure that the method is really asynchronous and does not
  // block the sequence).
  {
    base::ScopedDisallowBlocking disallow_blocking;
    const bool success = profile_manager().LoadProfileAsync(
        profile_name,
        CaptureParam(&loaded_keep_alive).Then(run_loop.QuitClosure()),
        CaptureParam(&created_keep_alive));

    ASSERT_TRUE(success);
  }

  // The Profile instance should have been created but not yet fully initialized
  // (as the initialization is asynchronous).
  EXPECT_TRUE(created_keep_alive.profile());
  EXPECT_FALSE(loaded_keep_alive.profile());

  run_loop.Run();

  // The Profile should have been successfully loaded and initialized.
  EXPECT_TRUE(created_keep_alive.profile());
  EXPECT_TRUE(loaded_keep_alive.profile());

  // The two callbacks were invoked with the same object.
  EXPECT_EQ(created_keep_alive.profile(), loaded_keep_alive.profile());
}

// Tests that calls LoadProfileAsync(...) on a loaded Profile return the Profile
// immediately and still don't block the main thread.
TEST_F(ProfileManagerIOSImplTest, LoadProfileAsync_Reload) {
  // Reserve a new profile name and mark it as existing.
  const std::string profile_name = profile_manager().ReserveNewProfileName();
  attributes_storage().UpdateAttributesForProfileWithName(
      profile_name, base::BindOnce([](ProfileAttributesIOS& attrs) {
        attrs.ClearIsNewProfile();
      }));

  // Ensure the profile is not deleted between the two blocks when the
  // ScopedProfileKeepAliveIOS is dropped.
  ScopedProfileKeepAliveIOS keep_alive;

  // Load the Profile a first time.
  {
    base::RunLoop run_loop;
    ScopedProfileKeepAliveIOS created_keep_alive;
    ScopedProfileKeepAliveIOS loaded_keep_alive;

    // Load the Profile asynchronously while disallowing blocking on the current
    // sequence (to ensure that the method is really asynchronous and does not
    // block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = profile_manager().LoadProfileAsync(
          profile_name,
          CaptureParam(&loaded_keep_alive).Then(run_loop.QuitClosure()),
          CaptureParam(&created_keep_alive));

      ASSERT_TRUE(success);
    }

    // The Profile instance should have been created but not yet fully
    // initialized (as the initialization is asynchronous).
    EXPECT_TRUE(created_keep_alive.profile());
    EXPECT_FALSE(loaded_keep_alive.profile());

    run_loop.Run();

    // The Profile should have been successfully loaded and initialized.
    EXPECT_TRUE(created_keep_alive.profile());
    EXPECT_TRUE(loaded_keep_alive.profile());

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_keep_alive.profile(), loaded_keep_alive.profile());

    keep_alive = std::move(loaded_keep_alive);
  }

  // Load the Profile a second time. Since it is already loaded, the callback
  // should be called synchronously and successfully.
  {
    base::RunLoop run_loop;
    ScopedProfileKeepAliveIOS created_keep_alive;
    ScopedProfileKeepAliveIOS loaded_keep_alive;

    // Load the Profile asynchronously while disallowing blocking on the current
    // sequence (to ensure that the method is really asynchronous and does not
    // block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = profile_manager().LoadProfileAsync(
          profile_name,
          CaptureParam(&loaded_keep_alive).Then(run_loop.QuitClosure()),
          CaptureParam(&created_keep_alive));

      ASSERT_TRUE(success);
    }

    // Since the Profile has already been loaded, both callback should be
    // invoked synchronously.
    EXPECT_TRUE(created_keep_alive.profile());
    EXPECT_TRUE(loaded_keep_alive.profile());

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_keep_alive.profile(), loaded_keep_alive.profile());

    run_loop.Run();
  }
}

// Tests that LoadProfileAsync(...) fails to load an unknown Profile.
TEST_F(ProfileManagerIOSImplTest, LoadProfileAsync_Missing) {
  // Ensures that no Profile named `kProfileName1` exists. This will cause
  // LoadProfileAsync(...) to fail since it does not create new Profiles.
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName1));

  base::RunLoop run_loop;
  ScopedProfileKeepAliveIOS created_keep_alive;
  ScopedProfileKeepAliveIOS loaded_keep_alive;

  // Load the Profile asynchronously while disallowing blocking on the current
  // sequence (to ensure that the method is really asynchronous and does not
  // block the sequence).
  {
    base::ScopedDisallowBlocking disallow_blocking;
    const bool success = profile_manager().LoadProfileAsync(
        kProfileName1,
        CaptureParam(&loaded_keep_alive).Then(run_loop.QuitClosure()),
        CaptureParam(&created_keep_alive));

    ASSERT_FALSE(success);
  }

  run_loop.Run();

  // The Profile was not loaded nor created.
  EXPECT_FALSE(created_keep_alive.profile());
  EXPECT_FALSE(loaded_keep_alive.profile());
}

// Tests that CreateProfileAsync(...) creates and load successfully a new
// Profile.
TEST_F(ProfileManagerIOSImplTest, CreateProfileAsync) {
  // Ensures that no Profile named `kProfileName1` exists. This will cause
  // CreateProfileAsync(...) to create a new Profile.
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName1));

  base::RunLoop run_loop;
  ScopedProfileKeepAliveIOS created_keep_alive;
  ScopedProfileKeepAliveIOS loaded_keep_alive;

  // Load the Profile asynchronously while disallowing blocking on the current
  // sequence (to ensure that the method is really asynchronous and does not
  // block the sequence).
  {
    base::ScopedDisallowBlocking disallow_blocking;
    const bool success = profile_manager().CreateProfileAsync(
        kProfileName1,
        CaptureParam(&loaded_keep_alive).Then(run_loop.QuitClosure()),
        CaptureParam(&created_keep_alive));

    ASSERT_TRUE(success);
  }

  // The Profile instance should have been created but not yet fully initialized
  // (as the initialization is asynchronous).
  EXPECT_TRUE(created_keep_alive.profile());
  EXPECT_FALSE(loaded_keep_alive.profile());

  run_loop.Run();

  // The Profile should have been successfully loaded and initialized.
  EXPECT_TRUE(created_keep_alive.profile());
  EXPECT_TRUE(loaded_keep_alive.profile());

  // The two callbacks were invoked with the same object.
  EXPECT_EQ(created_keep_alive.profile(), loaded_keep_alive.profile());
}

// Tests that Profile marked for deletion does not create a new profile.
TEST_F(ProfileManagerIOSImplTest, CreateProfile_MarkedForDeletion) {
  // Create a few profiles synchronously.
  ScopedProfileKeepAliveIOS keep_alive1 = CreateProfile(kProfileName1);
  ScopedProfileKeepAliveIOS keep_alive2 = CreateProfile(kProfileName2);
  ASSERT_TRUE(keep_alive1.profile());
  ASSERT_TRUE(keep_alive2.profile());

  // Check that the profiles are accessible.
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));

  profile_manager().MarkProfileForDeletion(kProfileName2);
  keep_alive2.Reset();

  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName2));

  // Ensures that the profile cannot be created and has been removed from
  // the profile attributes storage.
  keep_alive2 = CreateProfile(kProfileName2);
  ASSERT_FALSE(keep_alive2.profile());
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName2));
}

// Tests that calling CreateProfileAsync(...) a second time returns the Profile
// that has already been laoded.
TEST_F(ProfileManagerIOSImplTest, CreateProfileAsync_Reload) {
  // Ensures that no Profile named `kProfileName1` exists. This will cause
  // CreateProfileAsync(...) to create a new Profile.
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName1));

  // Ensure the profile is not deleted between the two blocks when the
  // ScopedProfileKeepAliveIOS is dropped.
  ScopedProfileKeepAliveIOS keep_alive;

  // Load the Profile a first time.
  {
    base::RunLoop run_loop;
    ScopedProfileKeepAliveIOS created_keep_alive;
    ScopedProfileKeepAliveIOS loaded_keep_alive;

    // Load the Profile asynchronously while disallowing blocking on the current
    // sequence (to ensure that the method is really asynchronous and does not
    // block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = profile_manager().CreateProfileAsync(
          kProfileName1,
          CaptureParam(&loaded_keep_alive).Then(run_loop.QuitClosure()),
          CaptureParam(&created_keep_alive));

      ASSERT_TRUE(success);
    }

    // The Profile instance should have been created but not yet fully
    // initialized (as the initialization is asynchronous).
    EXPECT_TRUE(created_keep_alive.profile());
    EXPECT_FALSE(loaded_keep_alive.profile());

    run_loop.Run();

    // The Profile should have been successfully loaded and initialized.
    EXPECT_TRUE(created_keep_alive.profile());
    EXPECT_TRUE(loaded_keep_alive.profile());

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_keep_alive.profile(), loaded_keep_alive.profile());

    keep_alive = std::move(loaded_keep_alive);
  }

  // Load the Profile a second time. Since it is already loaded, the callback
  // should be called synchronously and successfully.
  {
    base::RunLoop run_loop;
    ScopedProfileKeepAliveIOS created_keep_alive;
    ScopedProfileKeepAliveIOS loaded_keep_alive;

    // Load the Profile asynchronously while disallowing blocking on the current
    // sequence (to ensure that the method is really asynchronous and does not
    // block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = profile_manager().CreateProfileAsync(
          kProfileName1,
          CaptureParam(&loaded_keep_alive).Then(run_loop.QuitClosure()),
          CaptureParam(&created_keep_alive));

      ASSERT_TRUE(success);
    }

    // Since the Profile has already been loaded, both callback should be
    // invoked synchronously.
    EXPECT_TRUE(created_keep_alive.profile());
    EXPECT_TRUE(loaded_keep_alive.profile());

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_keep_alive.profile(), loaded_keep_alive.profile());

    run_loop.Run();
  }
}

// Tests that LoadProfile(...) correctly loads a known Profile in a synchronous
// fashion (i.e. blocks the main thread).
TEST_F(ProfileManagerIOSImplTest, LoadProfile) {
  // Reserve a new profile name and mark it as existing.
  const std::string profile_name = profile_manager().ReserveNewProfileName();
  attributes_storage().UpdateAttributesForProfileWithName(
      profile_name, base::BindOnce([](ProfileAttributesIOS& attrs) {
        attrs.ClearIsNewProfile();
      }));

  // Load the Profile synchronously.
  ScopedProfileKeepAliveIOS keep_alive = LoadProfile(profile_name);
  ProfileIOS* profile = keep_alive.profile();

  // The Profile should have been successfully loaded and initialized.
  EXPECT_TRUE(profile);

  // Calling LoadProfile(...) a second time should return the same
  // object.
  EXPECT_EQ(profile, LoadProfile(profile_name).profile());
}

// Tests that LoadProfile(...) fails to load an unknown Profile.
TEST_F(ProfileManagerIOSImplTest, LoadProfile_Missing) {
  // Ensures that no Profile named `kProfileName1` exists. This will cause
  // LoadProfile(...) to fail since it does not create new Profiles.
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName1));

  // Load the Profile synchronously.
  ScopedProfileKeepAliveIOS keep_alive = LoadProfile(kProfileName1);
  ProfileIOS* profile = keep_alive.profile();

  // The Profile was not loaded nor created.
  EXPECT_FALSE(profile);
}

// Tests that CreateProfile(...) creates and load successfully a new Profile in
// a synchronous fashion (i.e. blocks the main thread).
TEST_F(ProfileManagerIOSImplTest, CreateProfile) {
  // Ensures that no Profile named `kProfileName1` exists. This will cause
  // CreateProfileAsync(...) to create a new Profile.
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName1));

  // Create the Profile synchronously.
  ScopedProfileKeepAliveIOS keep_alive = CreateProfile(kProfileName1);

  // The Profile should have been successfully loaded and initialized.
  EXPECT_TRUE(keep_alive.profile());

  // Calling CreateProfile(...) a second time should return the same
  // object.
  ScopedProfileKeepAliveIOS second_keep_alive = CreateProfile(kProfileName1);
  EXPECT_EQ(keep_alive.profile(), second_keep_alive.profile());
}

// Check that if there are not profile marked as the personal profile, then
// a profile is automatically created and marked as personal profile.
TEST_F(ProfileManagerIOSImplTest, PersonalProfileExists) {
  const std::string& personal_profile =
      attributes_storage().GetPersonalProfileName();
  EXPECT_TRUE(profile_manager().HasProfileWithName(personal_profile));
}

// Check that if there is a profile marked as the personal profile, creating
// a new profile does not overwrite the personal profile.
TEST_F(ProfileManagerIOSImplTest, CreatingProfileDontOverwritePersonalProfile) {
  // Record the personal profile (created by the constructor).
  const std::string profile_name1 =
      attributes_storage().GetPersonalProfileName();
  ASSERT_NE(profile_name1, std::string());

  // Create another profile, this should not change the personal profile.
  const std::string profile_name2 = profile_manager().ReserveNewProfileName();
  ScopedProfileKeepAliveIOS keep_alive = CreateProfile(profile_name2);
  EXPECT_TRUE(keep_alive.profile());

  // The personal profile should not have been changed.
  EXPECT_EQ(attributes_storage().GetPersonalProfileName(), profile_name1);
}

// Tests that unloading a profile invoke OnProfileUnloaded(...) on the
// observers.
TEST_F(ProfileManagerIOSImplTest, UnloadProfile) {
  // Create a few profiles synchronously.
  ScopedProfileKeepAliveIOS keep_alive1 = CreateProfile(kProfileName1);
  ScopedProfileKeepAliveIOS keep_alive2 = CreateProfile(kProfileName2);
  ASSERT_TRUE(keep_alive1.profile());
  ASSERT_TRUE(keep_alive2.profile());

  ScopedTestProfileManagerObserverIOS observer(profile_manager());
  EXPECT_FALSE(observer.on_profile_unloaded_called());

  // Check that the profiles are accessible.
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));

  // Unload a profile, it should not longer be accessible and the
  // observer must have been notified of that.
  keep_alive1.Reset();

  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));
  EXPECT_TRUE(observer.on_profile_unloaded_called());
}

// Tests that unloading all profiles invoke OnProfileUnloaded(...) on the
// observers.
TEST_F(ProfileManagerIOSImplTest, UnloadAllProfiles) {
  // Create a few profiles synchronously.
  ScopedProfileKeepAliveIOS keep_alive1 = CreateProfile(kProfileName1);
  ScopedProfileKeepAliveIOS keep_alive2 = CreateProfile(kProfileName2);
  ASSERT_TRUE(keep_alive1.profile());
  ASSERT_TRUE(keep_alive2.profile());

  ScopedTestProfileManagerObserverIOS observer(profile_manager());
  EXPECT_FALSE(observer.on_profile_unloaded_called());

  // Check that the profiles are accessible.
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));

  // Unload all profiles, they should not longer be accessible and the
  // observer must have been notified of that.
  keep_alive2.Reset();
  keep_alive1.Reset();

  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName2));
  EXPECT_TRUE(observer.on_profile_unloaded_called());
}

// Tests that ReserveNewProfileName(...) returns a new profile name that is
// randomly generated, and register it with ProfileAttributesStorageIOS. The
// name must also be a valid UUID.
TEST_F(ProfileManagerIOSImplTest, ReserveNewProfileName) {
  const std::string name = profile_manager().ReserveNewProfileName();
  EXPECT_FALSE(name.empty());

  const base::Uuid uuid = base::Uuid::ParseLowercase(name);
  EXPECT_TRUE(uuid.is_valid());

  ASSERT_TRUE(attributes_storage().HasProfileWithName(name));
  ProfileAttributesIOS attrs =
      attributes_storage().GetAttributesForProfileWithName(name);
  EXPECT_TRUE(attrs.IsNewProfile());
}

// Tests that marking profile for deletion invoke
// OnProfileMarkedForPermanentDeletion(...) on the observers.
TEST_F(ProfileManagerIOSImplTest, MarkProfileForDeletion) {
  // Create a few profiles synchronously.
  ScopedProfileKeepAliveIOS keep_alive1 = CreateProfile(kProfileName1);
  ScopedProfileKeepAliveIOS keep_alive2 = CreateProfile(kProfileName2);
  ASSERT_TRUE(keep_alive1.profile());
  ASSERT_TRUE(keep_alive2.profile());

  ScopedTestProfileManagerObserverIOS observer(profile_manager());
  EXPECT_FALSE(observer.on_profile_marked_for_permanent_deletion_called());
  EXPECT_FALSE(observer.on_profile_unloaded_called());

  // Check that the profiles are accessible.
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));

  // Mark profile for deletion, they should not longer be accessible and the
  // observer must have been notified of that.
  profile_manager().MarkProfileForDeletion(kProfileName2);

  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName2));
  EXPECT_FALSE(attributes_storage().HasProfileWithName(kProfileName2));
  EXPECT_TRUE(observer.on_profile_marked_for_permanent_deletion_called());
}

// Tests that marking unloaded profile for deletion does not invoke
// OnProfileMarkedForPermanentDeletion(...) on the observers.
TEST_F(ProfileManagerIOSImplTest,
       MarkProfileForDeletion_UnloadedProfileShouldNotCallObserver) {
  // Create a few profiles synchronously.
  ScopedProfileKeepAliveIOS keep_alive1 = CreateProfile(kProfileName1);
  ScopedProfileKeepAliveIOS keep_alive2 = CreateProfile(kProfileName2);
  ASSERT_TRUE(keep_alive1.profile());
  ASSERT_TRUE(keep_alive2.profile());

  ScopedTestProfileManagerObserverIOS observer(profile_manager());
  EXPECT_FALSE(observer.on_profile_marked_for_permanent_deletion_called());
  EXPECT_FALSE(observer.on_profile_unloaded_called());

  // Check that the profiles are accessible.
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));

  // Unload a profile, it should not longer be accessible and the
  // observer must have been notified of that.
  keep_alive2.Reset();

  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName2));
  EXPECT_TRUE(observer.on_profile_unloaded_called());

  // Mark unloaded profile for deletion, ensure the observer is not called.
  profile_manager().MarkProfileForDeletion(kProfileName2);

  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName2));
  EXPECT_FALSE(attributes_storage().HasProfileWithName(kProfileName2));
  EXPECT_FALSE(observer.on_profile_marked_for_permanent_deletion_called());
}

// Tests that it is not possible to create a profile with a name marked for
// deletion.
TEST_F(ProfileManagerIOSImplTest,
       MarkProfileForDeletion_CantCreateProfileWithProfileMarkedForDeletion) {
  // Create a few profiles synchronously.
  ScopedProfileKeepAliveIOS keep_alive1 = CreateProfile(kProfileName1);
  ScopedProfileKeepAliveIOS keep_alive2 = CreateProfile(kProfileName2);
  ASSERT_TRUE(keep_alive1.profile());
  ASSERT_TRUE(keep_alive2.profile());

  // Check that the profiles are accessible.
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));

  // Mark profile for deletion.
  profile_manager().MarkProfileForDeletion(kProfileName2);
  EXPECT_FALSE(profile_manager().CanCreateProfileWithName(kProfileName2));
  EXPECT_FALSE(attributes_storage().HasProfileWithName(kProfileName2));
}

// Tests that PurgeProfilesMarkedForDeletion(...) works correctly and delete
// the data for the profile from the disk.
TEST_F(ProfileManagerIOSImplTest, PurgeProfilesMarkedForDeletion) {
  // Reserve profiles without creating them, then write data on disk and
  // mark them for deletion. By not loading the profile we ensure that
  // there is no risk for the test to be flaky because WKWebsiteStoage
  // is still in use on the IO thread.
  const std::string profile1 = profile_manager().ReserveNewProfileName();
  const std::string profile2 = profile_manager().ReserveNewProfileName();
  const std::string profile3 = profile_manager().ReserveNewProfileName();

  ASSERT_TRUE(profile_manager().HasProfileWithName(profile1));
  ASSERT_TRUE(profile_manager().HasProfileWithName(profile2));
  ASSERT_TRUE(profile_manager().HasProfileWithName(profile3));

  // Create fake data for the profiles (just a directory is enough for
  // this test).
  const base::FilePath profile1_dir = profile_data_dir().Append(profile1);
  const base::FilePath profile2_dir = profile_data_dir().Append(profile2);
  const base::FilePath profile3_dir = profile_data_dir().Append(profile3);

  ASSERT_TRUE(base::CreateDirectory(profile1_dir));
  ASSERT_TRUE(base::CreateDirectory(profile2_dir));
  ASSERT_TRUE(base::CreateDirectory(profile3_dir));

  // Mark some of the profiles for deletion.
  profile_manager().MarkProfileForDeletion(profile1);
  profile_manager().MarkProfileForDeletion(profile2);

  // Check that the data has not been deleted yet.
  EXPECT_TRUE(base::DirectoryExists(profile1_dir));
  EXPECT_TRUE(base::DirectoryExists(profile2_dir));
  EXPECT_TRUE(base::DirectoryExists(profile3_dir));

  EXPECT_TRUE(profile_manager().IsProfileMarkedForDeletion(profile1));
  EXPECT_TRUE(profile_manager().IsProfileMarkedForDeletion(profile2));
  EXPECT_FALSE(profile_manager().IsProfileMarkedForDeletion(profile3));

  EXPECT_FALSE(profile_manager().HasProfileWithName(profile1));
  EXPECT_FALSE(profile_manager().HasProfileWithName(profile2));
  EXPECT_TRUE(profile_manager().HasProfileWithName(profile3));

  // Schedule the profile deletion and wait for the operation to complete.
  base::RunLoop run_loop;
  profile_manager().PurgeProfilesMarkedForDeletion(run_loop.QuitClosure());
  run_loop.Run();

  // Check that the data has been deleted, and the profiles are no longer
  // marked for deletion. The data should not be deleted for the profiles
  // not marked for deletion.
  EXPECT_FALSE(base::DirectoryExists(profile1_dir));
  EXPECT_FALSE(base::DirectoryExists(profile2_dir));
  EXPECT_TRUE(base::DirectoryExists(profile3_dir));

  EXPECT_FALSE(profile_manager().IsProfileMarkedForDeletion(profile1));
  EXPECT_FALSE(profile_manager().IsProfileMarkedForDeletion(profile2));
  EXPECT_FALSE(profile_manager().IsProfileMarkedForDeletion(profile3));

  EXPECT_FALSE(profile_manager().HasProfileWithName(profile1));
  EXPECT_FALSE(profile_manager().HasProfileWithName(profile2));
  EXPECT_TRUE(profile_manager().HasProfileWithName(profile3));
}
