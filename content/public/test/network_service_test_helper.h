// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NETWORK_SERVICE_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_NETWORK_SERVICE_TEST_HELPER_H_

#include <memory>

#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {

// Used by testing environments to inject test-only interface binders into an
// embedded network service instance. Test suites should create a long-lived
// instance of this class.
class NetworkServiceTestHelper {
 public:
  // Returns the instance if the current process is running the network service
  // and call this function first.
  // If this returns an instance, it is ready to fulfill NetworkServiceTest
  // mojo interface requests within the network service.
  static std::unique_ptr<NetworkServiceTestHelper> Create();
  NetworkServiceTestHelper(const NetworkServiceTestHelper&) = delete;
  NetworkServiceTestHelper& operator=(const NetworkServiceTestHelper&) = delete;

  ~NetworkServiceTestHelper();

 private:
  class NetworkServiceTestImpl;

  NetworkServiceTestHelper();

  // Registers the helper's interfaces on |registry|. Note that this object
  // must outlive |registry|.
  void RegisterNetworkBinders(service_manager::BinderRegistry* registry);

  std::unique_ptr<NetworkServiceTestImpl> network_service_test_impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NETWORK_SERVICE_TEST_HELPER_H_
