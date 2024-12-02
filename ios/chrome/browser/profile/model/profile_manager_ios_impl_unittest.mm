// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_manager_ios_impl.h"

#import "base/containers/contains.h"
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
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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

  bool on_profile_marked_for_permanent_deletation_called() const {
    return on_profile_marked_for_permanent_deletation_called_;
  }

  // ProfileManagerObserverIOS implementation:
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
    on_profile_marked_for_permanent_deletation_called_ = true;
  }

 private:
  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      scoped_observation_{this};

  bool on_profile_created_called_ = false;
  bool on_profile_loaded_called_ = false;
  bool on_profile_unloaded_called_ = false;
  bool on_profile_marked_for_permanent_deletation_called_ = false;
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
  return base::BindOnce([](T* output, T value) { *output = value; }, output);
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
  // Pretends that a Profile named `kProfileName1` exists. Required as
  // LoadProfileAsync(...) won't create new Profiles.
  attributes_storage().AddProfile(kProfileName1);
  attributes_storage().UpdateAttributesForProfileWithName(
      kProfileName1, base::BindOnce([](ProfileAttributesIOS attrs) {
        attrs.ClearIsNewProfile();
        return attrs;
      }));

  base::RunLoop run_loop;
  ProfileIOS* created_profile = nullptr;
  ProfileIOS* loaded_profile = nullptr;

  // Load the Profile asynchronously while disallowing blocking on the/ current
  // sequence (to ensure that the method is really asynchronous and does not
  // block the sequence).
  {
    base::ScopedDisallowBlocking disallow_blocking;
    const bool success = profile_manager().LoadProfileAsync(
        kProfileName1,
        CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
        CaptureParam(&created_profile));

    ASSERT_TRUE(success);
  }

  // The Profile instance should have been created but not yet fully initialized
  // (as the initialisation is asynchronous).
  EXPECT_TRUE(created_profile);
  EXPECT_FALSE(loaded_profile);

  run_loop.Run();

  // The Profile should have been successfully loaded and initialized.
  EXPECT_TRUE(created_profile);
  EXPECT_TRUE(loaded_profile);

  // The two callbacks were invoked with the same object.
  EXPECT_EQ(created_profile, loaded_profile);
}

// Tests that calls LoadProfileAsync(...) on a loaded Profile return the Profile
// immediately and still don't block the main thread.
TEST_F(ProfileManagerIOSImplTest, LoadProfileAsync_Reload) {
  // Pretends that a Profile named `kProfileName1` exists. Required as
  // LoadProfileAsync(...) won't create new Profiles.
  attributes_storage().AddProfile(kProfileName1);
  attributes_storage().UpdateAttributesForProfileWithName(
      kProfileName1, base::BindOnce([](ProfileAttributesIOS attrs) {
        attrs.ClearIsNewProfile();
        return attrs;
      }));

  // Load the Profile a first time.
  {
    base::RunLoop run_loop;
    ProfileIOS* created_profile = nullptr;
    ProfileIOS* loaded_profile = nullptr;

    // Load the Profile asynchronously while disallowing blocking on the current
    // sequence (to ensure that the method is really asynchronous and does not
    // block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = profile_manager().LoadProfileAsync(
          kProfileName1,
          CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
          CaptureParam(&created_profile));

      ASSERT_TRUE(success);
    }

    // The Profile instance should have been created but not yet fully
    // initialized (as the initialisation is asynchronous).
    EXPECT_TRUE(created_profile);
    EXPECT_FALSE(loaded_profile);

    run_loop.Run();

    // The Profile should have been successfully loaded and initialized.
    EXPECT_TRUE(created_profile);
    EXPECT_TRUE(loaded_profile);

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_profile, loaded_profile);
  }

  // Load the Profile a second time. Since it is already loaded, the callback
  // should be called synchronously and successfully.
  {
    base::RunLoop run_loop;
    ProfileIOS* created_profile = nullptr;
    ProfileIOS* loaded_profile = nullptr;

    // Load the Profile asynchronously while disallowing blocking on the current
    // sequence (to ensure that the method is really asynchronous and does not
    // block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = profile_manager().LoadProfileAsync(
          kProfileName1,
          CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
          CaptureParam(&created_profile));

      ASSERT_TRUE(success);
    }

    // Since the Profile has already been loaded, both callback should be
    // invoked synchronously.
    EXPECT_TRUE(created_profile);
    EXPECT_TRUE(loaded_profile);

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_profile, loaded_profile);

    run_loop.Run();
  }
}

