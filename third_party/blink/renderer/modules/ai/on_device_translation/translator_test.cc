// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/translator.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_translator_translate_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "v8/include/v8.h"

namespace blink {

TEST(TranslatorTest, PromptRequestSizeMetric) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  base::HistogramTester histogram_tester;
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
  mojo::PendingRemote<mojom::blink::Translator> pending_remote;
  auto pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();
  auto* translator = MakeGarbageCollected<Translator>(
      scope.GetScriptState(), std::move(pending_remote), task_runner, "en",
      "ja", /*abort_signal=*/nullptr, /*input_quota=*/std::nullopt);

  DummyExceptionStateForTesting exception_state;

  // Use a 16-bit string where CharactersSizeInBytes() != length().
  const String kInput = String::FromUtf8("こんにちは");
  ASSERT_FALSE(kInput.Is8Bit());
  EXPECT_NE(kInput.CharactersSizeInBytes(), kInput.length());
  translator->translate(scope.GetScriptState(), kInput,
                        TranslatorTranslateOptions::Create(), exception_state);

  histogram_tester.ExpectUniqueSample("AI.Session.Translator.PromptRequestSize",
                                      kInput.length(), 1);

  translator->translateStreaming(scope.GetScriptState(), kInput,
                                 TranslatorTranslateOptions::Create(),
                                 exception_state);

  histogram_tester.ExpectUniqueSample("AI.Session.Translator.PromptRequestSize",
                                      kInput.length(), 2);
}

TEST(TranslatorTest, InputQuotaAndMeasureInputUsage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      scope.GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault);

  auto Measure = [&](Translator* translator, const String& text) {
    DummyExceptionStateForTesting exception_state;
    ScriptPromiseTester tester(
        scope.GetScriptState(),
        translator->measureInputUsage(scope.GetScriptState(), text,
                                      TranslatorTranslateOptions::Create(),
                                      exception_state));
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    return tester.Value().V8Value().As<v8::Number>()->Value();
  };

  {
    mojo::PendingRemote<mojom::blink::Translator> pending_remote;
    auto pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();
    auto* unlimited = MakeGarbageCollected<Translator>(
        scope.GetScriptState(), std::move(pending_remote), task_runner, "en",
        "ja", /*abort_signal=*/nullptr, /*input_quota=*/std::nullopt);
    EXPECT_TRUE(std::isinf(unlimited->inputQuota()));
    EXPECT_GT(unlimited->inputQuota(), 0);
    EXPECT_EQ(Measure(unlimited, String::FromUtf8("こんにちは")), 0.0);
  }

  {
    // Matches the Android default for `TranslationAPIMaxInputSizeBytes`
    // (256 KiB).
    constexpr uint64_t kTestInputQuota = 262144;
    mojo::PendingRemote<mojom::blink::Translator> pending_remote;
    auto pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();
    auto* bounded = MakeGarbageCollected<Translator>(
        scope.GetScriptState(), std::move(pending_remote), task_runner, "en",
        "ja", /*abort_signal=*/nullptr, kTestInputQuota);
    EXPECT_EQ(bounded->inputQuota(), static_cast<double>(kTestInputQuota));
    EXPECT_EQ(Measure(bounded, String()), 0.0);
    EXPECT_EQ(Measure(bounded, g_empty_string), 0.0);
    EXPECT_EQ(Measure(bounded, "hello"), 5.0);

    constexpr LChar kLatin1EAcute = 0xE9;
    const String latin1_input(base::span_from_ref(kLatin1EAcute));
    ASSERT_TRUE(latin1_input.Is8Bit());
    EXPECT_EQ(Measure(bounded, latin1_input), 2.0);

    EXPECT_EQ(Measure(bounded, String::FromUtf8("こんにちは")), 15.0);
    EXPECT_EQ(Measure(bounded, String::FromUtf8("\xf0\x9f\x98\x80")), 4.0);

    constexpr UChar kLoneHighSurrogate = 0xD800;
    EXPECT_EQ(Measure(bounded, String(base::span_from_ref(kLoneHighSurrogate))),
              3.0);

    constexpr UChar kLoneLowSurrogate = 0xDC00;
    EXPECT_EQ(Measure(bounded, String(base::span_from_ref(kLoneLowSurrogate))),
              3.0);
  }
}

}  // namespace blink
