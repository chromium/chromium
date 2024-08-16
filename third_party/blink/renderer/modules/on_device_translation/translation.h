// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_TRANSLATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_TRANSLATION_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_translation_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_translation_language_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class LanguageDetector;
class LanguageTranslator;
class V8TranslationAvailability;

// The class that manages the translation APIs that allow developers to
// create Translators.
class Translation final : public ScriptWrappable,
                          public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Translation(ExecutionContext* context);
  ~Translation() override = default;

  void Trace(Visitor* visitor) const override;

  // translation.idl implementation.
  ScriptPromise<V8TranslationAvailability> canTranslate(
      ScriptState* script_state,
      TranslationLanguageOptions* options,
      ExceptionState& exception_state);
  ScriptPromise<LanguageTranslator> createTranslator(
      ScriptState* script_state,
      TranslationLanguageOptions* options,
      ExceptionState& exception_state);
  ScriptPromise<V8TranslationAvailability> canDetect(
      ScriptState* script_state,
      ExceptionState& exception_state);
  ScriptPromise<LanguageDetector> createDetector(
      ScriptState* script_state,
      ExceptionState& exception_state);

 private:
  HeapMojoRemote<mojom::blink::TranslationManager>&
  GetTranslationManagerRemote();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::TranslationManager> translation_manager_remote_{
      nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_TRANSLATION_H_
