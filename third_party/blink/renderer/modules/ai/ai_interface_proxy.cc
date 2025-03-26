// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

const char AIInterfaceProxy::kSupplementName[] = "AIInterfaceProxy";

AIInterfaceProxy::AIInterfaceProxy(ExecutionContext* execution_context)
    : Supplement<ExecutionContext>(*execution_context),
      task_runner_(
          execution_context->GetTaskRunner(TaskType::kInternalDefault)) {}

AIInterfaceProxy::~AIInterfaceProxy() = default;

// static
AIInterfaceProxy* AIInterfaceProxy::From(ExecutionContext* execution_context) {
  AIInterfaceProxy* translation_manager_proxy =
      Supplement<ExecutionContext>::From<AIInterfaceProxy>(*execution_context);
  if (!translation_manager_proxy) {
    translation_manager_proxy =
        MakeGarbageCollected<AIInterfaceProxy>(execution_context);
    ProvideTo(*execution_context, translation_manager_proxy);
  }
  return translation_manager_proxy;
}

void AIInterfaceProxy::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  visitor->Trace(translation_manager_remote_);
  visitor->Trace(language_detection_driver_);
  visitor->Trace(ai_manager_remote_);
}

// static
scoped_refptr<base::SequencedTaskRunner> AIInterfaceProxy::GetTaskRunner(
    ExecutionContext* execution_context) {
  return AIInterfaceProxy::From(execution_context)->task_runner_;
}

// static
HeapMojoRemote<mojom::blink::TranslationManager>&
AIInterfaceProxy::GetTranslationManagerRemote(
    ExecutionContext* execution_context) {
  return From(execution_context)
      ->GetTranslationManagerRemoteImpl(execution_context);
}

// static
HeapMojoRemote<
    language_detection::mojom::blink::ContentLanguageDetectionDriver>&
AIInterfaceProxy::GetLanguageDetectionDriverRemote(
    ExecutionContext* execution_context) {
  return From(execution_context)
      ->GetLanguageDetectionDriverRemoteImpl(execution_context);
}

// static
HeapMojoRemote<mojom::blink::AIManager>& AIInterfaceProxy::GetAIManagerRemote(
    ExecutionContext* execution_context) {
  return From(execution_context)->GetAIManagerRemoteImpl(execution_context);
}

HeapMojoRemote<mojom::blink::TranslationManager>&
AIInterfaceProxy::GetTranslationManagerRemoteImpl(
    ExecutionContext* execution_context) {
  if (!translation_manager_remote_.is_bound()) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        translation_manager_remote_.BindNewPipeAndPassReceiver(task_runner_));
  }
  return translation_manager_remote_;
}

HeapMojoRemote<
    language_detection::mojom::blink::ContentLanguageDetectionDriver>&
AIInterfaceProxy::GetLanguageDetectionDriverRemoteImpl(
    ExecutionContext* execution_context) {
  if (!language_detection_driver_.is_bound()) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        language_detection_driver_.BindNewPipeAndPassReceiver(task_runner_));
  }
  return language_detection_driver_;
}

HeapMojoRemote<mojom::blink::AIManager>&
AIInterfaceProxy::GetAIManagerRemoteImpl(ExecutionContext* execution_context) {
  if (!ai_manager_remote_.is_bound()) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        ai_manager_remote_.BindNewPipeAndPassReceiver(task_runner_));
  }
  return ai_manager_remote_;
}

}  // namespace blink