// Tests that LoadProfileAsync(...) fails to load an unknown Profile.
TEST_F(ProfileManagerIOSImplTest, LoadProfileAsync_Missing) {
  // Ensures that no Profile named `kProfileName1` exists. This will cause
  // LoadProfileAsync(...) to fail since it does not create new Profiles.
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName1));

  base::RunLoop run_loop;
  ProfileIOS* created_profile = nullptr;
  ProfileIOS* loaded_profile = nullptr;

  // Load the Profile asynchronously while disallowing blocking on the current
  // sequence (to ensure that the method is really asynchronous and does not
  // block the sequence).
  {
    base::ScopedDisallowBlocking disallow_blocking;
    const bool success = profile_manager().LoadProfileAsync(
        kProfileName1,
        CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
        CaptureParam(&created_profile));

    ASSERT_FALSE(success);
  }

  run_loop.Run();

  // The Profile was not loaded nor created.
  EXPECT_FALSE(created_profile);
  EXPECT_FALSE(loaded_profile);
}

// Tests that CreateProfileAsync(...) creates and load successfully a new
// Profile.
TEST_F(ProfileManagerIOSImplTest, CreateProfileAsync) {
  // Ensures that no Profile named `kProfileName1` exists. This will cause
  // CreateProfileAsync(...) to create a new Profile.
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName1));

  base::RunLoop run_loop;
  ProfileIOS* created_profile = nullptr;
  ProfileIOS* loaded_profile = nullptr;

  // Load the Profile asynchronously while disallowing blocking on the current
  // sequence (to ensure that the method is really asynchronous and does not
  // block the sequence).
  {
    base::ScopedDisallowBlocking disallow_blocking;
    const bool success = profile_manager().CreateProfileAsync(
        kProfileName1,
        CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
        CaptureParam(&created_profile));

    ASSERT_TRUE(success);
  }

  // The Profile instance should have been created but not yet fully initialized
  // (as the initialisation is asynchronous).
  EXPECT_TRUE(created_profile);
  EXPECT_FALSE(loaded_profile);

  run_loop.Run();

  // The Profile should have been successfully loaded and initialized.
  EXPECT_TRUE(created_profile);
  EXPECT_TRUE(loaded_profile);

  // The two callbacks were invoked with the same object.
  EXPECT_EQ(created_profile, loaded_profile);
}

// Tests that calling CreateProfileAsync(...) a second time returns the Profile
// that has already been laoded.
TEST_F(ProfileManagerIOSImplTest, CreateProfileAsync_Reload) {
  // Ensures that no Profile named `kProfileName1` exists. This will cause
  // CreateProfileAsync(...) to create a new Profile.
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName1));

  // Load the Profile a first time.
  {
    base::RunLoop run_loop;
    ProfileIOS* created_profile = nullptr;
    ProfileIOS* loaded_profile = nullptr;

    // Load the Profile asynchronously while disallowing blocking on the current
    // sequence (to ensure that the method is really asynchronous and does not
    // block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = profile_manager().CreateProfileAsync(
          kProfileName1,
          CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
          CaptureParam(&created_profile));

      ASSERT_TRUE(success);
    }

    // The Profile instance should have been created but not yet fully
    // initialized (as the initialisation is asynchronous).
    EXPECT_TRUE(created_profile);
    EXPECT_FALSE(loaded_profile);

    run_loop.Run();

    // The Profile should have been successfully loaded and initialized.
    EXPECT_TRUE(created_profile);
    EXPECT_TRUE(loaded_profile);

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_profile, loaded_profile);
  }

  // Load the Profile a second time. Since it is already loaded, the callback
  // should be called synchronously and successfully.
  {
    base::RunLoop run_loop;
    ProfileIOS* created_profile = nullptr;
    ProfileIOS* loaded_profile = nullptr;

    // Load the Profile asynchronously while disallowing blocking on the current
    // sequence (to ensure that the method is really asynchronous and does not
    // block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = profile_manager().CreateProfileAsync(
          kProfileName1,
          CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
          CaptureParam(&created_profile));

      ASSERT_TRUE(success);
    }

    // Since the Profile has already been loaded, both callback should be
    // invoked synchronously.
    EXPECT_TRUE(created_profile);
    EXPECT_TRUE(loaded_profile);

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_profile, loaded_profile);

    run_loop.Run();
  }
}

