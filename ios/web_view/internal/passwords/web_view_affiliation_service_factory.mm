// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_affiliation_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/password_manager/core/browser/affiliation/affiliation_service_impl.h"
#import "components/password_manager/core/browser/bulk_leak_check_service.h"
#import "components/password_manager/core/browser/bulk_leak_check_service_interface.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/mojom/proxy_resolving_socket.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
WebViewAffiliationServiceFactory*
WebViewAffiliationServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewAffiliationServiceFactory> instance;
  return instance.get();
}

// static
password_manager::AffiliationService*
WebViewAffiliationServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<password_manager::AffiliationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewAffiliationServiceFactory::WebViewAffiliationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AffiliationService",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewAffiliationServiceFactory::~WebViewAffiliationServiceFactory() = default;

std::unique_ptr<KeyedService>
WebViewAffiliationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  auto affiliation_service =
      std::make_unique<password_manager::AffiliationServiceImpl>(
          context->GetSharedURLLoaderFactory(), backend_task_runner,
          WebViewBrowserState::FromBrowserState(context)->GetPrefs());

  base::FilePath database_path = context->GetStatePath().Append(
      password_manager::kAffiliationDatabaseFileName);
  affiliation_service->Init(
      ApplicationContext::GetInstance()->GetNetworkConnectionTracker(),
      database_path);

  return affiliation_service;
}

}  // namespace ios_web_view
