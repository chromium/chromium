// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_APP_APPLICATION_CONTEXT_H_
#define IOS_WEB_VIEW_INTERNAL_APP_APPLICATION_CONTEXT_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "ios/web/public/init/network_context_owner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace component_updater {
class ComponentUpdateService;
}

namespace net {
class NetLog;
class URLRequestContextGetter;
}

namespace network {
class NetworkChangeManager;
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
class WeakWrapperSharedURLLoaderFactory;
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace os_crypt_async {
class OSCryptAsync;
}

class PrefService;
class SafeBrowsingService;

namespace ios_web_view {

class WebViewIOThread;

// Exposes application global state objects.
class ApplicationContext {
 public:
  static ApplicationContext* GetInstance();

  ApplicationContext(const ApplicationContext&) = delete;
  ApplicationContext& operator=(const ApplicationContext&) = delete;

  // Gets the preferences associated with this application.
  PrefService* GetLocalState();

  // Gets the URL request context associated with this application.
  net::URLRequestContextGetter* GetSystemURLRequestContext();

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory();
  network::mojom::NetworkContext* GetSystemNetworkContext();

  // Returns the NetworkConnectionTracker instance for this ApplicationContext.
  network::NetworkConnectionTracker* GetNetworkConnectionTracker();

  // Gets the locale used by the application.
  const std::string& GetApplicationLocale();

  // Gets the NetLog.
  net::NetLog* GetNetLog();

  // Gets the ComponentUpdateService.
  component_updater::ComponentUpdateService* GetComponentUpdateService();

  // Gets the application specific OSCryptAsync instance.
  os_crypt_async::OSCryptAsync* GetOSCryptAsync();

  // Creates state tied to application threads. It is expected this will be
  // called from web::WebMainParts::PreCreateThreads.
  void PreCreateThreads();

  // Called after the browser threads are created. It is expected this will be
  // called from web::WebMainParts::PostCreateThreads.
  void PostCreateThreads();

  // Saves application context state if |local_state_| exists. This should be
  // called during shutdown to save application state.
  void SaveState();

  // Destroys state tied to application threads. It is expected this will be
  // called from web::WebMainParts::PostDestroyThreads.
  void PostDestroyThreads();

  // Gets the SafeBrowsingService.
  SafeBrowsingService* GetSafeBrowsingService();

  // Shuts down SafeBrowsingService if it was created.
  void ShutdownSafeBrowsingServiceIfNecessary();

 private:
  friend class base::NoDestructor<ApplicationContext>;

  ApplicationContext();
  ~ApplicationContext();

  // Gets the WebViewIOThread.
  WebViewIOThread* GetWebViewIOThread();

  // Sets the locale used by the application.
  void SetApplicationLocale(const std::string& locale);

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<WebViewIOThread> web_view_io_thread_;
  std::string application_locale_;

  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_;

  // Created on the UI thread, destroyed on the IO thread.
  std::unique_ptr<web::NetworkContextOwner> network_context_owner_;

  std::unique_ptr<network::NetworkChangeManager> network_change_manager_;
  std::unique_ptr<network::NetworkConnectionTracker>
      network_connection_tracker_;

  std::unique_ptr<component_updater::ComponentUpdateService> component_updater_;

  scoped_refptr<SafeBrowsingService> safe_browsing_service_;

  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_APP_APPLICATION_CONTEXT_H_
