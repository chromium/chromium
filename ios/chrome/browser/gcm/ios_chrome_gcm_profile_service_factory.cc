// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/gcm/ios_chrome_gcm_profile_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"

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
  base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::UI})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread, context,
                         std::move(service), std::move(request)));
}

}  // namespace

// static
gcm::GCMProfileService* IOSChromeGCMProfileServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<gcm::GCMProfileService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeGCMProfileServiceFactory*
IOSChromeGCMProfileServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeGCMProfileServiceFactory>::get();
}

// static
std::string IOSChromeGCMProfileServiceFactory::GetProductCategoryForSubtypes() {
#if defined(GOOGLE_CHROME_BUILD)
  return "com.chrome.ios";
#else
  return "org.chromium.ios";
#endif
}

IOSChromeGCMProfileServiceFactory::IOSChromeGCMProfileServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "GCMProfileService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSChromeGCMProfileServiceFactory::~IOSChromeGCMProfileServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeGCMProfileServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(!context->IsOffTheRecord());

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<gcm::GCMProfileService>(
      browser_state->GetPrefs(), browser_state->GetStatePath(),
      base::BindRepeating(&RequestProxyResolvingSocketFactory, context),
      browser_state->GetSharedURLLoaderFactory(),
      GetApplicationContext()->GetNetworkConnectionTracker(), ::GetChannel(),
      GetProductCategoryForSubtypes(),
      IdentityManagerFactory::GetForBrowserState(browser_state),
      base::WrapUnique(new gcm::GCMClientFactory),
      base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::UI}),
      base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::IO}),
      blocking_task_runner);
}
