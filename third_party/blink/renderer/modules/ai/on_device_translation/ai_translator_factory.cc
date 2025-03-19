// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator_factory.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/create_translator_client.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {
using mojom::blink::CanCreateTranslatorResult;
using mojom::blink::CreateTranslatorError;

}  // namespace

AITranslatorFactory::AITranslatorFactory(ExecutionContext* context)
    : ExecutionContextClient(context) {}

ScriptPromise<V8AIAvailability> AITranslatorFactory::availability(
    ScriptState* script_state,
    AITranslatorCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8AIAvailability>();
  }

  ScriptPromiseResolver<V8AIAvailability>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8AIAvailability>>(
          script_state);
  ScriptPromise<V8AIAvailability> promise = resolver->Promise();
  ExecutionContext* execution_context = GetExecutionContext();

  GetTranslationManagerRemote()->TranslationAvailable(
      mojom::blink::TranslatorLanguageCode::New(options->sourceLanguage()),
      mojom::blink::TranslatorLanguageCode::New(options->targetLanguage()),
      WTF::BindOnce(
          [](ExecutionContext* execution_context,
             ScriptPromiseResolver<V8AIAvailability>* resolver,
             CanCreateTranslatorResult result) {
            CHECK(resolver);

            AIAvailability availability =
                HandleTranslatorAvailabilityCheckResult(execution_context,
                                                        result);
            resolver->Resolve(AIAvailabilityToV8(availability));
          },
          WrapPersistent(execution_context), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<AITranslator> AITranslatorFactory::create(
    ScriptState* script_state,
    AITranslatorCreateOptions* options,
    ExceptionState& exception_state) {
  // If `sourceLanguage` and `targetLanguage` are not passed, A TypeError should
  // be thrown before we get here.
  CHECK(options && options->sourceLanguage() && options->targetLanguage());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AITranslator>>(script_state);

  CreateTranslatorClient* create_translator_client =
      MakeGarbageCollected<CreateTranslatorClient>(script_state, options,
                                                   resolver);

  GetTranslationManagerRemote()->CanCreateTranslator(
      mojom::blink::TranslatorLanguageCode::New(options->sourceLanguage()),
      mojom::blink::TranslatorLanguageCode::New(options->targetLanguage()),
      WTF::BindOnce(&CreateTranslatorClient::OnGotAvailability,
                    WrapPersistent(create_translator_client)));

  return resolver->Promise();
}

HeapMojoRemote<mojom::blink::TranslationManager>&
AITranslatorFactory::GetTranslationManagerRemote() {
  return AIInterfaceProxy::GetTranslationManagerRemote(GetExecutionContext());
}

void AITranslatorFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
