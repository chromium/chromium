// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "base/not_fatal_until.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/affiliations/core/browser/affiliation_constants.h"
#import "components/affiliations/core/browser/affiliation_service_impl.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
IOSChromeAffiliationServiceFactory*
IOSChromeAffiliationServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeAffiliationServiceFactory> instance;
  return instance.get();
}

// static
affiliations::AffiliationService*
IOSChromeAffiliationServiceFactory::GetForProfile(ProfileIOS* profile) {
  CHECK(profile, base::NotFatalUntil::M123);

  return static_cast<affiliations::AffiliationService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
affiliations::AffiliationService*
IOSChromeAffiliationServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
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
  affiliation_service->Init(
      GetApplicationContext()->GetNetworkConnectionTracker(),
      context->GetStatePath().Append(
          affiliations::kAffiliationDatabaseFileName));

  return affiliation_service;
}

web::BrowserState* IOSChromeAffiliationServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
