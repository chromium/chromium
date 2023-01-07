// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prefs/ios_chrome_pref_service_factory.h"

#import <vector>

#import "base/bind.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/common/policy_service.h"
#import "components/prefs/json_pref_store.h"
#import "components/prefs/persistent_pref_store.h"
#import "components/prefs/pref_filter.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/pref_value_store.h"
#import "components/proxy_config/proxy_config_pref_names.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/pref_service_syncable_factory.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/prefs/ios_chrome_pref_model_associator_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kPreferencesFilename[] = "Preferences";

// Record PersistentPrefStore's reading errors distribution.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
  // Sample the histogram also for the successful case in order to get a
  // baseline on the success rate in addition to the error distribution.
  UMA_HISTOGRAM_ENUMERATION("PrefService.ReadError", error,
                            PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM);
}

void PrepareFactory(sync_preferences::PrefServiceSyncableFactory* factory,
                    const base::FilePath& pref_filename,
                    base::SequencedTaskRunner* pref_io_task_runner,
                    policy::PolicyService* policy_service,
                    policy::BrowserPolicyConnector* policy_connector) {
  if (policy_service || policy_connector) {
    DCHECK(policy_service && policy_connector);
    factory->SetManagedPolicies(policy_service, policy_connector);
    factory->SetRecommendedPolicies(policy_service, policy_connector);
  }

  factory->set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      pref_filename, std::unique_ptr<PrefFilter>(), pref_io_task_runner));

  factory->set_read_error_callback(base::BindRepeating(&HandleReadError));
  factory->SetPrefModelAssociatorClient(
      IOSChromePrefModelAssociatorClient::GetInstance());
}

}  // namespace

std::unique_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    const scoped_refptr<PrefRegistry>& pref_registry,
    policy::PolicyService* policy_service,
    policy::BrowserPolicyConnector* policy_connector) {
  sync_preferences::PrefServiceSyncableFactory factory;
  PrepareFactory(&factory, pref_filename, pref_io_task_runner, policy_service,
                 policy_connector);
  return factory.Create(pref_registry.get());
}

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreateBrowserStatePrefs(
    const base::FilePath& browser_state_path,
    base::SequencedTaskRunner* pref_io_task_runner,
    const scoped_refptr<user_prefs::PrefRegistrySyncable>& pref_registry,
    policy::PolicyService* policy_service,
    policy::BrowserPolicyConnector* policy_connector) {
  // chrome_prefs::CreateProfilePrefs uses ProfilePrefStoreManager to create
  // the preference store however since Chrome on iOS does not need to track
  // preference modifications (as applications are sand-boxed), it can use a
  // simple JsonPrefStore to store them (which is what PrefStoreManager uses
  // on platforms that do not track preference modifications).
  sync_preferences::PrefServiceSyncableFactory factory;
  PrepareFactory(&factory, browser_state_path.Append(kPreferencesFilename),
                 pref_io_task_runner, policy_service, policy_connector);
  std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service =
      factory.CreateSyncable(pref_registry.get());
  return pref_service;
}

std::unique_ptr<sync_preferences::PrefServiceSyncable>
CreateIncognitoBrowserStatePrefs(
    sync_preferences::PrefServiceSyncable* pref_service) {
  // List of keys that cannot be changed in the user prefs file by the incognito
  // browser state. All preferences that store information about the browsing
  // history or behaviour of the user should have this property.
  std::vector<const char*> overlay_pref_names;
  overlay_pref_names.push_back(proxy_config::prefs::kProxy);
  return pref_service->CreateIncognitoPrefService(
      /*incognito_extension_pref_store=*/nullptr, overlay_pref_names);
}
