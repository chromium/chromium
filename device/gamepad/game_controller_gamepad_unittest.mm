// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/game_controller_gamepad.h"

#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>

#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ocmock/OCMock/OCMock.h"

namespace device {
namespace {

class GameControllerGamepadTest : public testing::Test {
 public:
  GameControllerGamepadTest() = default;
  GameControllerGamepadTest(const GameControllerGamepadTest&) = delete;
  GameControllerGamepadTest& operator=(const GameControllerGamepadTest&) =
      delete;
  ~GameControllerGamepadTest() override = default;

  void TearDown() override { gamepad_.Shutdown(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  GameControllerGamepad gamepad_{nil};
};

// Regression test for crbug.com/505481701: CoreHaptics throws NSException when
// the CHHapticEngine has been stopped by the OS (e.g. controller disconnect).
TEST_F(GameControllerGamepadTest,
       SetVibrationDoesNotCrashWhenPlayerThrowsException) {
  // Create a mock CHHapticEngine that won't crash on basic queries.
  id mock_engine = OCMClassMock([CHHapticEngine class]);

  // Create a mock CHHapticPatternPlayer that throws NSException on
  // sendParameters:atTime:error: — simulating what happens when the engine
  // was stopped externally by macOS due to controller disconnect.
  id mock_player = OCMProtocolMock(@protocol(CHHapticPatternPlayer));
  NSException* haptic_exception = [NSException
      exceptionWithName:NSInternalInconsistencyException
                 reason:@"The haptic engine is not in a valid state"
               userInfo:nil];
  OCMStub([mock_player sendParameters:[OCMArg any]
                               atTime:0
                                error:[OCMArg anyObjectRef]])
      .andThrow(haptic_exception);
  OCMStub([mock_player stopAtTime:0 error:[OCMArg anyObjectRef]])
      .andThrow(haptic_exception);

  // Inject the mock engine and player.
  gamepad_.SetDefaultHapticEngineForTesting(mock_engine);
  gamepad_.SetDefaultHapticPlayerForTesting(mock_player);
  gamepad_.SetHapticsStartedForTesting(true);

  // Call SetVibration and trigger the haptic engine exception.
  auto params = mojom::GamepadEffectParameters::New();
  params->strong_magnitude = 0.8;
  params->weak_magnitude = 0.5;
  gamepad_.SetVibration(std::move(params));

  // If we reach here, the exception was caught. Test passes.
}

// Test that setting zero vibration with a throwing player also doesn't crash.
TEST_F(GameControllerGamepadTest,
       SetZeroVibrationDoesNotCrashWhenPlayerThrowsException) {
  id mock_engine = OCMClassMock([CHHapticEngine class]);
  id mock_player = OCMProtocolMock(@protocol(CHHapticPatternPlayer));
  NSException* haptic_exception = [NSException
      exceptionWithName:NSInternalInconsistencyException
                 reason:@"The haptic engine is not in a valid state"
               userInfo:nil];
  OCMStub([mock_player stopAtTime:0 error:[OCMArg anyObjectRef]])
      .andThrow(haptic_exception);

  gamepad_.SetDefaultHapticEngineForTesting(mock_engine);
  gamepad_.SetDefaultHapticPlayerForTesting(mock_player);
  gamepad_.SetHapticsStartedForTesting(true);

  // Setting zero vibration calls [player stopAtTime:0 error:nil].
  auto params = mojom::GamepadEffectParameters::New();
  params->strong_magnitude = 0.0;
  params->weak_magnitude = 0.0;
  gamepad_.SetVibration(std::move(params));
}

// Test that DoShutdown doesn't crash when the engine throws.
TEST_F(GameControllerGamepadTest,
       ShutdownDoesNotCrashWhenEngineThrowsException) {
  id mock_engine = OCMClassMock([CHHapticEngine class]);
  NSException* haptic_exception = [NSException
      exceptionWithName:NSInternalInconsistencyException
                 reason:@"The haptic engine is not in a valid state"
               userInfo:nil];
  OCMStub([mock_engine stopWithCompletionHandler:[OCMArg any]])
      .andThrow(haptic_exception);

  gamepad_.SetDefaultHapticEngineForTesting(mock_engine);
  gamepad_.SetHapticsStartedForTesting(true);

  // Shutdown calls [engine stopWithCompletionHandler:nil] which throws.
  // The test's TearDown() calls Shutdown(), which will exercise this path.
  // If we reach here without crashing, the fix works.
}

// Test that StartHaptics doesn't crash when the engine throws on start.
TEST_F(GameControllerGamepadTest,
       StartHapticsDoesNotCrashWhenEngineThrowsException) {
  id mock_engine = OCMClassMock([CHHapticEngine class]);
  NSException* haptic_exception = [NSException
      exceptionWithName:NSInternalInconsistencyException
                 reason:@"The haptic engine is not in a valid state"
               userInfo:nil];
  OCMStub([mock_engine startAndReturnError:[OCMArg anyObjectRef]])
      .andThrow(haptic_exception);

  gamepad_.SetDefaultHapticEngineForTesting(mock_engine);

  // StartHaptics calls [engine startAndReturnError:] which throws.
  gamepad_.StartHaptics();

  // Verify that haptics_started_ was set to false after the exception.
  EXPECT_FALSE(gamepad_.GetHapticsStartedForTesting());
}

}  // namespace
}  // namespace device
