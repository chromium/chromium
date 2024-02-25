// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/memory/ref_counted.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/gcm_driver/gcm_client_factory.h"
#import "components/gcm_driver/gcm_profile_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/version_info/channel.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/mojom/proxy_resolving_socket.mojom.h"

namespace ios_web_view {

namespace {

// Requests a network::mojom::ProxyResolvingSocketFactory on the UI thread. Note
// that a WeakPtr of GCMProfileService is needed to detect when the KeyedService
// shuts down, and avoid calling into |profile| which might have also been
// destroyed.
void RequestProxyResolvingSocketFactoryOnUIThread(
    web::BrowserState* context,
    base::WeakPtr<gcm::GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  if (!service)
    return;
  context->GetProxyResolvingSocketFactory(std::move(receiver));
}

// A thread-safe wrapper to request a
// network::mojom::ProxyResolvingSocketFactory.
void RequestProxyResolvingSocketFactory(
    web::BrowserState* context,
    base::WeakPtr<gcm::GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread, context,
                     std::move(service), std::move(receiver)));
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
  static base::NoDestructor<WebViewGCMProfileServiceFactory> instance;
  return instance.get();
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
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<gcm::GCMProfileService>(
      browser_state->GetPrefs(), browser_state->GetStatePath(),
      base::BindRepeating(&RequestProxyResolvingSocketFactory, context),
      browser_state->GetSharedURLLoaderFactory(),
      ApplicationContext::GetInstance()->GetNetworkConnectionTracker(),
      version_info::Channel::STABLE, GetProductCategoryForSubtypes(),
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      base::WrapUnique(new gcm::GCMClientFactory),
      web::GetUIThreadTaskRunner({}), web::GetIOThreadTaskRunner({}),
      blocking_task_runner);
}
}  // namespace ios_web_view
