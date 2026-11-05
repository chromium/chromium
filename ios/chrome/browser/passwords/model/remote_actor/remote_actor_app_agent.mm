// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_app_agent.h"

#import <memory>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/signin/public/base/gaia_id_hash.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_init_stage.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace {

// Updates the shared common App Group NSUserDefaults with the Base64-encoded
// SHA-256 hashes of all signed-in profiles' GAIA IDs in `storage`.
void UpdateActiveGaiaIDHashes(const ProfileAttributesStorageIOS* storage) {
  if (!storage) {
    return;
  }

  NSMutableOrderedSet<NSString*>* active_hashes =
      [NSMutableOrderedSet orderedSet];
  storage->IterateOverProfileAttributes(base::BindRepeating(
      [](NSMutableOrderedSet<NSString*>* hashes,
         const ProfileAttributesIOS& attr) {
        if (attr.IsDeletedProfile()) {
          return;
        }
        GaiaId gaia_id = attr.GetGaiaId();
        if (!gaia_id.empty()) {
          NSString* hash = base::SysUTF8ToNSString(
              signin::GaiaIdHash::FromGaiaId(gaia_id).ToBase64());
          [hashes addObject:hash];
        }
      },
      active_hashes));

  NSUserDefaults* shared_defaults = app_group::GetCommonGroupUserDefaults();
  if (active_hashes.count > 0) {
    [shared_defaults setObject:[active_hashes array]
                        forKey:app_group::kChromeActiveGaiaIDHashesPreference];
  } else {
    [shared_defaults
        removeObjectForKey:app_group::kChromeActiveGaiaIDHashesPreference];
  }
}

// Observes profile attribute cache changes in Local State and refreshes the
// shared GAIA ID hash preference.
class ProfileAttributesObserverBridge : public ProfileManagerObserverIOS {
 public:
  ProfileAttributesObserverBridge(ProfileManagerIOS* profile_manager,
                                  PrefService* local_state)
      : profile_manager_(profile_manager) {
    DCHECK(profile_manager_);
    DCHECK(local_state);
    profile_manager_observation_.Observe(profile_manager_);
    pref_change_registrar_.Init(local_state);
    pref_change_registrar_.Add(
        prefs::kProfileInfoCache,
        base::BindRepeating(
            &ProfileAttributesObserverBridge::OnProfileInfoCacheChanged,
            base::Unretained(this)));
    UpdateActiveGaiaIDHashes(profile_manager_->GetProfileAttributesStorage());
  }

  ~ProfileAttributesObserverBridge() override = default;

  // ProfileManagerObserverIOS:
  void OnProfileManagerWillBeDestroyed(ProfileManagerIOS* manager) override {
    ResetObservations();
  }

  void OnProfileManagerDestroyed(ProfileManagerIOS* manager) override {
    ResetObservations();
  }

  void OnProfileCreated(ProfileManagerIOS* manager,
                        ProfileIOS* profile) override {}

  void OnProfileLoaded(ProfileManagerIOS* manager,
                       ProfileIOS* profile) override {}

  void OnProfileUnloaded(ProfileManagerIOS* manager,
                         ProfileIOS* profile) override {}

  void OnProfileMarkedForPermanentDeletion(ProfileManagerIOS* manager,
                                           ProfileIOS* profile) override {}

 private:
  void OnProfileInfoCacheChanged() {
    if (!profile_manager_) {
      return;
    }
    UpdateActiveGaiaIDHashes(profile_manager_->GetProfileAttributesStorage());
  }

  void ResetObservations() {
    pref_change_registrar_.RemoveAll();
    profile_manager_observation_.Reset();
    profile_manager_ = nullptr;
  }

  raw_ptr<ProfileManagerIOS> profile_manager_;
  PrefChangeRegistrar pref_change_registrar_;
  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      profile_manager_observation_{this};
};

}  // namespace

@implementation RemoteActorAppAgent {
  std::unique_ptr<ProfileAttributesObserverBridge> _observerBridge;
}

#pragma mark - ObservingAppAgent

- (void)setAppState:(AppState*)appState {
  [super setAppState:appState];
  [self startObservingIfReady];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  [self startObservingIfReady];
}

#pragma mark - Private

- (void)startObservingIfReady {
  if (_observerBridge ||
      self.appState.initStage <
          AppInitStage::kBrowserObjectsForBackgroundHandlers) {
    return;
  }

  ApplicationContext* applicationContext = GetApplicationContext();
  ProfileManagerIOS* profileManager = applicationContext->GetProfileManager();
  PrefService* localState = applicationContext->GetLocalState();
  if (!profileManager || !localState) {
    return;
  }

  _observerBridge = std::make_unique<ProfileAttributesObserverBridge>(
      profileManager, localState);
}

@end
