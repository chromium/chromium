// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "services/device/vibration/android/vibration_jni_headers/VibrationManagerImpl_jni.h"
#else
#include "services/device/vibration/vibration_manager_impl.h"
#endif

namespace device {

namespace {

class VibrationManagerImplTest : public DeviceServiceTestBase {
 public:
  VibrationManagerImplTest() = default;
  ~VibrationManagerImplTest() override = default;

 protected:
  void SetUp() override {
    DeviceServiceTestBase::SetUp();

    connector()->Connect(mojom::kServiceName,
                         vibration_manager_.BindNewPipeAndPassReceiver());
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
#if defined(OS_ANDROID)
    return Java_VibrationManagerImpl_getVibrateMilliSecondsForTesting(
        base::android::AttachCurrentThread());
#else
    return VibrationManagerImpl::milli_seconds_for_testing_;
#endif
  }

  bool GetVibrationCancelled() {
#if defined(OS_ANDROID)
    return Java_VibrationManagerImpl_getVibrateCancelledForTesting(
        base::android::AttachCurrentThread());
#else
    return VibrationManagerImpl::cancelled_for_testing_;
#endif
  }

 private:
  mojo::Remote<mojom::VibrationManager> vibration_manager_;

  DISALLOW_COPY_AND_ASSIGN(VibrationManagerImplTest);
};

TEST_F(VibrationManagerImplTest, VibrateThenCancel) {
  EXPECT_NE(10000, GetVibrationMilliSeconds());
  Vibrate(10000);
  EXPECT_EQ(10000, GetVibrationMilliSeconds());

  EXPECT_FALSE(GetVibrationCancelled());
  Cancel();
  EXPECT_TRUE(GetVibrationCancelled());
}

}  // namespace

}  // namespace device
