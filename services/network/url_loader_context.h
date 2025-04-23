// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_LOADER_CONTEXT_H_
#define SERVICES_NETWORK_URL_LOADER_CONTEXT_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/mojom/device_bound_sessions.mojom-forward.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace network {

class ResourceSchedulerClient;

namespace cors {
class OriginAccessList;
}  // namespace cors

namespace mojom {
class CrossOriginEmbedderPolicyReporter;
class NetworkContextClient;
class TrustedURLLoaderHeaderClient;
class URLLoaderFactoryParams;
}  // namespace mojom

using RefCountedDeviceBoundSessionAccessObserverRemote = base::RefCountedData<
    mojo::Remote<network::mojom::DeviceBoundSessionAccessObserver>>;

// An interface implemented in production code by network::URLLoaderFactory (or
// by URLLoaderContextForTests in unit tests).
class COMPONENT_EXPORT(NETWORK_SERVICE) URLLoaderContext {
 public:
  virtual bool ShouldRequireIsolationInfo() const = 0;
  virtual const cors::OriginAccessList& GetOriginAccessList() const = 0;
  virtual const mojom::URLLoaderFactoryParams& GetFactoryParams() const = 0;
  virtual mojom::CrossOriginEmbedderPolicyReporter* GetCoepReporter() const = 0;
  virtual mojom::DocumentIsolationPolicyReporter* GetDipReporter() const = 0;
  virtual mojom::NetworkContextClient* GetNetworkContextClient() const = 0;
  virtual mojom::TrustedURLLoaderHeaderClient* GetUrlLoaderHeaderClient()
      const = 0;
  virtual net::URLRequestContext* GetUrlRequestContext() const = 0;
  virtual scoped_refptr<ResourceSchedulerClient> GetResourceSchedulerClient()
      const = 0;
  virtual orb::PerFactoryState& GetMutableOrbState() = 0;
  virtual bool DataUseUpdatesEnabled() = 0;
  virtual scoped_refptr<RefCountedDeviceBoundSessionAccessObserverRemote>
  GetDeviceBoundSessionAccessObserverSharedRemote() const = 0;

 protected:
  // `protected` destructor = can only destruct via concrete implementations
  // (and not via an URLLoaderContext pointer).
  virtual ~URLLoaderContext() {}
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_LOADER_CONTEXT_H_
