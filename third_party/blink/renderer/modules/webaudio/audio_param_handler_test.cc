// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_param_handler.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>

#include "base/synchronization/lock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/testing/fake_audio_thread.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

TEST(AudioParamHandlerTest, UAFOnGCDerivedFromSummingBus) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 2, 1, 48000, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  OscillatorNode* osc1 = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());
  OscillatorNode* osc2 = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Connect osc1 to osc2's frequency AudioParam.
  osc1->connect(osc2->frequency(), 0, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Keep the AudioParam alive so it outlives the OscillatorNode during GC.
  Persistent<AudioParam> frequency_param = osc2->frequency();

  // Create a background thread to simulate the audio thread.
  FakeAudioThread audio_thread(ThreadType::kRealtimeAudioWorkletThread);

  audio_thread.RunOnAudioThreadWithContext(
      context,
      CrossThreadBindOnce(
          [](OfflineAudioContext* context, AudioParam* param) {
            {
              DeferredTaskHandler::GraphAutoLocker locker(
                  context->GetDeferredTaskHandler());
              context->GetDeferredTaskHandler().HandleDeferredTasks();
            }
            param->Handler().FinalValue();
          },
          WrapCrossThreadPersistent(context),
          WrapCrossThreadPersistent(frequency_param.Get())));

  // Drop references to nodes.
  osc1 = nullptr;
  osc2 = nullptr;

  // Force GC. Without the fix, frequency_param's summing_bus_ might hold a
  // dangling pointer.
  ThreadState::Current()->CollectAllGarbageForTesting();

  // Clear the AudioParam and GC again. This destroys the AudioParamHandler.
  frequency_param = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST(AudioParamHandlerTest, TimelinePruningOnDisconnectedNode) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 48000, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  OscillatorNode* osc = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());

  AudioParamHandler& param_handler = osc->frequency()->Handler();

  // We need to acquire the lock to inspect and mutate the timeline events
  // directly.
  {
    base::AutoLock locker(param_handler.events_lock_);
    EXPECT_EQ(param_handler.events_.size(), 0u);
    EXPECT_EQ(param_handler.new_events_.size(), 0u);

    // Insert E0 (past)
    param_handler.InsertEvent(
        AudioParamHandler::ParamEvent::CreateSetValueEvent(1.0, -2.0),
        exception_state);
    ASSERT_FALSE(exception_state.HadException());
    EXPECT_EQ(param_handler.events_.size(), 1u);
    EXPECT_EQ(param_handler.new_events_.size(), 1u);

    // Insert E1 (past)
    param_handler.InsertEvent(
        AudioParamHandler::ParamEvent::CreateSetValueEvent(2.0, -1.0),
        exception_state);
    ASSERT_FALSE(exception_state.HadException());
    EXPECT_EQ(param_handler.events_.size(), 2u);
    EXPECT_EQ(param_handler.new_events_.size(), 2u);

    // Insert E2 (future) -> triggers pruning of E0, keeps E1 (preceding) and
    // E2.
    param_handler.InsertEvent(
        AudioParamHandler::ParamEvent::CreateSetValueEvent(3.0, 1.0),
        exception_state);
    ASSERT_FALSE(exception_state.HadException());

    // E0 should be pruned.
    EXPECT_EQ(param_handler.events_.size(), 2u);
    EXPECT_EQ(param_handler.new_events_.size(), 2u);
  }
}

