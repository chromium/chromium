// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_IOS_IMPL_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_IOS_IMPL_H_

#include <memory>
#include <string_view>

#include "base/task/sequenced_task_runner.h"
#include "ios/chrome/browser/profile/model/profile_ios_impl_io_data.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace policy {
class SchemaRegistry;
class UserCloudPolicyManager;
}  // namespace policy

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefProxyConfigTracker;

// This class is the implementation of ProfileIOS used for non-incognito
// browsing.
class ProfileIOSImpl final : public ProfileIOS {
 public:
  ProfileIOSImpl(const ProfileIOSImpl&) = delete;
  ProfileIOSImpl& operator=(const ProfileIOSImpl&) = delete;

  ~ProfileIOSImpl() override;

  // ProfileIOS:
  // TODO(crbug.com/358299863): Remove these functions once fully migrated.
  ProfileIOS* GetOriginalChromeBrowserState() override;
  bool HasOffTheRecordChromeBrowserState() const override;
  ProfileIOS* GetOffTheRecordChromeBrowserState() override;
  void DestroyOffTheRecordChromeBrowserState() override;

  // ProfileIOS:
  ProfileIOS* GetOriginalProfile() override;
  bool HasOffTheRecordProfile() const override;
  ProfileIOS* GetOffTheRecordProfile() override;
  void DestroyOffTheRecordProfile() override;
  PrefProxyConfigTracker* GetProxyConfigTracker() override;
  BrowserStatePolicyConnector* GetPolicyConnector() override;
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
  sync_preferences::PrefServiceSyncable* GetSyncablePrefs() override;
  const sync_preferences::PrefServiceSyncable* GetSyncablePrefs()
      const override;
  ProfileIOSIOData* GetIOData() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   base::OnceClosure completion) override;
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) override;
  base::WeakPtr<ProfileIOS> AsWeakPtr() override;

  // BrowserState:
  bool IsOffTheRecord() const override;
  const std::string& GetWebKitStorageID() const override;

 private:
  friend class ProfileIOS;

  ProfileIOSImpl(const base::FilePath& state_path,
                 std::string_view profile_name,
                 scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                 CreationMode creation_mode,
                 Delegate* delegate);

  // Sets the OffTheRecordProfileIOS.
  void SetOffTheRecordProfileIOS(std::unique_ptr<ProfileIOS> otr_state);

  // Called when the PrefService is done loading (may be called synchronously
  // if the creation is done with `CreationMode::kSynchronous`).
  void OnPrefsLoaded(CreationMode creation_mode,
                     bool is_new_profile,
                     bool success);

  // The ProfileIOS::Delegate that will be notified of the progress
  // of the initialisation if not null.
  raw_ptr<Delegate> delegate_;

  // The incognito ProfileIOS instance that is associated with this ProfileIOS
  // instance. NULL if `GetOffTheRecordProfile()` has never been called or has
  // not been called since `DestroyOffTheRecordProfile()`.
  std::unique_ptr<ProfileIOS> otr_state_;

  // !!! BIG HONKING WARNING !!!
  //  The order of the members below is important. Do not change it unless
  //  you know what you're doing. Also, if adding a new member here make sure
  //  that the declaration occurs AFTER things it depends on as destruction
  //  happens in reverse order of declaration.

  // `policy_connector_` and its associated `policy_schema_registry_` must
  // outlive `prefs_`. `policy_connector_` depends on the policy provider
  // `user_cloud_policy_manager_` which depends on `policy_schema_registry_`.
  std::unique_ptr<policy::SchemaRegistry> policy_schema_registry_;
  std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
  std::unique_ptr<BrowserStatePolicyConnector> policy_connector_;

  // Keep `prefs_` above the rest for destruction order because `io_data_` and
  // others store pointers to `prefs_` and shall be destructed first.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  std::unique_ptr<ProfileIOSImplIOData::Handle> io_data_;

  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  // `storage_uuid_` can be empty if the profile already existed and no value is
  // stored in PrefService. Use a default data store if it's empty.
  std::string storage_uuid_;

  base::WeakPtrFactory<ProfileIOSImpl> weak_ptr_factory_{this};

  // STOP!!!! DO NOT ADD ANY MORE ITEMS HERE!!!!
  //
  // Instead, make your Service/Manager/whatever object you're hanging off the
  // Profile use our BrowserStateKeyedServiceFactory system instead.
  // You can find the design document here:
  //
  //   https://sites.google.com/a/chromium.org/dev/developers/design-documents/profile-architecture
  //
  // and you can read the raw headers here:
  //
  // components/keyed_service/ios/browser_state_dependency_manager.*
  // components/keyed_service/core/keyed_service.h
  // components/keyed_service/ios/browser_state_keyed_service_factory.*
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_IOS_IMPL_H_
