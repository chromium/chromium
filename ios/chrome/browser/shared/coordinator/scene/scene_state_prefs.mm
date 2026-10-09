// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_prefs.h"

#import <string>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

namespace {

// Helper used to read/write preference of type `T`.
template <typename T>
struct StorageHelper {};

// Partial specialisation of `StorageHelper` for `bool` preference.
template <>
struct StorageHelper<bool> {
  static bool Get(const ProfileAttributesIOS& attr,
                  std::string_view session_name,
                  std::string_view pref_name) {
    return attr.GetSessionScopedBoolPref(session_name, pref_name);
  }

  static void Set(ProfileAttributesIOS& attr,
                  std::string_view session_name,
                  std::string_view pref_name,
                  bool value) {
    attr.SetSessionScopedBoolPref(session_name, pref_name, value);
  }
};

// Partial specialisation of `StorageHelper` for `base::Time` preference.
template <>
struct StorageHelper<base::Time> {
  static base::Time Get(const ProfileAttributesIOS& attr,
                        std::string_view session_name,
                        std::string_view pref_name) {
    return attr.GetSessionScopedTimePref(session_name, pref_name);
  }

  static void Set(ProfileAttributesIOS& attr,
                  std::string_view session_name,
                  std::string_view pref_name,
                  base::Time value) {
    attr.SetSessionScopedTimePref(session_name, pref_name, value);
  }
};

// Helper providing read/write access to SceneState session scoped prefs.
class SceneStatePrefsHelper {
 public:
  SceneStatePrefsHelper(ProfileAttributesStorageIOS* storage,
                        std::string_view profile_name,
                        std::string_view session_name)
      : storage_(CHECK_DEREF(storage)),
        profile_name_(profile_name),
        session_name_(session_name) {}

  // Gets bool for `pref_name`.
  bool GetBoolPref(std::string_view pref_name) const {
    return GetPref<bool>(pref_name);
  }

  // Sets value for `pref_name`.
  void SetBoolPref(std::string_view pref_name, bool value) {
    SetPref(pref_name, value);
  }

  // Gets base::Time for `pref_name`.
  base::Time GetTimePref(std::string_view pref_name) const {
    return GetPref<base::Time>(pref_name);
  }

  // Sets value for `pref_name`.
  void SetTimePref(std::string_view pref_name, base::Time value) {
    SetPref(pref_name, value);
  }

 private:
  // Stores `value` under `pref`.
  template <typename T>
  void SetPref(std::string_view pref_name, T value) {
    storage_->UpdateAttributesForProfileWithName(
        profile_name_,
        base::BindOnce(
            [](std::string_view session_name, std::string_view pref_name,
               T value, ProfileAttributesIOS& attrs) {
              StorageHelper<T>::Set(attrs, session_name, pref_name, value);
            },
            session_name_, pref_name, value));
  }

  // Retrieves the value for `pref`
  template <typename T>
  T GetPref(std::string_view pref) const {
    return StorageHelper<T>::Get(
        storage_->GetAttributesForProfileWithName(profile_name_), session_name_,
        pref);
  }

  raw_ref<ProfileAttributesStorageIOS> storage_;
  const std::string profile_name_;
  const std::string session_name_;
};

// Helper observing the ProfileManagerIOS and calling a callback when
// the observed profile is unloaded.
class SceneStatePrefsProfileManagerObserver : public ProfileManagerObserverIOS {
 public:
  SceneStatePrefsProfileManagerObserver(ProfileManagerIOS* manager,
                                        std::string_view profile_name,
                                        base::OnceClosure closure)
      : closure_(std::move(closure)), profile_name_(profile_name) {
    observation_.Observe(manager);
  }

  // Called when the profile `profile_name` has been unloaded.
  void OnProfileUnloaded() {
    observation_.Reset();
    std::move(closure_).Run();
  }

  // ProfileManagerObserverIOS:
  void OnProfileManagerWillBeDestroyed(ProfileManagerIOS* manager) override {
    OnProfileUnloaded();
  }

  void OnProfileManagerDestroyed(ProfileManagerIOS* manager) override {
    OnProfileUnloaded();
  }

  void OnProfileCreated(ProfileManagerIOS* manager,
                        ProfileIOS* profile) override {
    // Nothing to do.
  }

  void OnProfileLoaded(ProfileManagerIOS* manager,
                       ProfileIOS* profile) override {
    // Nothing to do.
  }

  void OnProfileUnloaded(ProfileManagerIOS* manager,
                         ProfileIOS* profile) override {
    if (profile->GetProfileName() == profile_name_) {
      OnProfileUnloaded();
    }
  }

  void OnProfileMarkedForPermanentDeletion(ProfileManagerIOS* manager,
                                           ProfileIOS* profile) override {
    // Nothing to do.
  }

 private:
  base::OnceClosure closure_;
  const std::string profile_name_;
  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      observation_{this};
};

}  // namespace

@implementation SceneStatePrefs {
  std::unique_ptr<SceneStatePrefsHelper> _helper;
  std::unique_ptr<SceneStatePrefsProfileManagerObserver> _observer;
}

- (instancetype)initWithProfileManager:(ProfileManagerIOS*)profileManager
                           profileName:(std::string_view)profileName
                     sessionIdentifier:(std::string_view)sessionIdentifier {
  if ((self = [super init])) {
    _helper = std::make_unique<SceneStatePrefsHelper>(
        profileManager->GetProfileAttributesStorage(), profileName,
        sessionIdentifier);

    __weak SceneStatePrefs* weakSelf = self;
    _observer = std::make_unique<SceneStatePrefsProfileManagerObserver>(
        profileManager, profileName, base::BindOnce(^{
          [weakSelf profileUnloaded];
        }));
  }
  return self;
}

- (bool)boolForKey:(std::string_view)key {
  CHECK(_helper) << "should not be used after profile is unloaded";
  return _helper->GetBoolPref(key);
}

- (void)setBool:(bool)value forKey:(std::string_view)key {
  CHECK(_helper) << "should not be used after profile is unloaded";
  _helper->SetBoolPref(key, value);
}

- (base::Time)timeForKey:(std::string_view)key {
  CHECK(_helper) << "should not be used after profile is unloaded";
  return _helper->GetTimePref(key);
}

- (void)setTime:(base::Time)value forKey:(std::string_view)key {
  CHECK(_helper) << "should not be used after profile is unloaded";
  _helper->SetTimePref(key, value);
}

// Called when the profile is unloaded.
- (void)profileUnloaded {
  _observer.reset();
  _helper.reset();
}

@end
