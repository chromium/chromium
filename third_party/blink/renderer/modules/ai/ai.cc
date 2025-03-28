// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/language_model_factory.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

AI::AI(ExecutionContext* context)
    : ExecutionContextClient(context),
      task_runner_(context->GetTaskRunner(TaskType::kInternalDefault)),
      ai_remote_(context) {}

void AI::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_remote_);
  visitor->Trace(language_model_factory_);
}

HeapMojoRemote<mojom::blink::AIManager>& AI::GetAIRemote() {
  if (!ai_remote_.is_bound()) {
    if (GetExecutionContext()) {
      GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
          ai_remote_.BindNewPipeAndPassReceiver(task_runner_));
    }
  }
  return ai_remote_;
}

scoped_refptr<base::SequencedTaskRunner> AI::GetTaskRunner() {
  return task_runner_;
}

LanguageModelFactory* AI::languageModel() {
  if (!language_model_factory_) {
    language_model_factory_ = MakeGarbageCollected<LanguageModelFactory>(this);
  }
  return language_model_factory_.Get();
}


}  // namespace blink
