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
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {

const char kExceptionMessageUnableToCreateTranslator[] =
    "Unable to create translator for the given source and target language.";

bool RequiresUserActivation(mojom::blink::CanCreateTranslatorResult result) {
  switch (result) {
    case mojom::blink::CanCreateTranslatorResult::kAfterDownloadLibraryNotReady:
    case mojom::blink::CanCreateTranslatorResult::
        kAfterDownloadLanguagePackNotReady:
    case mojom::blink::CanCreateTranslatorResult::
        kAfterDownloadLibraryAndLanguagePackNotReady:
      return true;
    case mojom::blink::CanCreateTranslatorResult::kReadily:
    case mojom::blink::CanCreateTranslatorResult::kNoNotSupportedLanguage:
    case mojom::blink::CanCreateTranslatorResult::kNoAcceptLanguagesCheckFailed:
    case mojom::blink::CanCreateTranslatorResult::
        kNoExceedsLanguagePackCountLimitation:
    case mojom::blink::CanCreateTranslatorResult::kNoServiceCrashed:
    case mojom::blink::CanCreateTranslatorResult::kNoDisallowedByPolicy:
    case mojom::blink::CanCreateTranslatorResult::
        kNoExceedsServiceCountLimitation:
      return false;
  }
}

class CreateTranslatorClient
    : public GarbageCollected<CreateTranslatorClient>,
      public mojom::blink::TranslationManagerCreateTranslatorClient,
      public AIMojoClient<AITranslator> {
 public:
  CreateTranslatorClient(ScriptState* script_state,
                         AITranslatorFactory* translation,
                         AITranslatorCreateOptions* options,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         ScriptPromiseResolver<AITranslator>* resolver)
      : AIMojoClient(script_state,
                     translation,
                     resolver,
                     options->getSignalOr(nullptr)),
        translation_(translation),
        source_language_(options->sourceLanguage()),
        target_language_(options->targetLanguage()),
        receiver_(this, translation_->GetExecutionContext()),
        task_runner_(task_runner) {
    if (options->hasMonitor()) {
      monitor_ = MakeGarbageCollected<AICreateMonitor>(
          translation_->GetExecutionContext(), task_runner);
      std::ignore = options->monitor()->Invoke(nullptr, monitor_);
    }
  }
  ~CreateTranslatorClient() override = default;

  CreateTranslatorClient(const CreateTranslatorClient&) = delete;
  CreateTranslatorClient& operator=(const CreateTranslatorClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(translation_);
    visitor->Trace(receiver_);
    visitor->Trace(monitor_);
  }

  void OnResult(mojom::blink::CreateTranslatorResultPtr result) override {
    if (!GetResolver()) {
      // The request was aborted. Note: Currently abort signal is not supported.
      // TODO(crbug.com/331735396): Support abort signal.
      return;
    }
    if (result->is_translator()) {
      // TODO (crbug.com/391715395): Pass the real download progress rather than
      // mocking one.
      if (monitor_) {
        monitor_->OnDownloadProgressUpdate(0, 1);
        monitor_->OnDownloadProgressUpdate(1, 1);
      }

      GetResolver()->Resolve(MakeGarbageCollected<AITranslator>(
          std::move(result->get_translator()), task_runner_,
          std::move(source_language_), std::move(target_language_)));
    } else {
      CHECK(result->is_error());
      GetResolver()->Reject(DOMException::Create(
          kExceptionMessageUnableToCreateTranslator,
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
    }
    Cleanup();
  }

  void OnGotAvailability(mojom::blink::CanCreateTranslatorResult result) {
    LocalDOMWindow* const window = LocalDOMWindow::From(GetScriptState());

    if (RuntimeEnabledFeatures::TranslationAPIV1Enabled() &&
        RequiresUserActivation(result) &&
        !LocalFrame::ConsumeTransientUserActivation(window->GetFrame())) {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Requires handling a user gesture when availability is "
          "\"after-download\".");
      return;
    }
    mojo::PendingRemote<mojom::blink::TranslationManagerCreateTranslatorClient>
        client;

    receiver_.Bind(client.InitWithNewPipeAndPassReceiver(), task_runner_);

    translation_->GetTranslationManagerRemote()->CreateTranslator(
        std::move(client),
        mojom::blink::TranslatorCreateOptions::New(
            mojom::blink::TranslatorLanguageCode::New(source_language_),
            mojom::blink::TranslatorLanguageCode::New(target_language_)));
  }

  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<AITranslatorFactory> translation_;

  Member<AICreateMonitor> monitor_;
  String source_language_;
  String target_language_;

  HeapMojoReceiver<mojom::blink::TranslationManagerCreateTranslatorClient,
                   CreateTranslatorClient>
      receiver_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace

AITranslatorFactory::AITranslatorFactory(ExecutionContext* context)
    : ExecutionContextClient(context),
      task_runner_(context->GetTaskRunner(TaskType::kInternalDefault)) {}

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
      MakeGarbageCollected<CreateTranslatorClient>(script_state, this, options,
                                                   task_runner_, resolver);

  GetTranslationManagerRemote()->CanCreateTranslator(
      mojom::blink::TranslatorLanguageCode::New(options->sourceLanguage()),
      mojom::blink::TranslatorLanguageCode::New(options->targetLanguage()),
      WTF::BindOnce(&CreateTranslatorClient::OnGotAvailability,
                    WrapPersistent(create_translator_client)));

  return resolver->Promise();
}

ScriptPromise<AITranslatorCapabilities> AITranslatorFactory::capabilities(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<AITranslatorCapabilities>();
  }
  ScriptPromiseResolver<AITranslatorCapabilities>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AITranslatorCapabilities>>(
          script_state);
  ScriptPromise<AITranslatorCapabilities> promise = resolver->Promise();

  GetTranslationManagerRemote()->GetTranslatorAvailabilityInfo(WTF::BindOnce(
      [](ScriptPromiseResolver<AITranslatorCapabilities>* resolver,
         mojom::blink::TranslatorAvailabilityInfoPtr info) {
        resolver->Resolve(
            MakeGarbageCollected<AITranslatorCapabilities>(std::move(info)));
      },
      WrapPersistent(resolver)));
  return promise;
}

HeapMojoRemote<mojom::blink::TranslationManager>&
AITranslatorFactory::GetTranslationManagerRemote() {
  if (!translation_manager_remote_.is_bound()) {
    if (GetExecutionContext()) {
      GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
          translation_manager_remote_.BindNewPipeAndPassReceiver(task_runner_));
    }
  }
  return translation_manager_remote_;
}

void AITranslatorFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(translation_manager_remote_);
}

}  // namespace blink
