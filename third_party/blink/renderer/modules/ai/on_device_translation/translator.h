
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_TRANSLATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_TRANSLATOR_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_translator_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_translator_translate_options.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ExceptionState;
class ReadableStream;

class Translator final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Translator(
      ScriptState* script_state,
      mojo::PendingRemote<mojom::blink::Translator> pending_remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      String source_language,
      String target_language,
      AbortSignal* abort_signal);
  ~Translator() override = default;

  mojo::PendingReceiver<blink::mojom::blink::Translator>
  GetTranslatorReceiver();

  void Trace(Visitor* visitor) const override;

  static ScriptPromise<V8Availability> availability(
      ScriptState* script_state,
      TranslatorCreateCoreOptions* options,
      ExceptionState& exception_state);

  static ScriptPromise<Translator> create(ScriptState* script_state,
                                          TranslatorCreateOptions* options,
                                          ExceptionState& exception_state);

  String sourceLanguage() const;
  String targetLanguage() const;

  // translator.idl implementation
  ScriptPromise<IDLString> translate(ScriptState* script_state,
                                     const String& input,
                                     TranslatorTranslateOptions* options,
                                     ExceptionState& exception_state);

  // translator.idl implementation
  ReadableStream* translateStreaming(ScriptState* script_state,
                                     const String& input,
                                     TranslatorTranslateOptions* options,
                                     ExceptionState& exception_state);

  ScriptPromise<IDLDouble> measureInputUsage(
      ScriptState* script_state,
      const String& input,
      TranslatorTranslateOptions* options,
      ExceptionState& exception_state);

  double inputQuota() const;

  void destroy(ScriptState* script_state);

 private:
  void DestroyImpl();

  void OnCreateAbortSignalAborted(ScriptState* script_state);

  AbortSignal* CreateCompositeSignal(ScriptState* script_state,
                                     TranslatorTranslateOptions* options);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<blink::mojom::blink::Translator> translator_remote_{nullptr};

  String source_language_;
  String target_language_;

  Member<AbortController> destruction_abort_controller_;
  Member<AbortSignal> create_abort_signal_;
  Member<AbortSignal::AlgorithmHandle> create_abort_handle_;
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_TRANSLATOR_H_
