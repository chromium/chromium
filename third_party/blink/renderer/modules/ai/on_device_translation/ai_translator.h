
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_translator_translate_options.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class AITranslator final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AITranslator(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~AITranslator() override = default;

  mojo::PendingReceiver<blink::mojom::blink::Translator>
  GetTranslatorReceiver();

  void Trace(Visitor* visitor) const override;

  // ai_translator.idl implementation
  ScriptPromise<IDLString> translate(ScriptState* script_state,
                                     const WTF::String& input,
                                     AITranslatorTranslateOptions* options,
                                     ExceptionState& exception_state);
  void destroy(ScriptState*);

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<blink::mojom::blink::Translator> translator_remote_{nullptr};
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_H_