TEST(AudioParamHandlerTest, SetValueCurveWithPastStartTime) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 1024, 48000, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  const Vector<float> curve = {1.0f, 2.0f, 3.0f};

  // Case 1: A curve starts in the past but is still active during the render
  // quantum. Verify its start time is not clamped to currentTime (which would
  // shift the curve past its end event and following events) and that rendering
  // correctly offsets into the curve.
  {
    OscillatorNode* osc = context->createOscillator(exception_state);
    ASSERT_FALSE(exception_state.HadException());
    AudioParamHandler& handler = osc->frequency()->Handler();

    // Curve from t = 0 to t = 0.001 (frames 0..48 at 48kHz), then SetValue(4.0)
    // at t = 0.001.
    handler.SetValueCurveAtTime(curve, 0.0, 0.001, exception_state);
    ASSERT_FALSE(exception_state.HadException());
    handler.SetValueAtTime(4.0f, 0.001, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    // Render from frame 24 (t = 0.0005).
    std::array<float, 128> values;
    handler.ValuesForFrameRange(24, 24 + 128, 1.0f, values, 48000, 48000,
                                -3.4e38f, 3.4e38f, 128);

    // Frames 24..47 are the second half of the curve (2.0 -> ~3.0).
    EXPECT_FLOAT_EQ(values[0], 2.0f);
    EXPECT_NEAR(values[23], 2.958333f, 1e-5f);
    // Frames 48..151 are from SetValueAtTime(4.0).
    EXPECT_FLOAT_EQ(values[24], 4.0f);
    EXPECT_FLOAT_EQ(values[127], 4.0f);
  }

  // Case 2: A curve has already ended entirely in the past before the render
  // quantum. Verify the curve is skipped without timeline corruption and
  // rendering cleanly picks up subsequent events.
  {
    OscillatorNode* osc = context->createOscillator(exception_state);
    ASSERT_FALSE(exception_state.HadException());
    AudioParamHandler& handler = osc->frequency()->Handler();

    handler.SetValueCurveAtTime(curve, 0.0, 0.001, exception_state);
    ASSERT_FALSE(exception_state.HadException());
    handler.SetValueAtTime(4.0f, 0.002, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    // Render from frame 96 (t = 0.002), after the entire curve.
    std::array<float, 128> values;
    handler.ValuesForFrameRange(96, 96 + 128, 1.0f, values, 48000, 48000,
                                -3.4e38f, 3.4e38f, 128);

    EXPECT_FLOAT_EQ(values[0], 4.0f);
    EXPECT_FLOAT_EQ(values[127], 4.0f);
  }
}

TEST(AudioParamHandlerTest, KRateAutomationClamping) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 48000, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  OscillatorNode* osc = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());

  AudioParamHandler& handler = osc->frequency()->Handler();
  handler.SetAutomationRate(V8AutomationRate::Enum::kKRate);

  const float max_value = handler.MaxValue();
  const float min_value = handler.MinValue();

  FakeAudioThread audio_thread(ThreadType::kRealtimeAudioWorkletThread);
  auto verify_clamped_value = [&](float expected_value) {
    audio_thread.RunOnAudioThreadWithContext(
        context,
        CrossThreadBindOnce(
            [](AudioParam* param, float expected) {
              AudioParamHandler& h = param->Handler();
              EXPECT_FLOAT_EQ(h.FinalValue(), expected);
              EXPECT_FLOAT_EQ(h.Value(), expected);

              std::array<float, 128> values{};
              h.CalculateSampleAccurateValues(values);
              for (float v : values) {
                EXPECT_FLOAT_EQ(v, expected);
              }
            },
            WrapCrossThreadPersistent(osc->frequency()), expected_value));
  };

  // Schedule an automation value above maxValue at t = 0.
  handler.SetValueAtTime(max_value * 2.0f, 0.0, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  verify_clamped_value(max_value);

  // Schedule an automation value below minValue at t = 0 on the main thread.
  handler.SetValueAtTime(min_value * 2.0f, 0.0, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  verify_clamped_value(min_value);
}

TEST(AudioParamHandlerTest, CancelAndHoldAtTimeWithOutOfOrderAdjacentCurves) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 8000, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  OscillatorNode* osc = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Out-of-order adjacent curves place the earlier curve's end marker after
  // the later curve's start; cancelling a follow-up ramp must not crash.
  const Vector<float> curve = {1.0f, 2.0f};
  osc->frequency()
      ->setValueCurveAtTime(curve, 0.006, 0.008, exception_state)
      ->setValueCurveAtTime(curve, 0.002, 0.004, exception_state)
      ->cancelAndHoldAtTime(0.010, exception_state)
      ->linearRampToValueAtTime(5.0f, 0.015, exception_state)
      ->cancelAndHoldAtTime(0.010, exception_state);
  EXPECT_FALSE(exception_state.HadException());
}

