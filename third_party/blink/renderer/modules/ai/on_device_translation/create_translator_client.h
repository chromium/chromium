// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_CREATE_TRANSLATOR_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_CREATE_TRANSLATOR_CLIENT_H_

#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

class CreateTranslatorClient
    : public GarbageCollected<CreateTranslatorClient>,
      public mojom::blink::TranslationManagerCreateTranslatorClient,
      public ExecutionContextClient,
      public AIMojoClient<AITranslator> {
 public:
  CreateTranslatorClient(ScriptState* script_state,
                         AITranslatorCreateOptions* options,
                         ScriptPromiseResolver<AITranslator>* resolver);
  ~CreateTranslatorClient() override;

  CreateTranslatorClient(const CreateTranslatorClient&) = delete;
  CreateTranslatorClient& operator=(const CreateTranslatorClient&) = delete;

  void Trace(Visitor* visitor) const override;

  void OnResult(mojom::blink::CreateTranslatorResultPtr result) override;

  void OnGotAvailability(mojom::blink::CanCreateTranslatorResult result);

  void ResetReceiver() override;

 private:
  Member<AICreateMonitor> monitor_;
  String source_language_;
  String target_language_;

  HeapMojoReceiver<mojom::blink::TranslationManagerCreateTranslatorClient,
                   CreateTranslatorClient>
      receiver_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_CREATE_TRANSLATOR_CLIENT_H_
