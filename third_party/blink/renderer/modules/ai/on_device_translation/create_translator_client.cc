// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/create_translator_client.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/create_monitor.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {
using mojom::blink::CanCreateTranslatorResult;
using mojom::blink::CreateTranslatorError;

const char kExceptionMessageUnableToCreateTranslator[] =
    "Unable to create translator for the given source and target language.";

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
    case CreateTranslatorError::kInvalidStoragePartition:
      return "The Translator API is only accessible from a valid storage "
             "partition.";
  }
}

String ConvertCanCreateTranslatorResultToDebugString(
    CanCreateTranslatorResult error) {
  CreateTranslatorError equivalent_error;
  switch (error) {
    case CanCreateTranslatorResult::kReadily:
    case CanCreateTranslatorResult::kAfterDownloadLibraryNotReady:
    case CanCreateTranslatorResult::kAfterDownloadLanguagePackNotReady:
    case CanCreateTranslatorResult::
        kAfterDownloadLibraryAndLanguagePackNotReady:
    case CanCreateTranslatorResult::kAfterDownloadTranslatorCreationRequired:
      NOTREACHED();
    case CanCreateTranslatorResult::kNoNotSupportedLanguage:
      return "The language pair is unsupported.";
    case CanCreateTranslatorResult::kNoServiceCrashed:
      equivalent_error = CreateTranslatorError::kServiceCrashed;
      break;
    case CanCreateTranslatorResult::kNoDisallowedByPolicy:
      equivalent_error = CreateTranslatorError::kDisallowedByPolicy;
      break;
    case CanCreateTranslatorResult::kNoExceedsServiceCountLimitation:
      equivalent_error = CreateTranslatorError::kExceedsServiceCountLimitation;
      break;
    case CanCreateTranslatorResult::kNoInvalidStoragePartition:
      equivalent_error = CreateTranslatorError::kInvalidStoragePartition;
      break;
  }
  return ConvertCreateTranslatorErrorToDebugString(equivalent_error);
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
    case CanCreateTranslatorResult::kNoServiceCrashed:
    case CanCreateTranslatorResult::kNoDisallowedByPolicy:
    case CanCreateTranslatorResult::kNoExceedsServiceCountLimitation:
    case CanCreateTranslatorResult::kNoInvalidStoragePartition:
      return false;
  }
}

bool TranslatorIsUnavailable(CanCreateTranslatorResult result) {
  switch (result) {
    case CanCreateTranslatorResult::kReadily:
    case CanCreateTranslatorResult::kAfterDownloadLibraryNotReady:
    case CanCreateTranslatorResult::kAfterDownloadLanguagePackNotReady:
    case CanCreateTranslatorResult::
        kAfterDownloadLibraryAndLanguagePackNotReady:
    case CanCreateTranslatorResult::kAfterDownloadTranslatorCreationRequired:
      return false;
    case CanCreateTranslatorResult::kNoNotSupportedLanguage:
    case CanCreateTranslatorResult::kNoServiceCrashed:
    case CanCreateTranslatorResult::kNoDisallowedByPolicy:
    case CanCreateTranslatorResult::kNoExceedsServiceCountLimitation:
    case CanCreateTranslatorResult::kNoInvalidStoragePartition:
      return true;
  }
}

}  // namespace

CreateTranslatorClient::CreateTranslatorClient(
    ScriptState* script_state,
    TranslatorCreateOptions* options,
    ScriptPromiseResolver<Translator>* resolver)
    : ExecutionContextClient(ExecutionContext::From(script_state)),
      AIContextObserver(script_state,
                        this,
                        resolver,
                        options->getSignalOr(nullptr)),
      source_language_(options->sourceLanguage()),
      target_language_(options->targetLanguage()),
      receiver_(this, GetExecutionContext()),
      task_runner_(AIInterfaceProxy::GetTaskRunner(GetExecutionContext())) {
  if (options->hasMonitor()) {
    monitor_ = MakeGarbageCollected<CreateMonitor>(
        GetExecutionContext(), options->getSignalOr(nullptr), task_runner_);

    // If an exception is thrown, don't initiate language detection model
    // download. `AICreateMonitorCallback`'s `Invoke` will automatically
    // reject the promise with the thrown exception.
    if (options->monitor()->Invoke(nullptr, monitor_).IsNothing()) {
      return;
    }
  }

  AIInterfaceProxy::GetTranslationManagerRemote(GetExecutionContext())
      ->TranslationAvailable(
          mojom::blink::TranslatorLanguageCode::New(options->sourceLanguage()),
          mojom::blink::TranslatorLanguageCode::New(options->targetLanguage()),
          BindOnce(&CreateTranslatorClient::OnGotAvailability,
                   WrapPersistent(this)));
}
CreateTranslatorClient::~CreateTranslatorClient() = default;

