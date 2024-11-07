// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_LANGUAGE_TRANSLATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_LANGUAGE_TRANSLATOR_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// The class that represents a translator with source and target language.
class LanguageTranslator final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  LanguageTranslator(
      const String source_lang,
      const String target_lang,
      mojo::PendingRemote<mojom::blink::Translator> pending_remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~LanguageTranslator() override = default;

  void Trace(Visitor* visitor) const override;

  // language_translator.idl implementation.
  ScriptPromise<IDLString> translate(ScriptState* script_state,
                                     const String& input,
                                     ExceptionState& exception_state);
  void destroy();

 private:
  void OnTranslateFinished(ScriptPromiseResolver<IDLString>* resolver,
                           const WTF::String& output);

  const String source_lang_;
  const String target_lang_;
  HeapMojoRemote<blink::mojom::blink::Translator> translator_remote_{nullptr};
  HeapHashSet<Member<ScriptPromiseResolver<IDLString>>> pending_resolvers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_LANGUAGE_TRANSLATOR_H_
