// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/input_extra_info.h"

#include <windows.h>

#include <cstdint>
#include <thread>
#include <vector>

#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

RAWINPUT CreateMouseRawInputEvent(HANDLE device_handle, uint32_t extra_info) {
  RAWINPUT event = {};
  event.header.dwType = RIM_TYPEMOUSE;
  event.header.hDevice = device_handle;
  event.data.mouse.ulExtraInformation = extra_info;
  return event;
}

struct TouchOrPenSignatureMaskTestCase {
  const char* test_name;
  uint32_t extra_info;
  bool expected_matches;
};

class TouchOrPenSignatureMaskWinTest
    : public testing::TestWithParam<TouchOrPenSignatureMaskTestCase> {};

TEST_P(TouchOrPenSignatureMaskWinTest,
       PenSignatureInputWithLowBitsSetMatchesMask) {
  const auto& param = GetParam();
  EXPECT_EQ(IsTouchOrPenMouseInput(param.extra_info), param.expected_matches);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TouchOrPenSignatureMaskWinTest,
    testing::Values(
        TouchOrPenSignatureMaskTestCase{
            .test_name = "ExactTouchSignature",
            .extra_info = kTouchOrPenSignature,
            .expected_matches = true,
        },
        TouchOrPenSignatureMaskTestCase{
            .test_name = "TouchSignatureWithContactIdLowBits",
            .extra_info = kTouchOrPenSignature | 0x80u | 0x05u,
            .expected_matches = true,
        },
        TouchOrPenSignatureMaskTestCase{
            .test_name = "PenSignatureWithAllLowBitsSet",
            .extra_info = kTouchOrPenSignature | 0xFFu,
            .expected_matches = true,
        },
        TouchOrPenSignatureMaskTestCase{
            .test_name = "CrdExtraInfoDoesNotMatchTouchSignature",
            .extra_info = kCrdInputExtraInfo,
            .expected_matches = false,
        },
        TouchOrPenSignatureMaskTestCase{
            .test_name = "ZeroExtraInfoDoesNotMatchTouchSignature",
            .extra_info = 0u,
            .expected_matches = false,
        }),
    [](const testing::TestParamInfo<TouchOrPenSignatureMaskWinTest::ParamType>&
           info) { return info.param.test_name; });

