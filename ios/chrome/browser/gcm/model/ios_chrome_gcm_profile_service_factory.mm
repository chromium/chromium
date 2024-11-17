// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/memory/ref_counted.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "build/branding_buildflags.h"
#import "components/gcm_driver/gcm_client_factory.h"
#import "components/gcm_driver/gcm_profile_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/mojom/proxy_resolving_socket.mojom.h"

namespace {

// Requests a network::mojom::ProxyResolvingSocketFactory on the UI thread. Note
// that a WeakPtr of GCMProfileService is needed to detect when the KeyedService
// shuts down, and avoid calling into `profile` which might have also been
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
gcm::GCMProfileService* IOSChromeGCMProfileServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<gcm::GCMProfileService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
IOSChromeGCMProfileServiceFactory*
IOSChromeGCMProfileServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeGCMProfileServiceFactory> instance;
  return instance.get();
}

// static
std::string IOSChromeGCMProfileServiceFactory::GetProductCategoryForSubtypes() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<gcm::GCMProfileService>(
      profile->GetPrefs(), profile->GetStatePath(),
      base::BindRepeating(&RequestProxyResolvingSocketFactory, context),
      profile->GetSharedURLLoaderFactory(),
      GetApplicationContext()->GetNetworkConnectionTracker(), ::GetChannel(),
      GetProductCategoryForSubtypes(),
      IdentityManagerFactory::GetForProfile(profile),
      base::WrapUnique(new gcm::GCMClientFactory),
      web::GetUIThreadTaskRunner({}), web::GetIOThreadTaskRunner({}),
      blocking_task_runner);
}
