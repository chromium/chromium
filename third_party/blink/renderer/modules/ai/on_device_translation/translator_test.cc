// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/translator.h"

#include "base/test/metrics/histogram_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_translator_translate_options.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

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
      "ja", /*abort_signal=*/nullptr);

  DummyExceptionStateForTesting exception_state;
  const String kInput = "Hello, world!";
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

}  // namespace blink
