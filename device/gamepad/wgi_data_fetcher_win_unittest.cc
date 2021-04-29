// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/wgi_data_fetcher_win.h"

#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "device/gamepad/gamepad_provider.h"
#include "device/gamepad/test_support/fake_igamepad.h"
#include "device/gamepad/test_support/fake_igamepad_statics.h"
#include "device/gamepad/test_support/fake_ro_get_activation_factory.h"
#include "services/device/device_service_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class WgiDataFetcherWinTest : public DeviceServiceTestBase {
 public:
  WgiDataFetcherWinTest() = default;
  ~WgiDataFetcherWinTest() override = default;

  void SetUp() override {
    // Windows.Gaming.Input is available in Windows 10.0.10240.0 and later.
    if (base::win::GetVersion() < base::win::Version::WIN10)
      GTEST_SKIP();
    DeviceServiceTestBase::SetUp();
  }

  void SetUpTestEnv(
      WgiDataFetcherWin::GetActivationFactoryFunction activation_factory) {
    EXPECT_TRUE(base::win::ScopedHString::ResolveCoreWinRTStringDelayload());
    auto fetcher = std::make_unique<WgiDataFetcherWin>();
    fetcher->SetGetActivationFunctionForTesting(activation_factory);
    fetcher_ = fetcher.get();

    // Initialize provider to retrieve pad state.
    auto polling_thread = std::make_unique<base::Thread>("polling thread");
    polling_thread_ = polling_thread.get();
    provider_ = std::make_unique<GamepadProvider>(
        /*connection_change_client=*/nullptr, std::move(fetcher),
        std::move(polling_thread));

    RunUntilIdle();
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
    polling_thread_->FlushForTesting();
  }

  WgiDataFetcherWin& fetcher() const { return *fetcher_; }

 private:
  WgiDataFetcherWin* fetcher_;
  base::Thread* polling_thread_;
  std::unique_ptr<GamepadProvider> provider_;
};

TEST_F(WgiDataFetcherWinTest, AddAndRemoveWgiGamepad) {
  SetUpTestEnv(&FakeRoGetActivationFactory);

  // Check initial number of connected gamepad and WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            device::WgiDataFetcherWin::InitializationState::kInitialized);
  EXPECT_TRUE(fetcher().GetGamepadsForTesting().empty());

  const auto fake_gamepad = Microsoft::WRL::Make<FakeIGamepad>();
  auto* fake_gamepad_statics = FakeIGamepadStatics::GetInstance();

  // Check that the event handlers were added.
  EXPECT_EQ(fake_gamepad_statics->GetGamepadAddedEventHandlerCount(), 1u);
  EXPECT_EQ(fake_gamepad_statics->GetGamepadRemovedEventHandlerCount(), 1u);

  // Add a simulated WGI device.
  fake_gamepad_statics->SimulateGamepadAdded(fake_gamepad);

  // Check size of connected gamepad list and ensure gamepad state
  // is initialized correctly.
  EXPECT_EQ(fetcher().GetGamepadsForTesting().size(), 1u);
  PadState* state = fetcher().GetPadState(
      fetcher().GetGamepadsForTesting().front().source_id);
  EXPECT_TRUE(state->is_initialized);
  Gamepad& pad = state->data;
  EXPECT_TRUE(pad.connected);
  EXPECT_FALSE(pad.vibration_actuator.not_null);

  // Remove the device.
  fake_gamepad_statics->SimulateGamepadRemoved(fake_gamepad);

  // Check that the device was removed.
  EXPECT_TRUE(fetcher().GetGamepadsForTesting().empty());
}

TEST_F(WgiDataFetcherWinTest, VerifyGamepadAddedErrorHandling) {
  FakeIGamepadStatics* fake_gamepad_statics =
      FakeIGamepadStatics::GetInstance();

  // Let fake gamepad statics add_GamepadAdded return failure code to
  // test error handling.
  fake_gamepad_statics->SetAddGamepadAddedStatus(E_FAIL);

  SetUpTestEnv(&FakeRoGetActivationFactory);

  // Check WGI initialization status.
  EXPECT_EQ(
      fetcher().GetInitializationState(),
      device::WgiDataFetcherWin::InitializationState::kAddGamepadAddedFailed);
  fake_gamepad_statics->SetAddGamepadAddedStatus(S_OK);
}

TEST_F(WgiDataFetcherWinTest, VerifyGamepadRemovedErrorHandling) {
  FakeIGamepadStatics* fake_gamepad_statics =
      FakeIGamepadStatics::GetInstance();

  // Let fake gamepad statics add_GamepadRemoved return failure code to
  // test error handling.
  fake_gamepad_statics->SetAddGamepadRemovedStatus(E_FAIL);

  SetUpTestEnv(&FakeRoGetActivationFactory);

  // Check WGI initialization status.
  EXPECT_EQ(
      fetcher().GetInitializationState(),
      device::WgiDataFetcherWin::InitializationState::kAddGamepadRemovedFailed);
  fake_gamepad_statics->SetAddGamepadRemovedStatus(S_OK);
}

TEST_F(WgiDataFetcherWinTest, VerifyRoGetActivationFactoryErrorHandling) {
  // Let fake RoGetActivationFactory return failure code to
  // test error handling.
  SetUpTestEnv(&FakeRoGetActivationFactoryToTestErrorHandling);

  // Check WGI initialization status.
  EXPECT_EQ(fetcher().GetInitializationState(),
            device::WgiDataFetcherWin::InitializationState::
                kRoGetActivationFactoryFailed);
}

}  // namespace device
