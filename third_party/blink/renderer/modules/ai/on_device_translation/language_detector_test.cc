// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/language_detector.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detector_detect_options.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/platform/language_detection/language_detection_model.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(LanguageDetectorTest, PromptRequestSizeMetric) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  base::HistogramTester histogram_tester;
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* model = MakeGarbageCollected<LanguageDetectionModel>();
  auto* detector = MakeGarbageCollected<LanguageDetector>(
      scope.GetScriptState(), model, /*create_abort_signal=*/nullptr,
      /*expected_input_languages=*/std::nullopt, task_runner);

  DummyExceptionStateForTesting exception_state;
  // Use a 16-bit string where CharactersSizeInBytes() != length().
  const String kInput = String::FromUtf8("こんにちは");
  ASSERT_FALSE(kInput.Is8Bit());
  EXPECT_NE(kInput.CharactersSizeInBytes(), kInput.length());
  detector->detect(scope.GetScriptState(), kInput,
                   LanguageDetectorDetectOptions::Create(), exception_state);

  histogram_tester.ExpectUniqueSample(
      "AI.Session.LanguageDetector.PromptRequestSize", kInput.length(), 1);
  histogram_tester.ExpectUniqueSample(
      "AI.Session.LanguageDetector.PromptResponseStatus",
      mojom::blink::ModelStreamingResponseStatus::kErrorUnknown, 1);
}

TEST(LanguageDetectorTest, OnDetectCompleteMetrics) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  base::HistogramTester histogram_tester;

  auto* resolver = MakeGarbageCollected<
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>>(
      scope.GetScriptState(), /*abort_signal=*/nullptr);

  Vector<LanguageDetectionModel::LanguagePrediction> predictions;
  predictions.push_back(LanguageDetectionModel::LanguagePrediction{"en", 0.95});
  predictions.push_back(
      LanguageDetectionModel::LanguagePrediction{"unknown", 0.05});

  LanguageDetector::OnDetectComplete(
      resolver, base::TimeTicks::Now() - base::Milliseconds(5),
      std::move(predictions));

  histogram_tester.ExpectUniqueSample(
      "AI.Session.LanguageDetector.PromptResponseStatus",
      mojom::blink::ModelStreamingResponseStatus::kComplete, 1);
  histogram_tester.ExpectTotalCount(
      "AI.Session.LanguageDetector.ResponseCompleteTime", 1);
}

TEST(LanguageDetectorTest, OnDetectCompleteAbortedNoMetrics) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  base::HistogramTester histogram_tester;

  auto* abort_controller = AbortController::Create(scope.GetScriptState());
  auto* resolver = MakeGarbageCollected<
      ResolverWithAbortSignal<IDLSequence<LanguageDetectionResult>>>(
      scope.GetScriptState(), abort_controller->signal());

  abort_controller->abort(scope.GetScriptState());
  ASSERT_TRUE(resolver->aborted());

  Vector<LanguageDetectionModel::LanguagePrediction> predictions;
  predictions.push_back(LanguageDetectionModel::LanguagePrediction{"en", 0.95});
  predictions.push_back(
      LanguageDetectionModel::LanguagePrediction{"unknown", 0.05});

  LanguageDetector::OnDetectComplete(
      resolver, base::TimeTicks::Now() - base::Milliseconds(5),
      std::move(predictions));

  histogram_tester.ExpectTotalCount(
      "AI.Session.LanguageDetector.PromptResponseStatus", 0);
  histogram_tester.ExpectTotalCount(
      "AI.Session.LanguageDetector.ResponseCompleteTime", 0);
}

}  // namespace blink