// When a SetTarget event is followed by a ramp, the SetTarget event is replaced
// by a SetValue event at the current frame.  The time of that frame can be one
// ULP before the SetTarget event, and so before an earlier event at the same
// time.  Verify that the replacement event keeps the timeline sorted, so that a
// second evaluation in the same render quantum doesn't fail
// CHECK_GE(time2, time1).  See crbug.com/579569387.
TEST(AudioParamHandlerTest, SetTargetFollowedByRampAfterEventAtSameTime) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  constexpr float kSampleRate = 48000;
  constexpr unsigned kRenderQuantumFrames = 128;
  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 1, kSampleRate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  OscillatorNode* osc = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());
  AudioParamHandler& handler = osc->frequency()->Handler();

  // `time` is one ULP after `kFrame` / `kSampleRate`, but `time` *
  // `kSampleRate` still rounds to `kFrame`, which starts a render quantum.
  constexpr size_t kFrame = 2944;
  const double frame_time = kFrame / double{kSampleRate};
  const double time =
      std::nextafter(frame_time, std::numeric_limits<double>::infinity());
  ASSERT_GT(time, frame_time);
  ASSERT_EQ(time * kSampleRate, static_cast<double>(kFrame));

  handler.SetValueAtTime(100, 0, exception_state);
  handler.SetValueAtTime(200, time, exception_state);
  handler.SetTargetAtTime(300, time, 0.1, exception_state);
  handler.LinearRampToValueAtTime(400, time + 1, 0, 0, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Compute the k-rate value twice in the render quantum that starts at
  // `kFrame`, as happens when a node reads the same AudioParam more than once
  // per render quantum (e.g. PannerHandler::Process() reads its k-rate position
  // params in UpdateDirtyState() and again in DistanceConeGain()).  The first
  // computation replaces the SetTarget event.
  std::array<float, 1> value;
  for (int i = 0; i < 2; ++i) {
    handler.ValuesForFrameRange(kFrame, kFrame + 1, 0, value, kSampleRate,
                                kSampleRate / kRenderQuantumFrames,
                                handler.MinValue(), handler.MaxValue(),
                                kRenderQuantumFrames);
  }

  base::AutoLock locker(handler.events_lock_);
  for (wtf_size_t i = 1; i < handler.events_.size(); ++i) {
    EXPECT_LE(handler.events_[i - 1]->Time(), handler.events_[i]->Time());
  }
}

// Like SetTargetFollowedByRampAfterEventAtSameTime, but the time of the current
// frame is one ULP after the ramp that follows the SetTarget event.  This used
// to fail CHECK_GE(time2, time1) in the first evaluation.  See
// crbug.com/579569387.
TEST(AudioParamHandlerTest, SetTargetFollowedByRampAtSameTime) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  constexpr float kSampleRate = 48000;
  constexpr unsigned kRenderQuantumFrames = 128;
  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 1, kSampleRate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  OscillatorNode* osc = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());
  AudioParamHandler& handler = osc->frequency()->Handler();

  // `time` is one ULP before `kFrame` / `kSampleRate`, but `time` *
  // `kSampleRate` still rounds to `kFrame`.
  constexpr size_t kFrame = 5;
  const double frame_time = kFrame / double{kSampleRate};
  const double time = std::nextafter(frame_time, 0.0);
  ASSERT_LT(time, frame_time);
  ASSERT_EQ(time * kSampleRate, static_cast<double>(kFrame));

  handler.SetTargetAtTime(300, time, 0.1, exception_state);
  handler.LinearRampToValueAtTime(400, time, 0, 0, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  std::array<float, kRenderQuantumFrames> values;
  handler.ValuesForFrameRange(0, kRenderQuantumFrames, 0, values, kSampleRate,
                              kSampleRate, handler.MinValue(),
                              handler.MaxValue(), kRenderQuantumFrames);

  // The default value holds until the first event, and the ramp ends at
  // `kFrame`, so its value holds from there on.
  EXPECT_FLOAT_EQ(values[kFrame - 1], 0);
  EXPECT_FLOAT_EQ(values[kFrame], 400);
  EXPECT_FLOAT_EQ(values[kRenderQuantumFrames - 1], 400);

  base::AutoLock locker(handler.events_lock_);
  for (wtf_size_t i = 1; i < handler.events_.size(); ++i) {
    EXPECT_LE(handler.events_[i - 1]->Time(), handler.events_[i]->Time());
  }
}

