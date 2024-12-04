// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/on_device_translation/translation.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_translation_language_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/on_device_translation/language_detector.h"
#include "third_party/blink/renderer/modules/on_device_translation/language_translator.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {
namespace {

using mojom::blink::CreateTranslatorError;
using mojom::blink::TranslatorLanguageCode;

const char kExceptionMessageUnableToCreateTranslator[] =
    "Unable to create translator for the given source and target language.";
const char kLinkToDocument[] =
    "See "
    "https://developer.chrome.com/docs/ai/translator-api?#supported-languages "
    "for more details.";

String ConvertCreateTranslatorErrorToDebugString(
    mojom::blink::CreateTranslatorError error) {
  switch (error) {
    case CreateTranslatorError::kInvalidBinary:
      return "Failed to load the translation library.";
    case CreateTranslatorError::kInvalidFunctionPointer:
      return "The translation library is not compatible.";
    case CreateTranslatorError::kFailedToInitialize:
      return "Failed to initialize the translation library.";
    case CreateTranslatorError::kFailedToCreateTranslator:
      return "The translation library failed to create a translator.";
    case CreateTranslatorError::kAcceptLanguagesCheckFailed:
      return String(base::StrCat(
          {"The preferred languages check for Translator API failed. ",
           kLinkToDocument}));
    case CreateTranslatorError::kExceedsLanguagePackCountLimitation:
      return String(base::StrCat(
          {"The Translator API language pack count exceeded the limitation. ",
           kLinkToDocument}));
    case CreateTranslatorError::kServiceCrashed:
      return "The translation service crashed.";
    case CreateTranslatorError::kDisallowedByPolicy:
      return "The translation is disallowed by policy.";
    case CreateTranslatorError::kExceedsServiceCountLimitation:
      return "The translation service count exceeded the limitation.";
    case CreateTranslatorError::kExceedsPendingTaskCountLimitation:
      return "Too many Translator API requests are queued.";
  }
}
class CreateTranslatorClient
    : public GarbageCollected<CreateTranslatorClient>,
      public mojom::blink::TranslationManagerCreateTranslatorClient,
      public AIMojoClient<LanguageTranslator> {
 public:
  CreateTranslatorClient(
      ScriptState* script_state,
      Translation* translation,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ScriptPromiseResolver<LanguageTranslator>* resolver,
      String source_language,
      String target_language,
      mojo::PendingReceiver<
          mojom::blink::TranslationManagerCreateTranslatorClient>
          pending_receiver)
      : AIMojoClient(script_state,
                     translation,
                     resolver,
                     // Currently abort signal is not supported.
                     // TODO(crbug.com/331735396): Support abort signal.
                     /*abort_signal=*/nullptr),
        translation_(translation),
        receiver_(this, translation_->GetExecutionContext()),
        task_runner_(task_runner),
        source_language_(source_language),
        target_language_(target_language) {
    receiver_.Bind(std::move(pending_receiver), task_runner);
  }
  ~CreateTranslatorClient() override = default;

  CreateTranslatorClient(const CreateTranslatorClient&) = delete;
  CreateTranslatorClient& operator=(const CreateTranslatorClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(translation_);
    visitor->Trace(receiver_);
  }

  void OnResult(mojom::blink::CreateTranslatorResultPtr result) override {
    if (!GetResolver()) {
      // The request was aborted. Note: Currently abort signal is not supported.
      // TODO(crbug.com/331735396): Support abort signal.
      return;
    }
    if (result->is_translator()) {
      GetResolver()->Resolve(MakeGarbageCollected<LanguageTranslator>(
          source_language_, target_language_,
          std::move(result->get_translator()), task_runner_));
    } else {
      CHECK(result->is_error());
      translation_->GetExecutionContext()->AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          ConvertCreateTranslatorErrorToDebugString(result->get_error()));
      GetResolver()->Reject(DOMException::Create(
          kExceptionMessageUnableToCreateTranslator,
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
    }
    Cleanup();
  }

  void ResetReceiver() override {
    receiver_.reset();
  }

 private:
  Member<Translation> translation_;
  HeapMojoReceiver<mojom::blink::TranslationManagerCreateTranslatorClient,
                   CreateTranslatorClient>
      receiver_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const String source_language_;
  const String target_language_;
};

}  // namespace

using mojom::blink::CanCreateTranslatorResult;

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

// TODO(crbug.com/322229993): The new version is
// AITranslatorCapabilities::languagePairAvailable(). Delete this old version.
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
        TranslatorLanguageCode::New(options->sourceLanguage()),
        TranslatorLanguageCode::New(options->targetLanguage()),
        WTF::BindOnce(
            [](ScriptPromiseResolver<V8TranslationAvailability>* resolver,
               CanCreateTranslatorResult result) {
              // TODO(crbug.com/369761976): Record UMAs.
              switch (result) {
                case CanCreateTranslatorResult::kReadily:
                  resolver->Resolve(V8TranslationAvailability(
                      V8TranslationAvailability::Enum::kReadily));
                  break;
                case CanCreateTranslatorResult::kAfterDownloadLibraryNotReady:
                case CanCreateTranslatorResult::
                    kAfterDownloadLanguagePackNotReady:
                case CanCreateTranslatorResult::
                    kAfterDownloadLibraryAndLanguagePackNotReady:
                  resolver->Resolve(V8TranslationAvailability(
                      V8TranslationAvailability::Enum::kAfterDownload));
                  break;
                case CanCreateTranslatorResult::kNoNotSupportedLanguage:
                case CanCreateTranslatorResult::kNoAcceptLanguagesCheckFailed:
                case CanCreateTranslatorResult::
                    kNoExceedsLanguagePackCountLimitation:
                case CanCreateTranslatorResult::kNoServiceCrashed:
                case CanCreateTranslatorResult::kNoDisallowedByPolicy:
                case CanCreateTranslatorResult::
                    kNoExceedsServiceCountLimitation:
                  resolver->Resolve(V8TranslationAvailability(
                      V8TranslationAvailability::Enum::kNo));
                  break;
              }
            },
            WrapPersistent(resolver)));
  }

  return promise;
}

// TODO(crbug.com/349927087): The new version is
// AITranslatorFactory::create(). Delete this old version.
ScriptPromise<LanguageTranslator> Translation::createTranslator(
    ScriptState* script_state,
    TranslationLanguageOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }
  CHECK(options);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LanguageTranslator>>(
          script_state);
  auto promise = resolver->Promise();

  mojo::PendingRemote<mojom::blink::TranslationManagerCreateTranslatorClient>
      client;
  MakeGarbageCollected<CreateTranslatorClient>(
      script_state, this, task_runner_, resolver, options->sourceLanguage(),
      options->targetLanguage(), client.InitWithNewPipeAndPassReceiver());
  GetTranslationManagerRemote()->CreateTranslator(
      std::move(client),
      mojom::blink::TranslatorCreateOptions::New(
          TranslatorLanguageCode::New(options->sourceLanguage()),
          TranslatorLanguageCode::New(options->targetLanguage())));
  return promise;
}

// TODO(crbug.com/349927087): The new version is
// AILanguageDetectorCapabilities::languageAvailable(). Delete this old version.
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
