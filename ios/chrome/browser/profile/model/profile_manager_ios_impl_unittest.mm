// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_manager_ios_impl.h"

#import "base/containers/contains.h"
#import "base/scoped_observation.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_file_util.h"
#import "base/threading/thread_restrictions.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "ios/chrome/browser/optimization_guide/model/ios_chrome_prediction_model_store.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"
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
const char kTestProfile1[] = "TestProfile1";
const char kTestProfile2[] = "TestProfile2";
const char kLegacyProfile[] = "LegacyProfile";

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

 private:
  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      scoped_observation_{this};

  bool on_profile_created_called_ = false;
  bool on_profile_loaded_called_ = false;
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

// State in which a feature should be.
enum class FeatureState {
  kDefault,
  kEnabled,
  kDisabled,
};

// Wrapper around a ScopedFeatureList that initialize it while putting the
// feature as either in its default state, as force-enabled or force-disabled.
// This allow to ensure the ScopedFeatureList is fully initialized before the
// threads are created (as the initialization is not thread-safe and some of
// the code running on background threads check the FeatureList).
template <FeatureState state>
class ScopedFeatureListWithState {
 public:
  ScopedFeatureListWithState(const base::Feature& feature) {
    switch (state) {
      case FeatureState::kDefault:
        scoped_feature_list_.Init();
        break;

      case FeatureState::kEnabled:
        scoped_feature_list_.InitAndEnableFeature(feature);
        break;

      case FeatureState::kDisabled:
        scoped_feature_list_.InitAndDisableFeature(feature);
        break;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

template <FeatureState state>
class ConfigurableProfileManagerIOSImplTest : public PlatformTest {
 public:
  ConfigurableProfileManagerIOSImplTest()
      : profile_manager_(GetApplicationContext()->GetLocalState(),
                         base::CreateUniqueTempDirectoryScopedToTest()) {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    // IOSChromeIOThread needs to be created before the IO thread is started.
    // Thus DELAY_IO_THREAD_START is set in WebTaskEnvironment's options. The
    // thread is then started after the creation of IOSChromeIOThread.
    chrome_io_ = std::make_unique<IOSChromeIOThread>(
        application_context->GetLocalState(), application_context->GetNetLog());

    // Register the objects with the TestingApplicationContext.
    application_context->SetIOSChromeIOThread(chrome_io_.get());
    application_context->SetProfileManager(&profile_manager_);

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

  ~ConfigurableProfileManagerIOSImplTest() override {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    // Cleanup the prediction model store (since it is a singleton).
    optimization_guide::IOSChromePredictionModelStore::GetInstance()
        ->ResetForTesting();

    application_context->GetBrowserPolicyConnector()->Shutdown();
    application_context->GetIOSChromeIOThread()->NetworkTearDown();
    application_context->SetProfileManager(nullptr);
    application_context->SetIOSChromeIOThread(nullptr);
  }

  ProfileManagerIOSImpl& profile_manager() { return profile_manager_; }

  ProfileAttributesStorageIOS& profile_attributes_storage() {
    return *profile_manager_.GetProfileAttributesStorage();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

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
  base::HistogramTester histogram_tester_;
  ScopedFeatureListWithState<state> scoped_feature_list_{kHideLegacyProfiles};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<IOSChromeIOThread> chrome_io_;
  web::WebTaskEnvironment web_task_environment_{
      web::WebTaskEnvironment::IOThreadType::REAL_THREAD_DELAYED};
  ProfileManagerIOSImpl profile_manager_;

  // Some KeyedService requires a VariationsIdsProvider to be installed.
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

// By default tests use the default state of the kHideLegacyProfiles feature.
using ProfileManagerIOSImplTest =
    ConfigurableProfileManagerIOSImplTest<FeatureState::kDefault>;

// Tests that GetLoadedProfiles() returns an empty list before the Profiles are
// loaded, and then a list containing at least one Profile, and the last used
// Profile is loaded.
TEST_F(ProfileManagerIOSImplTest, LoadProfiles) {
  // There should be no Profile loaded yet.
  EXPECT_EQ(GetLoadedProfileNames(), (std::set<std::string>{}));

  // Register an observer and check that it is correctly notified that
  // a Profile is created and then fully loaded.
  ScopedTestProfileManagerObserverIOS observer(profile_manager());
  ASSERT_FALSE(observer.on_profile_created_called());
  ASSERT_FALSE(observer.on_profile_loaded_called());

  // Load the Profiles, this will implicitly add "Default" as a Profile if there
  // is no saved Profiles. Thus it should load exactly one Profile.
  profile_manager().LoadProfiles();

  // Check that the observer has been notified of the creation and load.
  ASSERT_TRUE(observer.on_profile_created_called());
  ASSERT_TRUE(observer.on_profile_loaded_called());

  // Exactly one Profile must be loaded, it must be the last used Profile with
  // name `kIOSChromeInitialProfile`.
  EXPECT_EQ(GetLoadedProfileNames(),
            (std::set<std::string>{kIOSChromeInitialProfile}));
}

// Tests that LoadProfiles() always loads the "last used Profile" when
// `kLastUsedProfile` and `kLastActiveProfiles` are out of sync.
//
// See https://crbug.com/345478758 for crashes related to this.
//
// Specifically, this test case check that even if both properties are set but
// `kLastUsedProfile` is not `kIOSChromeInitialProfile` and not in
// `kLastActiveProfiles`, then the last used Profile is still loaded.
TEST_F(ProfileManagerIOSImplTest, LoadProfiles_IncoherentPrefs_1) {
  ASSERT_NE(kProfileName1, kIOSChromeInitialProfile);
  ASSERT_NE(kProfileName2, kIOSChromeInitialProfile);

  // There should be no Profile loaded yet.
  EXPECT_EQ(GetLoadedProfileNames(), (std::set<std::string>{}));

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetString(prefs::kLastUsedProfile, kProfileName1);
  local_state->SetList(prefs::kLastActiveProfiles,
                       base::Value::List().Append(kProfileName2));

  profile_manager().LoadProfiles();

  // Exactly two Profile must be loaded, named `kProfileName1` and
  // `kProfileName2`.
  EXPECT_EQ(GetLoadedProfileNames(),
            (std::set<std::string>{kProfileName1, kProfileName2}));
}

// Tests that LoadProfiles() always loads the "last used Profile" when
// `kLastUsedProfile` and `kLastActiveProfiles` are out of sync.
//
// See https://crbug.com/345478758 for crashes related to this.
//
// Specifically, this test case check that if `kLastActiveProfiles` is not set
// and `kLastUsedProfile` is not `kIOSChromeInitialProfile`, then the last
// used Profile is still loaded.
TEST_F(ProfileManagerIOSImplTest, LoadProfiles_IncoherentPrefs_2) {
  ASSERT_NE(kProfileName1, kIOSChromeInitialProfile);
  ASSERT_NE(kProfileName2, kIOSChromeInitialProfile);

  // There should be no Profile loaded yet.
  EXPECT_EQ(GetLoadedProfileNames(), (std::set<std::string>{}));

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetString(prefs::kLastUsedProfile, kProfileName1);
  local_state->SetList(prefs::kLastActiveProfiles, base::Value::List());

  profile_manager().LoadProfiles();

  // Exactly one Profile must be loaded, it must be the last used Profile with
  // name `kProfileName1`.
  EXPECT_EQ(GetLoadedProfileNames(), (std::set<std::string>{kProfileName1}));
}

// Tests that LoadProfiles() always loads the "last used Profile" when
// `kLastUsedProfile` and `kLastActiveProfiles` are out of sync.
//
// See https://crbug.com/345478758 for crashes related to this.
//
// Specifically, this test case check that if `kLastActiveProfiles` is set but
// does not contains the value `kIOSChromeInitialProfile` and
// `kLastUsedProfile` is unset, then the last used Profile is still loaded.
TEST_F(ProfileManagerIOSImplTest, LoadProfiles_IncoherentPrefs_3) {
  ASSERT_NE(kProfileName1, kIOSChromeInitialProfile);
  ASSERT_NE(kProfileName2, kIOSChromeInitialProfile);

  // There should be no Profile loaded yet.
  EXPECT_EQ(GetLoadedProfileNames(), (std::set<std::string>{}));

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetString(prefs::kLastUsedProfile, std::string());
  local_state->SetList(prefs::kLastActiveProfiles,
                       base::Value::List().Append(kProfileName2));

  profile_manager().LoadProfiles();

  // Exactly one Profile must be loaded, named `kProfileName2`.
  EXPECT_EQ(GetLoadedProfileNames(), (std::set<std::string>{kProfileName2}));
}

// Tests that LoadProfiles() ignores profile named "TestProfile[0-9]+" which
// were test profiles created for an experiment and should no longer be used.
TEST_F(ProfileManagerIOSImplTest, LoadProfiles_IgnoreTestProfiles) {
  ASSERT_NE(kTestProfile1, kIOSChromeInitialProfile);
  ASSERT_NE(kTestProfile2, kIOSChromeInitialProfile);

  // There should be no Profile loaded yet.
  EXPECT_EQ(GetLoadedProfileNames(), (std::set<std::string>{}));

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetString(prefs::kLastUsedProfile, std::string());
  local_state->SetList(prefs::kLastActiveProfiles, base::Value::List()
                                                       .Append(kProfileName1)
                                                       .Append(kTestProfile1)
                                                       .Append(kTestProfile2));

  profile_manager().LoadProfiles();

  // Exactly one Profile must be loaded, named `kProfileName1`.
  EXPECT_EQ(GetLoadedProfileNames(), (std::set<std::string>{kProfileName1}));
}

// Tests that LoadProfileAsync(...) correctly loads a known Profile, and that
// the load is not blocking the main thread.
TEST_F(ProfileManagerIOSImplTest, LoadProfileAsync) {
  // Pretends that a Profile named `kProfileName1` exists. Required as
  // LoadProfileAsync(...) won't create new Profiles.
  profile_attributes_storage().AddProfile(kProfileName1);

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
  profile_attributes_storage().AddProfile(kProfileName1);

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
  ASSERT_FALSE(profile_attributes_storage().HasProfileWithName(kProfileName1));

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
  ASSERT_FALSE(profile_attributes_storage().HasProfileWithName(kProfileName1));

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
  ASSERT_FALSE(profile_attributes_storage().HasProfileWithName(kProfileName1));

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
  profile_attributes_storage().AddProfile(kProfileName1);

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
  ASSERT_FALSE(profile_attributes_storage().HasProfileWithName(kProfileName1));

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
  ASSERT_FALSE(profile_attributes_storage().HasProfileWithName(kProfileName1));

  // Create the Profile synchronously.
  ProfileIOS* profile = profile_manager().CreateProfile(kProfileName1);

  // The Profile should have been successfully loaded and initialized.
  EXPECT_TRUE(profile);

  // Calling CreateProfile(...) a second time should return the same
  // object.
  EXPECT_EQ(profile, profile_manager().CreateProfile(kProfileName1));
}

using ProfileManagerIOSImplTest_HideLegacyProfile =
    ConfigurableProfileManagerIOSImplTest<FeatureState::kEnabled>;

// Tests that legacy profiles are hidden when kHideLegacyProfiles is enabled.
TEST_F(ProfileManagerIOSImplTest_HideLegacyProfile, Hide) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  ASSERT_FALSE(local_state->GetBoolean(prefs::kLegacyProfileHidden));

  // Create a legacy profile.
  profile_attributes_storage().AddProfile(kLegacyProfile);
  ASSERT_TRUE(profile_attributes_storage().HasProfileWithName(kLegacyProfile));
  local_state->ClearPref(prefs::kLastActiveProfiles);

  // Check that the profile is correctly marked as legacy when the profiles
  // are loaded.
  profile_manager().LoadProfiles();

  EXPECT_THAT(histogram_tester().GetAllSamples("Profile.LegacyProfilesCount"),
              testing::ElementsAre(base::Bucket(1, 1)));

  // Exactly one Profile must be loaded, it must be the last used Profile with
  // name `kIOSChromeInitialProfile`.
  EXPECT_EQ(GetLoadedProfileNames(),
            (std::set<std::string>{kIOSChromeInitialProfile}));

  // The legacy profile should no longer be visible in the
  // ProfileAttributesStorageIOS.
  EXPECT_FALSE(profile_attributes_storage().HasProfileWithName(kLegacyProfile));

  base::RunLoop run_loop;
  ProfileIOS* loaded_profile = nullptr;

  // Trying to create a profile named kLegacyProfile should fail and call the
  // initialized_callback with nullptr.
  const bool success = profile_manager().CreateProfileAsync(
      kLegacyProfile,
      CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
      FailCallback<ProfileIOS*>());
  EXPECT_FALSE(success);

  run_loop.Run();
  EXPECT_EQ(loaded_profile, nullptr);
}

// Tests that legacy profiles are hidden when kHideLegacyProfiles is enabled,
// but that this only happens once.
TEST_F(ProfileManagerIOSImplTest_HideLegacyProfile, Hide_AlreadyDone) {
  // Create profile that is not referenced (i.e. not loaded) thus could be
  // considered legacy, but pretend that the categorisation of legacy profiles
  // has already been run.
  profile_attributes_storage().AddProfile(kLegacyProfile);
  ASSERT_TRUE(profile_attributes_storage().HasProfileWithName(kLegacyProfile));

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->ClearPref(prefs::kLastActiveProfiles);
  local_state->SetBoolean(prefs::kLegacyProfileHidden, true);

  // Check that the profile is not marked as legacy but it is not loaded.
  profile_manager().LoadProfiles();

  EXPECT_THAT(histogram_tester().GetAllSamples("Profile.LegacyProfilesCount"),
              testing::ElementsAre(base::Bucket(0, 1)));

  // Exactly one Profile must be loaded, it must be the last used Profile with
  // name `kIOSChromeInitialProfile`.
  EXPECT_EQ(GetLoadedProfileNames(),
            (std::set<std::string>{kIOSChromeInitialProfile}));

  // The profile must still be visible in the ProfileAttributesStorageIOS.
  EXPECT_TRUE(profile_attributes_storage().HasProfileWithName(kLegacyProfile));

  base::RunLoop run_loop;
  ProfileIOS* created_profile = nullptr;
  ProfileIOS* loaded_profile = nullptr;

  // Trying to load the profile should succeed.
  const bool success = profile_manager().CreateProfileAsync(
      kLegacyProfile,
      CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
      CaptureParam(&created_profile));

  EXPECT_NE(created_profile, nullptr);
  EXPECT_TRUE(success);

  run_loop.Run();

  EXPECT_NE(loaded_profile, nullptr);
}

using ProfileManagerIOSImplTest_KeepLegacyProfile =
    ConfigurableProfileManagerIOSImplTest<FeatureState::kDisabled>;

// Tests that legacy profile are not touched if kHideLegacyProfiles feature
// is disabled.
TEST_F(ProfileManagerIOSImplTest_KeepLegacyProfile, Keep) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  ASSERT_FALSE(local_state->GetBoolean(prefs::kLegacyProfileHidden));

  // Create profile that is not referenced (i.e. not loaded) thus could be
  // considered legacy.
  profile_attributes_storage().AddProfile(kLegacyProfile);
  ASSERT_TRUE(profile_attributes_storage().HasProfileWithName(kLegacyProfile));
  local_state->ClearPref(prefs::kLastActiveProfiles);

  // Check that the profile is not marked as legacy but it is not loaded.
  profile_manager().LoadProfiles();

  EXPECT_THAT(histogram_tester().GetAllSamples("Profile.LegacyProfilesCount"),
              testing::ElementsAre(base::Bucket(0, 1)));

  // Exactly one Profile must be loaded, it must be the last used Profile with
  // name `kIOSChromeInitialProfile`.
  EXPECT_EQ(GetLoadedProfileNames(),
            (std::set<std::string>{kIOSChromeInitialProfile}));

  // The profile must still be visible in the ProfileAttributesStorageIOS.
  EXPECT_TRUE(profile_attributes_storage().HasProfileWithName(kLegacyProfile));

  base::RunLoop run_loop;
  ProfileIOS* created_profile = nullptr;
  ProfileIOS* loaded_profile = nullptr;

  // Trying to load the profile should succeed.
  const bool success = profile_manager().CreateProfileAsync(
      kLegacyProfile,
      CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
      CaptureParam(&created_profile));

  EXPECT_NE(created_profile, nullptr);
  EXPECT_TRUE(success);

  run_loop.Run();

  EXPECT_NE(loaded_profile, nullptr);
}

// Tests that legacy profile are restored when the feature is disabled and
// some profile were hidden due to the feature being enabled previously.
TEST_F(ProfileManagerIOSImplTest_KeepLegacyProfile, Restore) {
  // Pretend a legacy profile was hidden.
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetBoolean(prefs::kLegacyProfileHidden, true);
  local_state->SetDict(
      prefs::kLegacyProfileMap,
      base::Value::Dict().Set(kLegacyProfile, base::Value::Dict()));
  EXPECT_FALSE(profile_attributes_storage().HasProfileWithName(kLegacyProfile));

  // Check that the profile is not loaded but is restored.
  profile_manager().LoadProfiles();

  // Exactly one Profile must be loaded, it must be the last used Profile with
  // name `kIOSChromeInitialProfile`.
  EXPECT_EQ(GetLoadedProfileNames(),
            (std::set<std::string>{kIOSChromeInitialProfile}));

  EXPECT_THAT(histogram_tester().GetAllSamples("Profile.LegacyProfilesCount"),
              testing::ElementsAre(base::Bucket(0, 1)));

  // The profile must now be visible in the ProfileAttributesStorageIOS.
  EXPECT_TRUE(profile_attributes_storage().HasProfileWithName(kLegacyProfile));

  base::RunLoop run_loop;
  ProfileIOS* created_profile = nullptr;
  ProfileIOS* loaded_profile = nullptr;

  // Trying to load the profile should succeed.
  const bool success = profile_manager().CreateProfileAsync(
      kLegacyProfile,
      CaptureParam(&loaded_profile).Then(run_loop.QuitClosure()),
      CaptureParam(&created_profile));

  EXPECT_NE(created_profile, nullptr);
  EXPECT_TRUE(success);

  run_loop.Run();

  EXPECT_NE(loaded_profile, nullptr);
}
