// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prefs/model/ios_chrome_pref_service_factory.h"

#import <vector>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/task/sequenced_task_runner.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/common/policy_service.h"
#import "components/prefs/json_pref_store.h"
#import "components/prefs/persistent_pref_store.h"
#import "components/prefs/pref_filter.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/pref_store.h"
#import "components/prefs/pref_value_store.h"
#import "components/prefs/wrap_with_prefix_pref_store.h"
#import "components/proxy_config/proxy_config_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/supervised_user/core/browser/supervised_user_pref_store.h"
#import "components/supervised_user/core/common/features.h"
#import "components/sync/base/features.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/pref_service_syncable_factory.h"
#import "ios/chrome/browser/prefs/model/ios_chrome_pref_model_associator_client.h"

namespace {

const char kAccountPreferencesPrefix[] = "account_values";

void PrepareFactory(sync_preferences::PrefServiceSyncableFactory* factory,
                    scoped_refptr<JsonPrefStore> user_pref_store,
                    policy::PolicyService* policy_service,
                    policy::BrowserPolicyConnector* policy_connector,
                    const scoped_refptr<PrefStore>& supervised_user_prefs) {
  if (policy_service || policy_connector) {
    DCHECK(policy_service && policy_connector);
    factory->SetManagedPolicies(policy_service, policy_connector);
    factory->SetRecommendedPolicies(policy_service, policy_connector);
  }

  if (supervised_user_prefs) {
    factory->set_supervised_user_prefs(supervised_user_prefs);
  }

  factory->set_user_prefs(std::move(user_pref_store));

  factory->SetPrefModelAssociatorClient(
      base::MakeRefCounted<IOSChromePrefModelAssociatorClient>());
}

}  // namespace

const base::FilePath::CharType kPreferencesFilename[] =
    FILE_PATH_LITERAL("Preferences");
const base::FilePath::CharType kAccountPreferencesFilename[] =
    FILE_PATH_LITERAL("AccountPreferences");

std::unique_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    const scoped_refptr<PrefRegistry>& pref_registry,
    policy::PolicyService* policy_service,
    policy::BrowserPolicyConnector* policy_connector) {
  sync_preferences::PrefServiceSyncableFactory factory;
  PrepareFactory(
      &factory,
      base::MakeRefCounted<JsonPrefStore>(
          pref_filename, std::unique_ptr<PrefFilter>(), pref_io_task_runner),
      policy_service, policy_connector, /*supervised_user_prefs=*/nullptr);
  return factory.Create(pref_registry.get());
}

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreateProfilePrefs(
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* pref_io_task_runner,
    const scoped_refptr<user_prefs::PrefRegistrySyncable>& pref_registry,
    policy::PolicyService* policy_service,
    policy::BrowserPolicyConnector* policy_connector,
    const scoped_refptr<PrefStore>& supervised_user_prefs,
    bool async) {
  // chrome_prefs::CreateProfilePrefs uses ProfilePrefStoreManager to create
  // the preference store however since Chrome on iOS does not need to track
  // preference modifications (as applications are sand-boxed), it can use a
  // simple JsonPrefStore to store them (which is what PrefStoreManager uses
  // on platforms that do not track preference modifications).
  scoped_refptr<JsonPrefStore> local_pref_store =
      base::MakeRefCounted<JsonPrefStore>(
          profile_path.Append(kPreferencesFilename),
          std::unique_ptr<PrefFilter>(), pref_io_task_runner);
  sync_preferences::PrefServiceSyncableFactory factory;
  PrepareFactory(&factory, local_pref_store, policy_service, policy_connector,
                 supervised_user_prefs);
  if (base::FeatureList::IsEnabled(
          switches::kEnablePreferencesAccountStorage)) {
    base::FilePath account_prefs_filepath =
        profile_path.Append(kAccountPreferencesFilename);
    if (base::FeatureList::IsEnabled(syncer::kMigrateAccountPrefs)) {
      // Post task to remove the account preferences file. This is done on the
      // IO thread.
      pref_io_task_runner->PostTask(
          FROM_HERE, base::BindOnce(IgnoreResult(&base::DeleteFile),
                                    account_prefs_filepath));
      factory.SetAccountPrefStore(base::MakeRefCounted<WrapWithPrefixPrefStore>(
          local_pref_store, kAccountPreferencesPrefix));
    } else {
      factory.SetAccountPrefStore(base::MakeRefCounted<JsonPrefStore>(
          account_prefs_filepath, nullptr, pref_io_task_runner));
    }
  }
  factory.set_async(async);
  std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service =
      factory.CreateSyncable(pref_registry.get());
  return pref_service;
}

std::unique_ptr<sync_preferences::PrefServiceSyncable>
CreateIncognitoProfilePrefs(
    sync_preferences::PrefServiceSyncable* pref_service) {
  // List of keys that cannot be changed in the user prefs file by the incognito
  // browser state. All preferences that store information about the browsing
  // history or behaviour of the user should have this property.
  std::vector<const char*> overlay_pref_names;
  overlay_pref_names.push_back(proxy_config::prefs::kProxy);
  return pref_service->CreateIncognitoPrefService(
      /*incognito_extension_pref_store=*/nullptr, overlay_pref_names);
}
