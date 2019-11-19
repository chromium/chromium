// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NETWORK_SERVICE_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_NETWORK_SERVICE_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {

// Used by testing environments to inject test-only interface binders into an
// embedded network service instance. Test suites should create a long-lived
// instance of this class and call RegisterNetworkBinders() on a BinderRegistry
// which will be used to fulfill interface requests within the network service.
class NetworkServiceTestHelper {
 public:
  NetworkServiceTestHelper();
  ~NetworkServiceTestHelper();

  // Registers the helper's interfaces on |registry|. Note that this object
  // must outlive |registry|.
  void RegisterNetworkBinders(service_manager::BinderRegistry* registry);

 private:
  class NetworkServiceTestImpl;

  void BindNetworkServiceTestReceiver(
      mojo::PendingReceiver<network::mojom::NetworkServiceTest> receiver);

  std::unique_ptr<NetworkServiceTestImpl> network_service_test_impl_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceTestHelper);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NETWORK_SERVICE_TEST_HELPER_H_