void CreateTranslatorClient::Trace(Visitor* visitor) const {
  AIContextObserver::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(receiver_);
  visitor->Trace(monitor_);
}

void CreateTranslatorClient::OnResult(
    mojom::blink::CreateTranslatorResultPtr result,
    mojom::blink::TranslatorLanguageCodePtr source_language,
    mojom::blink::TranslatorLanguageCodePtr target_language) {
  // Call `Cleanup` when this function returns.
  RunOnDestruction run_on_destruction(
      BindOnce(&CreateTranslatorClient::Cleanup, WrapWeakPersistent(this)));

  if (!GetResolver()) {
    // The request was aborted. Note: Currently abort signal is not supported.
    // TODO(crbug.com/331735396): Support abort signal.
    return;
  }

  if (!result->is_translator()) {
    CHECK(result->is_error());
    CHECK(!source_language);
    CHECK(!target_language);

    GetExecutionContext()->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        ConvertCreateTranslatorErrorToDebugString(result->get_error()));
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kNotSupportedError,
        kExceptionMessageUnableToCreateTranslator);
    return;
  }

  CHECK(source_language);
  CHECK(target_language);
  source_language_ = source_language->code;
  target_language_ = target_language->code;

  if (monitor_) {
    // Ensure that a download completion event is sent.
    monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);

    // Abort may have been triggered by `OnDownloadProgressUpdate`.
    if (!this->GetResolver()) {
      return;
    }

    // Ensure that a download completion event is sent.
    monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                       kNormalizedDownloadProgressMax);

    // Abort may have been triggered by `OnDownloadProgressUpdate`.
    if (!this->GetResolver()) {
      return;
    }
  }

  GetResolver()->Resolve(MakeGarbageCollected<Translator>(
      GetScriptState(), std::move(result->get_translator()), task_runner_,
      std::move(source_language_), std::move(target_language_),
      GetAbortSignal()));
}

void CreateTranslatorClient::OnGotAvailability(
    CanCreateTranslatorResult result) {
  ScriptState* script_state = GetScriptState();
  ExecutionContext* context = GetExecutionContext();
  LocalDOMWindow* const window = LocalDOMWindow::From(script_state);

  if (TranslatorIsUnavailable(result)) {
    GetExecutionContext()->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        ConvertCanCreateTranslatorResultToDebugString(result));
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kNotSupportedError,
        kExceptionMessageUnableToCreateTranslator);
    return;
  }

  // The Translator API is only available within a window or extension
  // service worker context. User activation is not consumed by workers, as
  // they lack the ability to do so.
  CHECK(window != nullptr || context->IsServiceWorkerGlobalScope());

  if (!context->IsServiceWorkerGlobalScope() &&
      RequiresUserActivation(result) &&
      !LocalFrame::ConsumeTransientUserActivation(window->GetFrame())) {
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        kExceptionMessageUserActivationRequired);
    return;
  }
  mojo::PendingRemote<mojom::blink::TranslationManagerCreateTranslatorClient>
      client;

  receiver_.Bind(client.InitWithNewPipeAndPassReceiver(), task_runner_);

  mojo::PendingRemote<mojom::blink::ModelDownloadProgressObserver>
      progress_observer;

  if (monitor_) {
    progress_observer = monitor_->BindRemote();
  }

  AIInterfaceProxy::GetTranslationManagerRemote(GetExecutionContext())
      ->CreateTranslator(
          std::move(client),
          mojom::blink::TranslatorCreateOptions::New(
              mojom::blink::TranslatorLanguageCode::New(source_language_),
              mojom::blink::TranslatorLanguageCode::New(target_language_),
              std::move(progress_observer)));
}

void CreateTranslatorClient::ResetReceiver() {
  receiver_.reset();
}
}  // namespace blink
