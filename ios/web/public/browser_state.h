// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSER_STATE_H_
#define IOS_WEB_PUBLIC_BROWSER_STATE_H_

#include <memory>

#include "base/supports_user_data.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"

namespace base {
class FilePath;
}

namespace net {
class URLRequestContextGetter;
}

namespace network {
class SharedURLLoaderFactory;
class WeakWrapperSharedURLLoaderFactory;
}  // namespace network

namespace service_manager {
class Connector;
}

namespace web {
class CertificatePolicyCache;
class NetworkContextOwner;
class ServiceManagerConnection;
class URLDataManagerIOS;
class URLDataManagerIOSBackend;
class URLRequestChromeJob;

// This class holds the context needed for a browsing session.
// It lives on the UI thread. All these methods must only be called on the UI
// thread.
class BrowserState : public base::SupportsUserData {
 public:
  ~BrowserState() override;

  // static
  static scoped_refptr<CertificatePolicyCache> GetCertificatePolicyCache(
      BrowserState* browser_state);

  // Returns whether this BrowserState is incognito. Default is false.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the path where the BrowserState data is stored.
  // Unlike Profile::GetPath(), incognito BrowserState do not share their path
  // with their original BrowserState.
  virtual base::FilePath GetStatePath() const = 0;

  // Returns the request context information associated with this
  // BrowserState.
  virtual net::URLRequestContextGetter* GetRequestContext() = 0;

  // Returns a URLLoaderFactory that is backed by GetRequestContext.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory();

  // Returns a CookieManager that is backed by GetRequestContext.
  network::mojom::CookieManager* GetCookieManager();

  // Binds a ProxyResolvingSocketFactory request to NetworkContext.
  void GetProxyResolvingSocketFactory(
      network::mojom::ProxyResolvingSocketFactoryRequest request);

  // Like URLLoaderFactory, but wrapped inside SharedURLLoaderFactory
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSharedURLLoaderFactory();

  // Safely cast a base::SupportsUserData to a BrowserState. Returns nullptr
  // if |supports_user_data| is not a BrowserState.
  static BrowserState* FromSupportsUserData(
      base::SupportsUserData* supports_user_data);

  // Returns a Service User ID associated with this BrowserState. This ID is
  // not persistent across runs. See
  // services/service_manager/public/mojom/connector.mojom. By default,
  // this user id is randomly generated when Initialize() is called.
  static const std::string& GetServiceUserIdFor(BrowserState* browser_state);

  // Returns a Connector associated with this BrowserState, which can be used
  // to connect to service instances bound as this user.
  static service_manager::Connector* GetConnectorFor(
      BrowserState* browser_state);

  // Returns a ServiceManagerConnection associated with this BrowserState,
  // which can be used to connect to service instances bound as this user.
  static ServiceManagerConnection* GetServiceManagerConnectionFor(
      BrowserState* browser_state);

  using StaticServiceMap =
      std::map<std::string, service_manager::EmbeddedServiceInfo>;

  // Registers per-browser-state services to be loaded by the Service Manager.
  virtual void RegisterServices(StaticServiceMap* services) {}

 protected:
  BrowserState();

  // Makes the Service Manager aware of this BrowserState, and assigns a user
  // ID number to it. Must be called for each BrowserState created. |path|
  // should be the same path that would be returned by GetStatePath().
  static void Initialize(BrowserState* browser_state,
                         const base::FilePath& path);

 private:
  friend class URLDataManagerIOS;
  friend class URLRequestChromeJob;

  // Returns the URLDataManagerIOSBackend instance associated with this
  // BrowserState, creating it if necessary. Should only be called on the IO
  // thread.
  // Not intended for usage outside of //web.
  URLDataManagerIOSBackend* GetURLDataManagerIOSBackendOnIOThread();

  void CreateNetworkContextIfNecessary();

  network::mojom::URLLoaderFactoryPtr url_loader_factory_;
  network::mojom::CookieManagerPtr cookie_manager_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_;
  network::mojom::NetworkContextPtr network_context_;

  // Owns the network::NetworkContext that backs |url_loader_factory_|. Created
  // on the UI thread, destroyed on the IO thread.
  std::unique_ptr<NetworkContextOwner> network_context_owner_;

  // The URLDataManagerIOSBackend instance associated with this BrowserState.
  // Created and destroyed on the IO thread, and should be accessed only from
  // the IO thread.
  URLDataManagerIOSBackend* url_data_manager_ios_backend_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSER_STATE_H_
