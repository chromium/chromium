// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSER_STATE_H_
#define IOS_WEB_PUBLIC_BROWSER_STATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

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

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace web {
class CertificatePolicyCache;
class NetworkContextOwner;
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

  // Returns a NetworkContext that is backed by GetRequestContext.
  network::mojom::NetworkContext* GetNetworkContext();

  // Returns an provider to create ProtoDatabase tied to the profile directory.
  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProvider();

  // Binds a ProxyResolvingSocketFactory receiver to NetworkContext.
  void GetProxyResolvingSocketFactory(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver);

  // Like URLLoaderFactory, but wrapped inside SharedURLLoaderFactory
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSharedURLLoaderFactory();

  // Safely cast a base::SupportsUserData to a BrowserState. Returns nullptr
  // if `supports_user_data` is not a BrowserState.
  static BrowserState* FromSupportsUserData(
      base::SupportsUserData* supports_user_data);

  // Updates `cors_exempt_header_list` field of the given `param` to register
  // headers that are used in content for special purpose and should not be
  // blocked by CORS checks.
  virtual void UpdateCorsExemptHeader(
      network::mojom::NetworkContextParams* params) {}

  // Returns the identifier used to access the WebKit storage for
  // the WebState attached to this BrowserState. Use the default data store if
  // the string is empty.
  virtual const std::string& GetWebKitStorageID() const;

 protected:
  BrowserState();

 private:
  friend class URLDataManagerIOS;
  friend class URLRequestChromeJob;

  // Returns the URLDataManagerIOSBackend instance associated with this
  // BrowserState, creating it if necessary. Should only be called on the IO
  // thread.
  // Not intended for usage outside of //web.
  URLDataManagerIOSBackend* GetURLDataManagerIOSBackendOnIOThread();

  void CreateNetworkContextIfNecessary();

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider>
      proto_database_provider_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;

  // Owns the network::NetworkContext that backs `url_loader_factory_`. Created
  // on the UI thread, destroyed on the IO thread.
  std::unique_ptr<NetworkContextOwner> network_context_owner_;

  // The URLDataManagerIOSBackend instance associated with this BrowserState.
  // Created and destroyed on the IO thread, and should be accessed only from
  // the IO thread.
  raw_ptr<URLDataManagerIOSBackend> url_data_manager_ios_backend_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSER_STATE_H_
