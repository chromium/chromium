// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/glue/sync_start_util.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
scoped_refptr<password_manager::PasswordStore>
IOSChromePasswordStoreFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      browser_state->IsOffTheRecord())
    return nullptr;
  return base::WrapRefCounted(static_cast<password_manager::PasswordStore*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
IOSChromePasswordStoreFactory* IOSChromePasswordStoreFactory::GetInstance() {
  static base::NoDestructor<IOSChromePasswordStoreFactory> instance;
  return instance.get();
}

// static
void IOSChromePasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged(
    ios::ChromeBrowserState* browser_state) {
  scoped_refptr<password_manager::PasswordStore> password_store =
      GetForBrowserState(browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserStateIfExists(browser_state);
  password_manager::ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
      password_store.get(), sync_service,
      browser_state->GetSharedURLLoaderFactory(),
      GetApplicationContext()->GetNetworkConnectionTracker(),
      browser_state->GetStatePath());
}

IOSChromePasswordStoreFactory::IOSChromePasswordStoreFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "PasswordStore",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::WebDataServiceFactory::GetInstance());
}

IOSChromePasswordStoreFactory::~IOSChromePasswordStoreFactory() {}

scoped_refptr<RefcountedKeyedService>
IOSChromePasswordStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForProfileStorage(
          context->GetStatePath()));

  scoped_refptr<base::SequencedTaskRunner> main_task_runner(
      base::SequencedTaskRunnerHandle::Get());
  // USER_VISIBLE priority is chosen for the background task runner, because
  // the passwords obtained through tasks on the background runner influence
  // what the user sees.
  // TODO(crbug.com/741660): Create the task runner inside password_manager
  // component instead.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner(
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                       base::TaskPriority::USER_VISIBLE}));

  scoped_refptr<password_manager::PasswordStore> store =
      new password_manager::PasswordStoreDefault(std::move(login_db));
  if (!store->Init(ios::sync_start_util::GetFlareForSyncableService(
                       context->GetStatePath()),
                   nullptr)) {
    // TODO(crbug.com/479725): Remove the LOG once this error is visible in the
    // UI.
    LOG(WARNING) << "Could not initialize password store.";
    return nullptr;
  }
  password_manager_util::RemoveUselessCredentials(
      store, ios::ChromeBrowserState::FromBrowserState(context)->GetPrefs(), 60,
      base::NullCallback());
  return store;
}

web::BrowserState* IOSChromePasswordStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool IOSChromePasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
