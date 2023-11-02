// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/advertisement.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeBluetoothAdvertisement : public device::BluetoothAdvertisement {
 public:
  // device::BluetoothAdvertisement:
  void Unregister(SuccessCallback success_callback,
                  ErrorCallback error_callback) override {
    called_unregister_ = true;
    std::move(success_callback).Run();
  }

  bool called_unregister() { return called_unregister_; }

 private:
  ~FakeBluetoothAdvertisement() override = default;

  bool called_unregister_ = false;
};

}  // namespace

namespace bluetooth {

class AdvertisementTest : public testing::Test {
 public:
  AdvertisementTest() = default;
  ~AdvertisementTest() override = default;
  AdvertisementTest(const AdvertisementTest&) = delete;
  AdvertisementTest& operator=(const AdvertisementTest&) = delete;

  void SetUp() override {
    fake_bluetooth_advertisement_ =
        base::MakeRefCounted<FakeBluetoothAdvertisement>();
    advertisement_ =
        std::make_unique<Advertisement>(fake_bluetooth_advertisement_);
  }

 protected:
  scoped_refptr<FakeBluetoothAdvertisement> fake_bluetooth_advertisement_;
  std::unique_ptr<Advertisement> advertisement_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AdvertisementTest, TestOnDestroyCallsUnregister) {
  // When destroyed, |advertisement_| is expected to tear down its
  // BluetoothAdvertisement.
  ASSERT_FALSE(fake_bluetooth_advertisement_->called_unregister());
  advertisement_.reset();
  EXPECT_TRUE(fake_bluetooth_advertisement_->called_unregister());
}

TEST_F(AdvertisementTest, TestUnregister) {
  ASSERT_FALSE(fake_bluetooth_advertisement_->called_unregister());
  base::RunLoop run_loop;
  advertisement_->Unregister(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(fake_bluetooth_advertisement_->called_unregister());
}

}  // namespace bluetooth
