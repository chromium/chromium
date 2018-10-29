// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {

// Requests a ProxyResolvingSocketFactoryPtr on the UI thread. Note that a
// WeakPtr of GCMProfileService is needed to detect when the KeyedService shuts
// down, and avoid calling into |profile| which might have also been destroyed.
void RequestProxyResolvingSocketFactoryOnUIThread(
    web::BrowserState* context,
    base::WeakPtr<gcm::GCMProfileService> service,
    network::mojom::ProxyResolvingSocketFactoryRequest request) {
  if (!service)
    return;
  context->GetProxyResolvingSocketFactory(std::move(request));
}

// A thread-safe wrapper to request a ProxyResolvingSocketFactoryPtr.
void RequestProxyResolvingSocketFactory(
    web::BrowserState* context,
    base::WeakPtr<gcm::GCMProfileService> service,
    network::mojom::ProxyResolvingSocketFactoryRequest request) {
  base::PostTaskWithTraits(
      FROM_HERE, {web::WebThread::UI},
      base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread, context,
                     std::move(service), std::move(request)));
}

}  // namespace

// static
gcm::GCMProfileService* WebViewGCMProfileServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<gcm::GCMProfileService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewGCMProfileServiceFactory*
WebViewGCMProfileServiceFactory::GetInstance() {
  return base::Singleton<WebViewGCMProfileServiceFactory>::get();
}

// static
std::string WebViewGCMProfileServiceFactory::GetProductCategoryForSubtypes() {
  return "org.chromium.chromewebview";
}

WebViewGCMProfileServiceFactory::WebViewGCMProfileServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "GCMProfileService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
}

WebViewGCMProfileServiceFactory::~WebViewGCMProfileServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewGCMProfileServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(!context->IsOffTheRecord());

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<gcm::GCMProfileService>(
      browser_state->GetPrefs(), browser_state->GetStatePath(),
      base::BindRepeating(&RequestProxyResolvingSocketFactory, context),
      browser_state->GetSharedURLLoaderFactory(),
      ApplicationContext::GetInstance()->GetNetworkConnectionTracker(),
      version_info::Channel::UNKNOWN, GetProductCategoryForSubtypes(),
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      base::WrapUnique(new gcm::GCMClientFactory),
      base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::UI}),
      base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::IO}),
      blocking_task_runner);
}
}  // namespace ios_web_view
