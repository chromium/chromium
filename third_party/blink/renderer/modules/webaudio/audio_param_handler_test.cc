// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_param_handler.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/testing/fake_audio_thread.h"
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

}  // namespace blink
