// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_TEST_BASE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_TEST_BASE_H_

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class DeviceService;

const char kTestGeolocationApiKey[] = "FakeApiKeyForTest";

// Base class responsible to setup Device Service for test.
class DeviceServiceTestBase : public testing::Test {
 public:
  DeviceServiceTestBase();
  ~DeviceServiceTestBase() override;

  // NOTE: It's important to do service instantiation within SetUp instead of
  // the constructor, as subclasses of this fixture may need to initialize some
  // global state before the service is constructed.
  void SetUp() override;

 protected:
  service_manager::Connector* connector() { return connector_.get(); }
  DeviceService* device_service() { return service_.get(); }

  // Can optionally be called to destroy the service before a child test fixture
  // shuts down, in case the DeviceService has dependencies on objects created
  // by the child test fixture.
  void DestroyDeviceService();

  base::test::TaskEnvironment task_environment_;

  // Both of these task runners should be deprecated in favor of individual
  // components of the device service creating their own.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  service_manager::TestConnectorFactory test_connector_factory_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<DeviceService> service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceServiceTestBase);
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_TEST_BASE_H_
