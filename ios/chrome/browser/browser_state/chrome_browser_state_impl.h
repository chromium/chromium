// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IMPL_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IMPL_H_

#include <memory>

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_impl_io_data.h"

namespace policy {
class SchemaRegistry;
class UserCloudPolicyManager;
}

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefProxyConfigTracker;

// This class is the implementation of ChromeBrowserState used for
// non-incognito browsing.
class ChromeBrowserStateImpl final : public ChromeBrowserState {
 public:
  ChromeBrowserStateImpl(const ChromeBrowserStateImpl&) = delete;
  ChromeBrowserStateImpl& operator=(const ChromeBrowserStateImpl&) = delete;

  ~ChromeBrowserStateImpl() override;

  // ChromeBrowserState:
  ChromeBrowserState* GetOriginalChromeBrowserState() override;
  bool HasOffTheRecordChromeBrowserState() const override;
  ChromeBrowserState* GetOffTheRecordChromeBrowserState() override;
  void DestroyOffTheRecordChromeBrowserState() override;
  PrefProxyConfigTracker* GetProxyConfigTracker() override;
  BrowserStatePolicyConnector* GetPolicyConnector() override;
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
  sync_preferences::PrefServiceSyncable* GetSyncablePrefs() override;
  ChromeBrowserStateIOData* GetIOData() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   base::OnceClosure completion) override;
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) override;
  base::WeakPtr<ChromeBrowserState> AsWeakPtr() override;

  // BrowserState:
  bool IsOffTheRecord() const override;
  base::FilePath GetStatePath() const override;

 private:
  friend class ChromeBrowserStateManagerImpl;

  ChromeBrowserStateImpl(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      const base::FilePath& path);

  // Sets the OffTheRecordChromeBrowserState.
  void SetOffTheRecordChromeBrowserState(
      std::unique_ptr<ChromeBrowserState> otr_state);

  base::FilePath state_path_;

  // The incognito ChromeBrowserState instance that is associated with this
  // ChromeBrowserState instance. NULL if `GetOffTheRecordChromeBrowserState()`
  // has never been called or has not been called since
  // `DestroyOffTheRecordChromeBrowserState()`.
  std::unique_ptr<ChromeBrowserState> otr_state_;
  base::FilePath otr_state_path_;

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
  std::unique_ptr<ChromeBrowserStateImplIOData::Handle> io_data_;

  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  base::WeakPtrFactory<ChromeBrowserStateImpl> weak_ptr_factory_{this};

  // STOP!!!! DO NOT ADD ANY MORE ITEMS HERE!!!!
  //
  // Instead, make your Service/Manager/whatever object you're hanging off the
  // BrowserState use our BrowserStateKeyedServiceFactory system instead.
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

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_IMPL_H_
