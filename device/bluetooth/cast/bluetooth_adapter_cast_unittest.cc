// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_adapter_cast.h"

#include "base/bind_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothAdapterCastTest : public testing::Test {
 public:
  BluetoothAdapterCastTest() = default;
  ~BluetoothAdapterCastTest() override {
    BluetoothAdapterCast::ResetFactoryForTest();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterCastTest);
};

TEST_F(BluetoothAdapterCastTest, TestSetFactory) {
  // Test that the callback set with SetFactory() is called by Create().
  base::MockCallback<BluetoothAdapterCast::FactoryCb> callback;
  BluetoothAdapterCast::SetFactory(callback.Get());

  // Call the method once.
  EXPECT_CALL(callback, Run());
  BluetoothAdapterCast::Create(base::DoNothing());

  // Call it again.
  EXPECT_CALL(callback, Run());
  BluetoothAdapterCast::Create(base::DoNothing());
}

#if DCHECK_IS_ON()
TEST_F(BluetoothAdapterCastTest, TestSetFactoryTwiceCrashes) {
  // Test that calling SetFactory() more than once causes a crash.
  base::MockCallback<BluetoothAdapterCast::FactoryCb> callback;
  BluetoothAdapterCast::SetFactory(callback.Get());

  // The factory has already been set. Crash.
  EXPECT_DCHECK_DEATH(BluetoothAdapterCast::SetFactory(callback.Get()));
}

TEST_F(BluetoothAdapterCastTest, TestNoSetFactoryCrashes) {
  // Test that calling BluetoothAdapterCast::Create() without calling
  // SetFactory() causes a crash.
  EXPECT_DCHECK_DEATH(BluetoothAdapterCast::Create(base::DoNothing()));
}
#endif  // DCHECK_IS_ON()

}  // namespace device