TEST(AudioParamHandlerTest, SetValueCurveWithTinyDuration) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 48000, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  OscillatorNode* osc = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());

  AudioParamHandler& handler = osc->frequency()->Handler();
  // Use 6 points and a normalized duration > DBL_MIN so that
  // (curve.size() - 1) / duration overflows double to +Infinity while
  // surviving DenormalDisabler (FTZ/DAZ) on the audio thread.
  const Vector<float> curve = {100.0f, 140.0f, 180.0f, 220.0f, 260.0f, 300.0f};
  constexpr double kTinyDuration = 2.3e-308;
  constexpr double kCancelTime = 2.25e-308;
  ASSERT_GT(kTinyDuration, std::numeric_limits<double>::min());
  ASSERT_GT(kCancelTime, std::numeric_limits<double>::min());
  ASSERT_LT(kCancelTime, kTinyDuration);
  ASSERT_TRUE(std::isinf((curve.size() - 1) / kTinyDuration));

  handler.SetValueCurveAtTime(curve, 0.0, kTinyDuration, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Verify CancelAndHoldAtTime within a tiny-duration curve does not produce
  // NaN or overflow when interpolating the hold value, and actually evaluates
  // to the expected interpolated value on the audio thread.
  handler.CancelAndHoldAtTime(kCancelTime, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  FakeAudioThread audio_thread(ThreadType::kRealtimeAudioWorkletThread);
  constexpr float kExpectedHoldValue =
      static_cast<float>(100.0 + 200.0 * (kCancelTime / kTinyDuration));
  audio_thread.RunOnAudioThreadWithContext(
      context,
      CrossThreadBindOnce(
          [](AudioParam* param, float expected_hold_value) {
            DenormalDisabler denormal_disabler;
            AudioParamHandler& h = param->Handler();
            h.SetAutomationRate(V8AutomationRate::Enum::kARate);
            std::array<float, 128> values{};
            h.CalculateSampleAccurateValues(values);
            EXPECT_FLOAT_EQ(values[0], 100.0f);
            EXPECT_NEAR(values[1], expected_hold_value, 1e-3f);
            EXPECT_NEAR(values[127], expected_hold_value, 1e-3f);
          },
          WrapCrossThreadPersistent(osc->frequency()), kExpectedHoldValue));

  // Re-schedule the tiny-duration curve and verify both k-rate and a-rate
  // evaluation produce finite curve values rather than NaN from 0 * inf.
  handler.CancelScheduledValues(0.0, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  handler.SetValueCurveAtTime(curve, 0.0, kTinyDuration, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  handler.SetAutomationRate(V8AutomationRate::Enum::kKRate);
  audio_thread.RunOnAudioThreadWithContext(
      context, CrossThreadBindOnce(
                   [](AudioParam* param) {
                     DenormalDisabler denormal_disabler;
                     AudioParamHandler& h = param->Handler();
                     EXPECT_FLOAT_EQ(h.FinalValue(), 100.0f);
                     EXPECT_FLOAT_EQ(h.Value(), 100.0f);

                     h.SetAutomationRate(V8AutomationRate::Enum::kARate);
                     std::array<float, 128> values{};
                     h.CalculateSampleAccurateValues(values);
                     EXPECT_FLOAT_EQ(values[0], 100.0f);
                     EXPECT_FLOAT_EQ(values[1], 300.0f);
                     EXPECT_FLOAT_EQ(values[127], 300.0f);
                   },
                   WrapCrossThreadPersistent(osc->frequency())));
}

}  // namespace blink
