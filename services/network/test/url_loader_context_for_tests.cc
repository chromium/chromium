// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/url_loader_context_for_tests.h"

namespace network {

URLLoaderContextForTests::URLLoaderContextForTests() = default;
URLLoaderContextForTests::~URLLoaderContextForTests() = default;

bool URLLoaderContextForTests::ShouldRequireIsolationInfo() const {
  return false;
}

const cors::OriginAccessList& URLLoaderContextForTests::GetOriginAccessList()
    const {
  return origin_access_list_;
}

const mojom::URLLoaderFactoryParams&
URLLoaderContextForTests::GetFactoryParams() const {
  return factory_params_;
}

mojom::CrossOriginEmbedderPolicyReporter*
URLLoaderContextForTests::GetCoepReporter() const {
  return nullptr;
}

mojom::DocumentIsolationPolicyReporter*
URLLoaderContextForTests::GetDipReporter() const {
  return nullptr;
}

scoped_refptr<RefCountedDeviceBoundSessionAccessObserverRemote>
URLLoaderContextForTests::GetDeviceBoundSessionAccessObserverSharedRemote()
    const {
  return nullptr;
}

mojom::NetworkContextClient* URLLoaderContextForTests::GetNetworkContextClient()
    const {
  return network_context_client_;
}

mojom::TrustedURLLoaderHeaderClient*
URLLoaderContextForTests::GetUrlLoaderHeaderClient() const {
  return nullptr;
}

net::URLRequestContext* URLLoaderContextForTests::GetUrlRequestContext() const {
  return url_request_context_;
}

scoped_refptr<ResourceSchedulerClient>
URLLoaderContextForTests::GetResourceSchedulerClient() const {
  return resource_scheduler_client_;
}

orb::PerFactoryState& URLLoaderContextForTests::GetMutableOrbState() {
  return orb_state_;
}

bool URLLoaderContextForTests::DataUseUpdatesEnabled() {
  return false;
}

}  // namespace network