class InputExtraInfoWinTest : public testing::Test {
 protected:
  void SetUp() override { ClearRemoteTouchInjectionActivity(); }
  void TearDown() override { ClearRemoteTouchInjectionActivity(); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(InputExtraInfoWinTest,
       RecordRemoteTouchInjectionActivityFalseWhenIdleDoesNotStartTailWindow) {
  EXPECT_FALSE(IsRemoteTouchInjectionActive());
  RecordRemoteTouchInjectionActivity(/*has_active_touches=*/false);
  EXPECT_FALSE(IsRemoteTouchInjectionActive());
}

TEST_F(InputExtraInfoWinTest,
       ClearRemoteTouchInjectionActivityResetsActiveAndTailWindowImmediately) {
  RecordRemoteTouchInjectionActivity(/*has_active_touches=*/true);
  EXPECT_TRUE(IsRemoteTouchInjectionActive());
  ClearRemoteTouchInjectionActivity();
  EXPECT_FALSE(IsRemoteTouchInjectionActive());

  RecordRemoteTouchInjectionActivity(/*has_active_touches=*/true);
  RecordRemoteTouchInjectionActivity(/*has_active_touches=*/false);
  EXPECT_TRUE(IsRemoteTouchInjectionActive());
  ClearRemoteTouchInjectionActivity();
  EXPECT_FALSE(IsRemoteTouchInjectionActive());
}

TEST_F(InputExtraInfoWinTest,
       TouchSignatureInputWhenRemoteTouchIdleIsTreatedAsLocalInput) {
  EXPECT_FALSE(IsRemoteTouchInjectionActive());

  RAWINPUT touch_event =
      CreateMouseRawInputEvent(nullptr, kTouchOrPenSignature | 0x80u | 0x01u);
  EXPECT_FALSE(IsCrdInjectedInput(touch_event));
}

TEST_F(InputExtraInfoWinTest,
       TouchSignatureInputDuringActiveRemoteTouchIsTreatedAsCrdInjected) {
  RecordRemoteTouchInjectionActivity(/*has_active_touches=*/true);
  EXPECT_TRUE(IsRemoteTouchInjectionActive());

  RAWINPUT touch_event =
      CreateMouseRawInputEvent(nullptr, kTouchOrPenSignature | 0x80u | 0x02u);
  EXPECT_TRUE(IsCrdInjectedInput(touch_event));

  // Even if significant time elapses while remote touch points remain in
  // contact, touch-promoted mouse input continues to be attributed to CRD.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(IsRemoteTouchInjectionActive());
  EXPECT_TRUE(IsCrdInjectedInput(touch_event));
}

TEST_F(InputExtraInfoWinTest,
       TouchSignatureInputWithinTailWindowAfterTouchEndIsTreatedAsCrdInjected) {
  RecordRemoteTouchInjectionActivity(/*has_active_touches=*/true);
  RecordRemoteTouchInjectionActivity(/*has_active_touches=*/false);

  RAWINPUT touch_event =
      CreateMouseRawInputEvent(nullptr, kTouchOrPenSignature | 0x03u);
  EXPECT_TRUE(IsRemoteTouchInjectionActive());
  EXPECT_TRUE(IsCrdInjectedInput(touch_event));

  task_environment_.FastForwardBy(kRemoteTouchTailWindow -
                                  base::Milliseconds(1));
  EXPECT_TRUE(IsRemoteTouchInjectionActive());
  EXPECT_TRUE(IsCrdInjectedInput(touch_event));
}

TEST_F(InputExtraInfoWinTest,
       TouchSignatureInputAfterTailWindowExpiresIsTreatedAsLocalInput) {
  RecordRemoteTouchInjectionActivity(/*has_active_touches=*/true);
  RecordRemoteTouchInjectionActivity(/*has_active_touches=*/false);

  RAWINPUT touch_event =
      CreateMouseRawInputEvent(nullptr, kTouchOrPenSignature | 0x80u | 0x04u);
  EXPECT_TRUE(IsCrdInjectedInput(touch_event));

  task_environment_.FastForwardBy(kRemoteTouchTailWindow +
                                  base::Milliseconds(1));
  EXPECT_FALSE(IsRemoteTouchInjectionActive());
  EXPECT_FALSE(IsCrdInjectedInput(touch_event));
}

TEST_F(InputExtraInfoWinTest,
       ConcurrentRemoteTouchActivityRecordingAndCheckingIsThreadSafe) {
  base::WaitableEvent start_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  constexpr int kIterations = 1000;

  std::thread writer([&]() {
    start_event.Wait();
    for (int i = 0; i < kIterations; ++i) {
      RecordRemoteTouchInjectionActivity(/*has_active_touches=*/true);
      RecordRemoteTouchInjectionActivity(/*has_active_touches=*/false);
    }
  });

  std::vector<std::thread> readers;
  readers.reserve(2);
  for (int t = 0; t < 2; ++t) {
    readers.emplace_back([&]() {
      RAWINPUT touch_event =
          CreateMouseRawInputEvent(nullptr, kTouchOrPenSignature | 0x80u);
      RAWINPUT crd_mouse_event =
          CreateMouseRawInputEvent(nullptr, kCrdInputExtraInfo);
      start_event.Wait();
      for (int i = 0; i < kIterations; ++i) {
        bool is_active = IsRemoteTouchInjectionActive();
        bool is_crd_touch = IsCrdInjectedInput(touch_event);
        // Non-touch CRD mouse events must always return true regardless of
        // concurrent remote touch activity state transitions.
        EXPECT_TRUE(IsCrdInjectedInput(crd_mouse_event));
        (void)is_active;
        (void)is_crd_touch;
      }
    });
  }

  start_event.Signal();
  writer.join();
  for (auto& reader : readers) {
    reader.join();
  }

  ClearRemoteTouchInjectionActivity();
  EXPECT_FALSE(IsRemoteTouchInjectionActive());
}

}  // namespace
}  // namespace remoting