// Tests that LoadProfile(...) correctly loads a known Profile in a synchronous
// fashion (i.e. blocks the main thread).
TEST_F(ProfileManagerIOSImplTest, LoadProfile) {
  // Pretends that a Profile named `kProfileName1` exists. Required as
  // LoadProfile(...) won't create new Profiles.
  attributes_storage().AddProfile(kProfileName1);
  attributes_storage().UpdateAttributesForProfileWithName(
      kProfileName1, base::BindOnce([](ProfileAttributesIOS attrs) {
        attrs.ClearIsNewProfile();
        return attrs;
      }));

  // Load the Profile synchronously.
  ProfileIOS* profile = profile_manager().LoadProfile(kProfileName1);

  // The Profile should have been successfully loaded and initialized.
  EXPECT_TRUE(profile);

  // Calling LoadProfile(...) a second time should return the same
  // object.
  EXPECT_EQ(profile, profile_manager().LoadProfile(kProfileName1));
}

// Tests that LoadProfile(...) fails to load an unknown Profile.
TEST_F(ProfileManagerIOSImplTest, LoadProfile_Missing) {
  // Ensures that no Profile named `kProfileName1` exists. This will cause
  // LoadProfile(...) to fail since it does not create new Profiles.
  ASSERT_FALSE(attributes_storage().HasProfileWithName(kProfileName1));

  // Load the Profile synchronously.
  ProfileIOS* profile = profile_manager().LoadProfile(kProfileName1);

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
  ProfileIOS* profile = profile_manager().CreateProfile(kProfileName1);

  // The Profile should have been successfully loaded and initialized.
  EXPECT_TRUE(profile);

  // Calling CreateProfile(...) a second time should return the same
  // object.
  EXPECT_EQ(profile, profile_manager().CreateProfile(kProfileName1));
}

// Check that if there are not profile marked as the personal profile, then
// the first profile created is marked as the personal profile.
TEST_F(ProfileManagerIOSImplTest, FirstProfileCreatedMarkedAsPersonalProfile) {
  ASSERT_TRUE(GetLoadedProfileNames().empty());
  ASSERT_TRUE(attributes_storage().GetPersonalProfileName().empty());

  // Create a profile. It should be marked as the personal profile.
  EXPECT_TRUE(profile_manager().CreateProfile(kProfileName1));

  // The profile should've been marked as the personal profile.
  EXPECT_EQ(attributes_storage().GetPersonalProfileName(), kProfileName1);
}

// Check that if there is a profile marked as the personal profile, creating
// a new profile does not overwrite the personal profile.
TEST_F(ProfileManagerIOSImplTest, CreatingProfileDontOverwritePersonalProfile) {
  ASSERT_TRUE(GetLoadedProfileNames().empty());
  ASSERT_TRUE(attributes_storage().GetPersonalProfileName().empty());

  // Mark kProfileName1 as the personal profile.
  attributes_storage().AddProfile(kProfileName1);
  attributes_storage().SetPersonalProfileName(kProfileName1);
  EXPECT_EQ(attributes_storage().GetPersonalProfileName(), kProfileName1);

  // Create another profile, this should not change the personal profile.
  EXPECT_TRUE(profile_manager().CreateProfile(kProfileName2));

  // The personal profile should not have been changed.
  EXPECT_EQ(attributes_storage().GetPersonalProfileName(), kProfileName1);
}

