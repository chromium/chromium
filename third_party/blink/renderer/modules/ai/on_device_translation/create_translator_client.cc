// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/create_translator_client.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {
using mojom::blink::CanCreateTranslatorResult;
using mojom::blink::CreateTranslatorError;

const char kExceptionMessageUnableToCreateTranslator[] =
    "Unable to create translator for the given source and target language.";
const char kLinkToDocument[] =
    "See "
    "https://developer.chrome.com/docs/ai/translator-api?#supported-languages "
    "for more details.";

String ConvertCreateTranslatorErrorToDebugString(CreateTranslatorError error) {
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
    case CreateTranslatorError::kInvalidVersion:
      return "The translation library version is invalid.";
  }
}

bool RequiresUserActivation(CanCreateTranslatorResult result) {
  switch (result) {
    case CanCreateTranslatorResult::kAfterDownloadLibraryNotReady:
    case CanCreateTranslatorResult::kAfterDownloadLanguagePackNotReady:
    case CanCreateTranslatorResult::
        kAfterDownloadLibraryAndLanguagePackNotReady:
    case mojom::blink::CanCreateTranslatorResult::
        kAfterDownloadTranslatorCreationRequired:
      return true;
    case CanCreateTranslatorResult::kReadily:
    case CanCreateTranslatorResult::kNoNotSupportedLanguage:
    case CanCreateTranslatorResult::kNoAcceptLanguagesCheckFailed:
    case CanCreateTranslatorResult::kNoExceedsLanguagePackCountLimitation:
    case CanCreateTranslatorResult::kNoServiceCrashed:
    case CanCreateTranslatorResult::kNoDisallowedByPolicy:
    case CanCreateTranslatorResult::kNoExceedsServiceCountLimitation:
      return false;
  }
}
}  // namespace

CreateTranslatorClient::CreateTranslatorClient(
    ScriptState* script_state,
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
CreateTranslatorClient::~CreateTranslatorClient() = default;

void CreateTranslatorClient::Trace(Visitor* visitor) const {
  AIMojoClient::Trace(visitor);
  visitor->Trace(translation_);
  visitor->Trace(receiver_);
  visitor->Trace(monitor_);
}

void CreateTranslatorClient::OnResult(
    mojom::blink::CreateTranslatorResultPtr result) {
  if (!GetResolver()) {
    // The request was aborted. Note: Currently abort signal is not supported.
    // TODO(crbug.com/331735396): Support abort signal.
    return;
  }
  if (result->is_translator()) {
    // TODO (crbug.com/391715395): Pass the real download progress rather than
    // mocking one.
    if (monitor_) {
      monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);
      monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                         kNormalizedDownloadProgressMax);
    }

    GetResolver()->Resolve(MakeGarbageCollected<AITranslator>(
        std::move(result->get_translator()), task_runner_,
        std::move(source_language_), std::move(target_language_)));
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

void CreateTranslatorClient::OnGotAvailability(
    CanCreateTranslatorResult result) {
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

void CreateTranslatorClient::ResetReceiver() {
  receiver_.reset();
}
}  // namespace blink
