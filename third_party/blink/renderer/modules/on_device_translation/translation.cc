// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/on_device_translation/translation.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_translation_language_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/on_device_translation/language_detector.h"
#include "third_party/blink/renderer/modules/on_device_translation/language_translator.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

Translation::Translation(ExecutionContext* context)
    : ExecutionContextClient(context),
      task_runner_(context->GetTaskRunner(TaskType::kInternalDefault)) {}

void Translation::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(translation_manager_remote_);
}

HeapMojoRemote<mojom::blink::TranslationManager>&
Translation::GetTranslationManagerRemote() {
  if (!translation_manager_remote_.is_bound()) {
    if (GetExecutionContext()) {
      GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
          translation_manager_remote_.BindNewPipeAndPassReceiver(task_runner_));
    }
  }
  return translation_manager_remote_;
}

ScriptPromise<V8TranslationAvailability> Translation::canTranslate(
    ScriptState* script_state,
    TranslationLanguageOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    // TODO(https://crbug.com/357031848): Expose and use the helper.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8TranslationAvailability>>(
          script_state);
  auto promise = resolver->Promise();

  if (!GetTranslationManagerRemote().is_connected()) {
    resolver->Resolve(
        V8TranslationAvailability(V8TranslationAvailability::Enum::kNo));
  } else {
    GetTranslationManagerRemote()->CanCreateTranslator(
        options->sourceLanguage(), options->targetLanguage(),
        WTF::BindOnce(
            [](ScriptPromiseResolver<V8TranslationAvailability>* resolver,
               bool can_create) {
              if (can_create) {
                resolver->Resolve(V8TranslationAvailability(
                    V8TranslationAvailability::Enum::kReadily));
              } else {
                resolver->Resolve(V8TranslationAvailability(
                    V8TranslationAvailability::Enum::kNo));
              }
            },
            WrapPersistent(resolver)));
  }

  return promise;
}

ScriptPromise<LanguageTranslator> Translation::createTranslator(
    ScriptState* script_state,
    TranslationLanguageOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LanguageTranslator>>(
          script_state);
  LanguageTranslator* translator = MakeGarbageCollected<LanguageTranslator>(
      options->sourceLanguage(), options->targetLanguage(), task_runner_);
  auto promise = resolver->Promise();

  GetTranslationManagerRemote()->CreateTranslator(
      options->sourceLanguage(), options->targetLanguage(),
      translator->GetTranslatorReceiver(),
      WTF::BindOnce(
          [](ScriptPromiseResolver<LanguageTranslator>* resolver,
             LanguageTranslator* translator, bool success) {
            if (success) {
              resolver->Resolve(translator);
            } else {
              resolver->Reject(DOMException::Create(
                  "Unable to create translator for the given source and target "
                  "language.",
                  DOMException::GetErrorName(
                      DOMExceptionCode::kNotSupportedError)));
            }
          },
          WrapPersistent(resolver), WrapPersistent(translator)));

  return promise;
}

// TODO(crbug.com/349927087): The new version is
// AILanguageDetectorCapabilities::canDetect(). Delete this old version.
ScriptPromise<V8TranslationAvailability> Translation::canDetect(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<V8TranslationAvailability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8TranslationAvailability>>(
          script_state);
  auto promise = resolver->Promise();

  resolver->Resolve(
      V8TranslationAvailability(V8TranslationAvailability::Enum::kReadily));

  return promise;
}

// TODO(crbug.com/349927087): The new version is
// AILanguageDetectorFactory::create(). Delete this old version.
ScriptPromise<LanguageDetector> Translation::createDetector(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<LanguageDetector>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LanguageDetector>>(
          script_state);
  resolver->Resolve(MakeGarbageCollected<LanguageDetector>());
  return resolver->Promise();
}
}  // namespace blink
