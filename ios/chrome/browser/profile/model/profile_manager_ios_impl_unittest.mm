// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_manager_ios_impl.h"

#import "base/containers/contains.h"
#import "base/scoped_observation.h"
#import "base/test/test_file_util.h"
#import "base/threading/thread_restrictions.h"
#import "base/uuid.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "ios/chrome/browser/optimization_guide/model/ios_chrome_prediction_model_store.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Profile names.
const char kProfileName1[] = "Profile1";
const char kProfileName2[] = "Profile2";

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

class ProfileManagerIOSImplTest : public PlatformTest {
 public:
  ProfileManagerIOSImplTest()
      : profile_manager_(GetApplicationContext()->GetLocalState(),
                         base::CreateUniqueTempDirectoryScopedToTest()) {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    // IOSChromeIOThread needs to be created before the IO thread is started.
    // Thus DELAY_IO_THREAD_START is set in WebTaskEnvironment's options. The
    // thread is then started after the creation of IOSChromeIOThread.
    chrome_io_ = std::make_unique<IOSChromeIOThread>(
        application_context->GetLocalState(), application_context->GetNetLog());

    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        application_context->GetSystemIdentityManager(), &profile_manager_);

    // Register the objects with the TestingApplicationContext.
    application_context->SetIOSChromeIOThread(chrome_io_.get());
    application_context->SetProfileManagerAndAccountProfileMapper(
        &profile_manager_, account_profile_mapper_.get());

    // Initialize the prediction model store (required by some KeyedServices).
    optimization_guide::IOSChromePredictionModelStore::GetInstance()
        ->Initialize(base::CreateUniqueTempDirectoryScopedToTest());

    // Start the IO thread.
    web_task_environment_.StartIOThread();

    // Post a task to initialize the IOSChromeIOThread object on the IO thread.
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&IOSChromeIOThread::InitOnIO,
                                  base::Unretained(chrome_io_.get())));

    // Init the BrowserPolicyConnect as this is required to create an instance
    // of ProfileIOSImpl.
    application_context->GetBrowserPolicyConnector()->Init(
        application_context->GetLocalState(),
        application_context->GetSharedURLLoaderFactory());

    // IOSChromeIOThread requires the SystemURLRequestContextGetter() to be
    // created before the object is shutdown, so force its creation here.
    std::ignore = chrome_io_->system_url_request_context_getter();
  }

  ~ProfileManagerIOSImplTest() override {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    // Cleanup the prediction model store (since it is a singleton).
    optimization_guide::IOSChromePredictionModelStore::GetInstance()
        ->ResetForTesting();

    // The profiles must be unloaded before the AccountProfileMapper gets
    // unregistered from the ApplicationContext, because keyed services may
    // depend on the AccountProfileMapper.
    profile_manager_.UnloadAllProfiles();

    application_context->GetBrowserPolicyConnector()->Shutdown();
    application_context->GetIOSChromeIOThread()->NetworkTearDown();
    application_context->SetProfileManagerAndAccountProfileMapper(nullptr,
                                                                  nullptr);
    application_context->SetIOSChromeIOThread(nullptr);
  }

  ProfileManagerIOSImpl& profile_manager() { return profile_manager_; }

  ProfileAttributesStorageIOS& attributes_storage() {
    return *profile_manager_.GetProfileAttributesStorage();
  }

  // Returns the name of the loaded Profiles.
  std::set<std::string> GetLoadedProfileNames() {
    std::set<std::string> profile_names;
    for (ProfileIOS* profile : profile_manager_.GetLoadedProfiles()) {
      CHECK(profile);

      const std::string& profile_name = profile->GetProfileName();

      CHECK(!base::Contains(profile_names, profile_name));
      profile_names.insert(profile_name);
    }
    return profile_names;
  }

 private:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<IOSChromeIOThread> chrome_io_;
  web::WebTaskEnvironment web_task_environment_{
      web::WebTaskEnvironment::IOThreadType::REAL_THREAD_DELAYED};
  ProfileManagerIOSImpl profile_manager_;
  std::unique_ptr<AccountProfileMapper> account_profile_mapper_;

  // Some KeyedService requires a VariationsIdsProvider to be installed.
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
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