// Tests that unloading a profile invoke OnProfileUnloaded(...) on the
// observers.
TEST_F(ProfileManagerIOSImplTest, UnloadProfile) {
  // Create a few profiles synchronously.
  ASSERT_TRUE(profile_manager().CreateProfile(kProfileName1));
  ASSERT_TRUE(profile_manager().CreateProfile(kProfileName2));

  ScopedTestProfileManagerObserverIOS observer(profile_manager());
  EXPECT_FALSE(observer.on_profile_unloaded_called());

  // Check that the profiles are accessible.
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));

  // Unload a profile, it should not longer be accessible and the
  // observer must have been notified of that.
  profile_manager().UnloadProfile(kProfileName1);

  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));
  EXPECT_TRUE(observer.on_profile_unloaded_called());
}

// Tests that unloading all profiles invoke OnProfileUnloaded(...) on the
// observers.
TEST_F(ProfileManagerIOSImplTest, UnloadAllProfiles) {
  // Create a few profiles synchronously.
  ASSERT_TRUE(profile_manager().CreateProfile(kProfileName1));
  ASSERT_TRUE(profile_manager().CreateProfile(kProfileName2));

  ScopedTestProfileManagerObserverIOS observer(profile_manager());
  EXPECT_FALSE(observer.on_profile_unloaded_called());

  // Check that the profiles are accessible.
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_TRUE(profile_manager().GetProfileWithName(kProfileName2));

  // Unload all profiles, they should not longer be accessible and the
  // observer must have been notified of that.
  profile_manager().UnloadAllProfiles();

  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName1));
  EXPECT_FALSE(profile_manager().GetProfileWithName(kProfileName2));
  EXPECT_TRUE(observer.on_profile_unloaded_called());
}

// Tests that OnProfileUnloaded(...) is not called if a profile in unloaded
// while still loading.
TEST_F(ProfileManagerIOSImplTest, UnloadAllProfiles_LoadPending) {
  // Load a profile asynchronously.
  base::RunLoop run_loop;
  ProfileIOS* loaded_profile = nullptr;
  ProfileIOS* created_profile = nullptr;
  const bool success = profile_manager().CreateProfileAsync(
      kProfileName1, CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
      CaptureParam(&created_profile));

  EXPECT_TRUE(created_profile);
  EXPECT_TRUE(success);

  ScopedTestProfileManagerObserverIOS observer(profile_manager());
  EXPECT_FALSE(observer.on_profile_unloaded_called());

  // Unload all profiles. The profile whose load is pending should no longer
  // be loading, and the load should be considered as failed.
  profile_manager().UnloadAllProfiles();

  // The callback should be called from UnloadAllProfiles(), so the RunLoop
  // should be considered as having quit called, and thus Run() should return
  // immediately.
  run_loop.Run();

  EXPECT_FALSE(observer.on_profile_unloaded_called());
  EXPECT_FALSE(loaded_profile);
}

// Tests that ReserveNewProfileName(...) returns a new profile name that is
// randomly generated, and register it with ProfileAttributesStorageIOS. The
// name must also be a valid UUID.
TEST_F(ProfileManagerIOSImplTest, ReserveNewProfileName) {
  ASSERT_EQ(attributes_storage().GetNumberOfProfiles(), 0u);

  const std::string name = profile_manager().ReserveNewProfileName();
  EXPECT_FALSE(name.empty());

  const base::Uuid uuid = base::Uuid::ParseLowercase(name);
  EXPECT_TRUE(uuid.is_valid());

  ASSERT_TRUE(attributes_storage().HasProfileWithName(name));
  ProfileAttributesIOS attrs =
      attributes_storage().GetAttributesForProfileWithName(name);
  EXPECT_TRUE(attrs.IsNewProfile());
}
