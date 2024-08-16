// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_profile_password_store_factory.h"

#import <memory>
#import <utility>

#import "base/command_line.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/password_store/login_database.h"
#import "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#import "components/password_manager/core/browser/password_store_factory_util.h"
#import "components/sync/service/sync_service.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
scoped_refptr<password_manager::PasswordStoreInterface>
WebViewProfilePasswordStoreFactory::GetForBrowserState(
    WebViewBrowserState* browser_state,
    ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      browser_state->IsOffTheRecord()) {
    return nullptr;
  }
  return base::WrapRefCounted(
      static_cast<password_manager::PasswordStoreInterface*>(
          GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
WebViewProfilePasswordStoreFactory*
WebViewProfilePasswordStoreFactory::GetInstance() {
  static base::NoDestructor<WebViewProfilePasswordStoreFactory> instance;
  return instance.get();
}

WebViewProfilePasswordStoreFactory::WebViewProfilePasswordStoreFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "PasswordStore",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewProfilePasswordStoreFactory::~WebViewProfilePasswordStoreFactory() {}

scoped_refptr<RefcountedKeyedService>
WebViewProfilePasswordStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForProfileStorage(
          context->GetStatePath(),
          WebViewBrowserState::FromBrowserState(context)->GetPrefs()));

  scoped_refptr<base::SequencedTaskRunner> main_task_runner(
      base::SequencedTaskRunner::GetCurrentDefault());
  // USER_VISIBLE priority is chosen for the background task runner, because
  // the passwords obtained through tasks on the background runner influence
  // what the user sees.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}));

  os_crypt_async::OSCryptAsync* os_crypt_async =
      base::FeatureList::IsEnabled(
          password_manager::features::kUseAsyncOsCryptInLoginDatabase)
          ? ApplicationContext::GetInstance()->GetOSCryptAsync()
          : nullptr;

  scoped_refptr<password_manager::PasswordStore> store =
      new password_manager::PasswordStore(
          std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
              std::move(login_db),
              syncer::WipeModelUponSyncDisabledBehavior::kNever,
              WebViewBrowserState::FromBrowserState(context)->GetPrefs(),
              os_crypt_async));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  return store;
}

web::BrowserState* WebViewProfilePasswordStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

bool WebViewProfilePasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios_web_view
