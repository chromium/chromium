// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_URL_LOADER_CONTEXT_FOR_TESTS_H_
#define SERVICES_NETWORK_TEST_URL_LOADER_CONTEXT_FOR_TESTS_H_

#include "base/memory/raw_ptr.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/url_loader_context.h"

namespace network {

class URLLoaderContextForTests : public URLLoaderContext {
 public:
  URLLoaderContextForTests();
  ~URLLoaderContextForTests() override;

  URLLoaderContextForTests(const URLLoaderContextForTests&) = delete;
  URLLoaderContextForTests& operator=(const URLLoaderContextForTests&) = delete;

  void Detach() {
    network_context_client_ = nullptr;
    url_request_context_ = nullptr;
    resource_scheduler_client_ = nullptr;
  }

  // Accessors to let tests configure some aspects of `this` object.
  mojom::URLLoaderFactoryParams& mutable_factory_params() {
    return factory_params_;
  }
  void set_network_context_client(mojom::NetworkContextClient* new_value) {
    network_context_client_ = new_value;
  }
  void set_url_request_context(net::URLRequestContext* new_value) {
    url_request_context_ = new_value;
  }
  void set_resource_scheduler_client(
      const scoped_refptr<ResourceSchedulerClient>& new_value) {
    resource_scheduler_client_ = new_value;
  }

  // URLLoaderContext implementation.
  bool ShouldRequireIsolationInfo() const override;
  const cors::OriginAccessList& GetOriginAccessList() const override;
  const mojom::URLLoaderFactoryParams& GetFactoryParams() const override;
  mojom::CookieAccessObserver* GetCookieAccessObserver() const override;
  mojom::TrustTokenAccessObserver* GetTrustTokenAccessObserver() const override;
  mojom::CrossOriginEmbedderPolicyReporter* GetCoepReporter() const override;
  mojom::DevToolsObserver* GetDevToolsObserver() const override;
  mojom::NetworkContextClient* GetNetworkContextClient() const override;
  mojom::TrustedURLLoaderHeaderClient* GetUrlLoaderHeaderClient()
      const override;
  mojom::URLLoaderNetworkServiceObserver* GetURLLoaderNetworkServiceObserver()
      const override;
  net::URLRequestContext* GetUrlRequestContext() const override;
  scoped_refptr<ResourceSchedulerClient> GetResourceSchedulerClient()
      const override;
  orb::PerFactoryState& GetMutableOrbState() override;
  bool DataUseUpdatesEnabled() override;

 private:
  mojom::URLLoaderFactoryParams factory_params_;
  cors::OriginAccessList origin_access_list_;
  orb::PerFactoryState orb_state_;

  raw_ptr<mojom::NetworkContextClient> network_context_client_ = nullptr;
  raw_ptr<net::URLRequestContext> url_request_context_ = nullptr;
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_URL_LOADER_CONTEXT_FOR_TESTS_H_
