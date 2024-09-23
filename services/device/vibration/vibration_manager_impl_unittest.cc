// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/vibration/vibration_manager_impl.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

using ::base::test::RunClosure;

namespace {

class VibrationManagerImplTest : public DeviceServiceTestBase,
                                 public mojom::VibrationManagerListener {
 public:
  VibrationManagerImplTest() = default;

  VibrationManagerImplTest(const VibrationManagerImplTest&) = delete;
  VibrationManagerImplTest& operator=(const VibrationManagerImplTest&) = delete;

  ~VibrationManagerImplTest() override = default;

 protected:
  void SetUp() override {
    DeviceServiceTestBase::SetUp();

    mojo::PendingRemote<device::mojom::VibrationManagerListener> remote;
    listener_.Bind(remote.InitWithNewPipeAndPassReceiver());
    device_service()->BindVibrationManager(
        vibration_manager_.BindNewPipeAndPassReceiver(), std::move(remote));
  }

  void Vibrate(int64_t milliseconds) {
    base::RunLoop run_loop;
    vibration_manager_->Vibrate(milliseconds, run_loop.QuitClosure());
    run_loop.Run();
  }

  void Cancel() {
    base::RunLoop run_loop;
    vibration_manager_->Cancel(run_loop.QuitClosure());
    run_loop.Run();
  }

  int64_t GetVibrationMilliSeconds() {
    return VibrationManagerImpl::milli_seconds_for_testing_;
  }

  bool GetVibrationCancelled() {
    return VibrationManagerImpl::cancelled_for_testing_;
  }

  MOCK_METHOD(void, OnVibrate, (), (override));

 private:
  mojo::Remote<mojom::VibrationManager> vibration_manager_;
  mojo::Receiver<mojom::VibrationManagerListener> listener_{this};
};

TEST_F(VibrationManagerImplTest, VibrateThenCancel) {
  EXPECT_NE(10000, GetVibrationMilliSeconds());
  Vibrate(10000);
  EXPECT_EQ(10000, GetVibrationMilliSeconds());

  EXPECT_FALSE(GetVibrationCancelled());
  Cancel();
  EXPECT_TRUE(GetVibrationCancelled());
}

TEST_F(VibrationManagerImplTest, VibrateNotifiesListener) {
  base::RunLoop loop;
  EXPECT_CALL(*this, OnVibrate)
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  Vibrate(10000);
  loop.Run();
}

}  // namespace

}  // namespace device
