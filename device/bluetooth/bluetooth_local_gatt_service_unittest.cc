// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_service.h"

#include <algorithm>
#include <iterator>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/test/bluetooth_gatt_server_test.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothLocalGattServiceTest : public BluetoothGattServerTest {
 public:
  void SetUp() override {
    BluetoothGattServerTest::SetUp();
  }

  bool ServiceSetsEqual(
      std::vector<BluetoothLocalGattService*> services,
      std::initializer_list<BluetoothLocalGattService*> services_to_check) {
    using ServiceSet = std::set<BluetoothLocalGattService*,
                                bool (*)(BluetoothLocalGattService*,
                                         BluetoothLocalGattService*)>;
    auto comparator = [](BluetoothLocalGattService* a,
                         BluetoothLocalGattService* b) -> bool {
      return a->GetIdentifier() < b->GetIdentifier();
    };

    return ServiceSet(services.begin(), services.end(), comparator) ==
           ServiceSet(services_to_check, comparator);
  }

 protected:
  base::WeakPtr<BluetoothLocalGattCharacteristic> read_characteristic_;
  base::WeakPtr<BluetoothLocalGattCharacteristic> write_characteristic_;
};

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_RegisterMultipleServices RegisterMultipleServices
#else
#define MAYBE_RegisterMultipleServices DISABLED_RegisterMultipleServices
#endif
TEST_F(BluetoothLocalGattServiceTest, MAYBE_RegisterMultipleServices) {
  base::WeakPtr<BluetoothLocalGattService> service2 =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, delegate_.get());
  base::WeakPtr<BluetoothLocalGattService> service3 =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, delegate_.get());
  base::WeakPtr<BluetoothLocalGattService> service4 =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, delegate_.get());
  service2->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service2.get()}));

  service3->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(),
                               {service2.get(), service3.get()}));

  service2->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service3.get()}));

  service4->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(),
                               {service3.get(), service4.get()}));

  service3->Register(GetCallback(Call::NOT_EXPECTED),
                     GetGattErrorCallback(Call::EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(),
                               {service3.get(), service4.get()}));

  service3->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service4.get()}));

  service4->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {}));
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_DeleteServices DeleteServices
#else
#define MAYBE_DeleteServices DISABLED_DeleteServices
#endif
TEST_F(BluetoothLocalGattServiceTest, MAYBE_DeleteServices) {
  base::WeakPtr<BluetoothLocalGattService> service2 =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, delegate_.get());
  base::WeakPtr<BluetoothLocalGattService> service3 =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, delegate_.get());
  service2->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service2.get()}));

  service3->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(),
                               {service2.get(), service3.get()}));

  service2->Unregister(GetCallback(Call::EXPECTED),
                       GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service3.get()}));

  service2->Delete();
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service3.get()}));

  service3->Delete();
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {}));

  // Create a service, register and then delete it, just to check everything
  // still works.
  base::WeakPtr<BluetoothLocalGattService> service4 =
      BluetoothLocalGattService::Create(
          adapter_.get(), BluetoothUUID(kTestUUIDGenericAttribute), true,
          nullptr, delegate_.get());
  service4->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {service4.get()}));
  service4->Delete();
  EXPECT_TRUE(ServiceSetsEqual(RegisteredGattServices(), {}));
}

}  // namespace device
