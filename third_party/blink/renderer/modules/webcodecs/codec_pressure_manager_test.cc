// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"

#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_gauge.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager_provider.h"
#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

static constexpr size_t kTestPressureThreshold = 3;

class FakeReclaimableCodec final
    : public GarbageCollected<FakeReclaimableCodec>,
      public ReclaimableCodec {
 public:
  explicit FakeReclaimableCodec(ReclaimableCodec::CodecType type,
                                ExecutionContext* context)
      : ReclaimableCodec(type, context) {}

  ~FakeReclaimableCodec() override = default;

  void OnCodecReclaimed(DOMException* ex) final { ReleaseCodecPressure(); }

  // GarbageCollected override.
  void Trace(Visitor* visitor) const override {
    ReclaimableCodec::Trace(visitor);
  }

  bool IsGlobalPressureFlagSet() {
    return global_pressure_exceeded_for_testing();
  }

  bool IsTimerActive() { return IsReclamationTimerActiveForTesting(); }

 private:
  // ContextLifecycleObserver override.
  void ContextDestroyed() override {}
};

}  // namespace

class CodecPressureManagerTest
    : public testing::TestWithParam<ReclaimableCodec::CodecType> {
 public:
  using TestCodecSet = HeapHashSet<Member<FakeReclaimableCodec>>;

  CodecPressureManagerTest() {
    GetCodecPressureGauge().set_pressure_threshold_for_testing(
        kTestPressureThreshold);
  }

  ~CodecPressureManagerTest() override = default;

  void SetUpManager(ExecutionContext* context) {
    manager_ = GetManagerFromContext(context);
  }

  void TearDown() override {
    // Force the pre-finalizer call, otherwise the CodecPressureGauge will have
    // leftover pressure between tests.
    if (Manager())
      CleanUpManager(Manager());
  }

  CodecPressureManager* GetManagerFromContext(ExecutionContext* context) {
    auto& provider = CodecPressureManagerProvider::From(*context);

    switch (GetParam()) {
      case ReclaimableCodec::CodecType::kDecoder:
        return provider.GetDecoderPressureManager();
      case ReclaimableCodec::CodecType::kEncoder:
        return provider.GetEncoderPressureManager();
    }

    return nullptr;
  }

  CodecPressureGauge& GetCodecPressureGauge() {
    return CodecPressureGauge::GetInstance(GetParam());
  }

 protected:
  void SyncPressureFlags() {
    base::RunLoop run_loop;
    scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  FakeReclaimableCodec* CreateCodec(ExecutionContext* context) {
    return MakeGarbageCollected<FakeReclaimableCodec>(GetParam(), context);
  }

  void CleanUpManager(CodecPressureManager* manager) {
    // Manually run the pre-finalizer here. Otherwise, CodecPressureGauge will
    // have global pressure leftover from tests, and expectations will fail.
    manager->UnregisterManager();
  }

  FakeReclaimableCodec* CreateBackgroundedCodec(ExecutionContext* context) {
    auto* codec = CreateCodec(context);

    // Mark all codecs as background for test simplicity.
    codec->SimulateLifecycleStateForTesting(
        scheduler::SchedulingLifecycleState::kHidden);

    return codec;
  }

  FakeReclaimableCodec* CreatePressuringCodec(ExecutionContext* context) {
    auto* codec = CreateBackgroundedCodec(context);
    codec->ApplyCodecPressure();
    return codec;
  }

  CodecPressureManager* Manager() { return manager_; }

  void VerifyTimersStarted(const TestCodecSet& codecs) {
    SyncPressureFlags();

    size_t total_started_timers = 0u;
    for (auto& codec : codecs) {
      if (codec->IsTimerActive())
        ++total_started_timers;
    }

    EXPECT_EQ(total_started_timers, codecs.size());
  }

  void VerifyTimersStopped(const TestCodecSet& codecs) {
    SyncPressureFlags();

    size_t total_stopped_timers = 0u;
    for (auto& codec : codecs) {
      if (!codec->IsTimerActive())
        ++total_stopped_timers;
    }

    EXPECT_EQ(total_stopped_timers, codecs.size());
  }

  bool ManagerGlobalPressureExceeded() {
    SyncPressureFlags();

    return Manager()->global_pressure_exceeded_;
  }

 private:
  test::TaskEnvironment task_environment_;
  WeakPersistent<CodecPressureManager> manager_;
};

TEST_P(CodecPressureManagerTest, OneManagerPerContext) {
  V8TestingScope v8_scope;

  {
    V8TestingScope other_v8_scope;
    ASSERT_NE(other_v8_scope.GetExecutionContext(),
              v8_scope.GetExecutionContext());

    EXPECT_NE(GetManagerFromContext(v8_scope.GetExecutionContext()),
              GetManagerFromContext(other_v8_scope.GetExecutionContext()));
  }
}

TEST_P(CodecPressureManagerTest, AllManagersIncrementGlobalPressureGauge) {
  V8TestingScope v8_scope;

  EXPECT_EQ(0u, GetCodecPressureGauge().global_pressure_for_testing());

  auto* codec = CreatePressuringCodec(v8_scope.GetExecutionContext());

  EXPECT_TRUE(codec->is_applying_codec_pressure());

  EXPECT_EQ(1u, GetCodecPressureGauge().global_pressure_for_testing());
  EXPECT_EQ(1u, GetManagerFromContext(v8_scope.GetExecutionContext())
                    ->pressure_for_testing());

  {
    V8TestingScope other_v8_scope;
    ASSERT_NE(other_v8_scope.GetExecutionContext(),
              v8_scope.GetExecutionContext());

    auto* other_codec =
        CreatePressuringCodec(other_v8_scope.GetExecutionContext());

    EXPECT_TRUE(other_codec->is_applying_codec_pressure());

    EXPECT_EQ(2u, GetCodecPressureGauge().global_pressure_for_testing());
    EXPECT_EQ(1u, GetManagerFromContext(other_v8_scope.GetExecutionContext())
                      ->pressure_for_testing());

    // Test cleanup.
    CleanUpManager(GetManagerFromContext(other_v8_scope.GetExecutionContext()));
  }

  CleanUpManager(GetManagerFromContext(v8_scope.GetExecutionContext()));
}

TEST_P(CodecPressureManagerTest,
       ManagersAreInitializedWithGlobalPressureValue) {
  V8TestingScope v8_scope;
  SetUpManager(v8_scope.GetExecutionContext());

  TestCodecSet codecs_with_pressure;

  // Add pressure until we exceed the threshold.
  for (size_t i = 0; i < kTestPressureThreshold + 1; ++i) {
    codecs_with_pressure.insert(
        CreatePressuringCodec(v8_scope.GetExecutionContext()));
  }

  SyncPressureFlags();

  EXPECT_TRUE(Manager()->is_global_pressure_exceeded_for_testing());

  {
    V8TestingScope other_v8_scope;

    // "New" managers should be created with the correct global value.
    EXPECT_TRUE(GetManagerFromContext(other_v8_scope.GetExecutionContext())
                    ->is_global_pressure_exceeded_for_testing());
  }
}

TEST_P(CodecPressureManagerTest, DifferentManagersForEncodersAndDecoders) {
  V8TestingScope v8_scope;

  auto& provider =
      CodecPressureManagerProvider::From(*v8_scope.GetExecutionContext());

  EXPECT_NE(provider.GetDecoderPressureManager(),
            provider.GetEncoderPressureManager());
}

TEST_P(CodecPressureManagerTest, DisposedCodecsRemovePressure) {
  V8TestingScope v8_scope;
  SetUpManager(v8_scope.GetExecutionContext());

  auto* codec = CreatePressuringCodec(v8_scope.GetExecutionContext());

  EXPECT_TRUE(codec->is_applying_codec_pressure());
  EXPECT_EQ(1u, Manager()->pressure_for_testing());

  // Garbage collecting a pressuring codec should release its pressure.
  codec = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_EQ(0u, Manager()->pressure_for_testing());
  EXPECT_EQ(0u, GetCodecPressureGauge().global_pressure_for_testing());
}

TEST_P(CodecPressureManagerTest, ZeroPressureThreshold) {
  V8TestingScope v8_scope;
  GetCodecPressureGauge().set_pressure_threshold_for_testing(0);

  SetUpManager(v8_scope.GetExecutionContext());

  auto* codec = CreatePressuringCodec(v8_scope.GetExecutionContext());

  EXPECT_TRUE(codec->is_applying_codec_pressure());

  SyncPressureFlags();

  // Any codec added should have its global pressure flag set, if the threshold
  // is 0.
  EXPECT_TRUE(codec->IsGlobalPressureFlagSet());
}

TEST_P(CodecPressureManagerTest, AddRemovePressure) {
  V8TestingScope v8_scope;
  SetUpManager(v8_scope.GetExecutionContext());

  TestCodecSet codecs;

  for (size_t i = 0; i < kTestPressureThreshold * 2; ++i) {
    codecs.insert(CreateBackgroundedCodec(v8_scope.GetExecutionContext()));

    // Codecs shouldn't apply pressure by default.
    EXPECT_EQ(0u, Manager()->pressure_for_testing());
  }

  size_t total_pressure = 0;
  for (auto codec : codecs) {
    codec->ApplyCodecPressure();

    EXPECT_EQ(++total_pressure, Manager()->pressure_for_testing());
  }

  EXPECT_EQ(codecs.size(), Manager()->pressure_for_testing());

  for (auto codec : codecs) {
    codec->ReleaseCodecPressure();

    EXPECT_EQ(--total_pressure, Manager()->pressure_for_testing());
  }

  EXPECT_EQ(0u, Manager()->pressure_for_testing());
}

TEST_P(CodecPressureManagerTest, PressureDoesntReclaimForegroundCodecs) {
  V8TestingScope v8_scope;
  SetUpManager(v8_scope.GetExecutionContext());

  TestCodecSet codecs;

  for (size_t i = 0; i < kTestPressureThreshold * 2; ++i) {
    auto* codec = CreateCodec(v8_scope.GetExecutionContext());

    EXPECT_FALSE(codec->is_backgrounded_for_testing());

    codec->ApplyCodecPressure();
    codecs.insert(codec);
  }

  SyncPressureFlags();

  EXPECT_GT(Manager()->pressure_for_testing(), kTestPressureThreshold);

  // No foreground codec should be reclaimable
  for (auto codec : codecs)
    EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // Backgrounding codecs should start their reclamation.
  for (auto codec : codecs) {
    codec->SimulateLifecycleStateForTesting(
        scheduler::SchedulingLifecycleState::kHidden);
    EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
  }
}

TEST_P(CodecPressureManagerTest, PressureStartsTimers) {
  V8TestingScope v8_scope;
  SetUpManager(v8_scope.GetExecutionContext());

  TestCodecSet pressuring_codecs;

  for (size_t i = 0; i < kTestPressureThreshold; ++i) {
    pressuring_codecs.insert(
        CreatePressuringCodec(v8_scope.GetExecutionContext()));
  }

  // We should be at the pressure limit, but not over it.
  ASSERT_EQ(Manager()->pressure_for_testing(), kTestPressureThreshold);
  ASSERT_FALSE(ManagerGlobalPressureExceeded());
  VerifyTimersStopped(pressuring_codecs);

  // Apply slightly more pressure, pushing us over the threshold.
  pressuring_codecs.insert(
      CreatePressuringCodec(v8_scope.GetExecutionContext()));

  // Idle timers should have been started.
  ASSERT_GT(Manager()->pressure_for_testing(), kTestPressureThreshold);
  ASSERT_TRUE(ManagerGlobalPressureExceeded());
  VerifyTimersStarted(pressuring_codecs);

  // Add still more pressure, keeping us over the threshold.
  pressuring_codecs.insert(
      CreatePressuringCodec(v8_scope.GetExecutionContext()));

  // Idle timers should remain active.
  ASSERT_GT(Manager()->pressure_for_testing(), kTestPressureThreshold);
  ASSERT_TRUE(ManagerGlobalPressureExceeded());
  VerifyTimersStarted(pressuring_codecs);

  // Simulate some pressure being released.
  pressuring_codecs.TakeAny()->SimulateCodecReclaimedForTesting();

  // This shouldn't have been enough bring us back down below threshold.
  VerifyTimersStarted(pressuring_codecs);

  // Release once more, bringing us at the threshold.
  auto released_codec = pressuring_codecs.TakeAny();
  released_codec->ReleaseCodecPressure();
  EXPECT_FALSE(released_codec->IsReclamationTimerActiveForTesting());

  ASSERT_EQ(Manager()->pressure_for_testing(), kTestPressureThreshold);
  ASSERT_FALSE(ManagerGlobalPressureExceeded());
  VerifyTimersStopped(pressuring_codecs);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CodecPressureManagerTest,
    testing::Values(ReclaimableCodec::CodecType::kDecoder,
                    ReclaimableCodec::CodecType::kEncoder));

}  // namespace blink
