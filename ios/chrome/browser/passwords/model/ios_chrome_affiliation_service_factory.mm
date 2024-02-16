// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_affiliation_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "base/not_fatal_until.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/affiliations/core/browser/affiliation_service_impl.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
IOSChromeAffiliationServiceFactory*
IOSChromeAffiliationServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeAffiliationServiceFactory> instance;
  return instance.get();
}

// static
affiliations::AffiliationService*
IOSChromeAffiliationServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  CHECK(browser_state, base::NotFatalUntil::M123);

  // Always use the original BrowserState, not incognito. AffiliationService is
  // safe to use in incognito.
  auto* original_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state)
          ->GetOriginalChromeBrowserState();

  return static_cast<affiliations::AffiliationService*>(
      GetInstance()->GetServiceForBrowserState(original_browser_state, true));
}

IOSChromeAffiliationServiceFactory::IOSChromeAffiliationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AffiliationService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromeAffiliationServiceFactory::~IOSChromeAffiliationServiceFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromeAffiliationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  auto affiliation_service =
      std::make_unique<affiliations::AffiliationServiceImpl>(
          context->GetSharedURLLoaderFactory(), backend_task_runner);

  // TODO(b/324553078): Move this constant into an affiliations file.
  base::FilePath database_path = context->GetStatePath().Append(
      password_manager::kAffiliationDatabaseFileName);
  affiliation_service->Init(
      GetApplicationContext()->GetNetworkConnectionTracker(), database_path);

  return affiliation_service;
}
