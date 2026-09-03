// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/language_detector.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detector_detect_options.h"
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
  const String kInput = "Hello, world!";
  detector->detect(scope.GetScriptState(), kInput,
                   LanguageDetectorDetectOptions::Create(), exception_state);

  histogram_tester.ExpectUniqueSample(
      "AI.Session.LanguageDetector.PromptRequestSize", kInput.length(), 1);
}

}  // namespace blink
