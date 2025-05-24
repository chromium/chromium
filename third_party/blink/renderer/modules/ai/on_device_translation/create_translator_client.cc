// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/create_translator_client.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
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
    monitor_ = MakeGarbageCollected<CreateMonitor>(GetExecutionContext(),
                                                   task_runner_);
    std::ignore = options->monitor()->Invoke(nullptr, monitor_);
  }
}
CreateTranslatorClient::~CreateTranslatorClient() = default;

void CreateTranslatorClient::Trace(Visitor* visitor) const {
  AIContextObserver::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(receiver_);
  visitor->Trace(monitor_);
}

void CreateTranslatorClient::OnResult(
    mojom::blink::CreateTranslatorResultPtr result) {
  // Call `Cleanup` when this function returns.
  RunOnDestruction run_on_destruction(WTF::BindOnce(
      &CreateTranslatorClient::Cleanup, WrapWeakPersistent(this)));

  if (!GetResolver()) {
    // The request was aborted. Note: Currently abort signal is not supported.
    // TODO(crbug.com/331735396): Support abort signal.
    return;
  }

  if (!result->is_translator()) {
    CHECK(result->is_error());
    GetExecutionContext()->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        ConvertCreateTranslatorErrorToDebugString(result->get_error()));
    GetResolver()->Reject(DOMException::Create(
        kExceptionMessageUnableToCreateTranslator,
        DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
    return;
  }

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
  ExecutionContext* context = ExecutionContext::From(script_state);
  LocalDOMWindow* const window = LocalDOMWindow::From(script_state);

  // The Translator API is only available within a window or extension
  // service worker context. User activation is not consumed by workers, as
  // they lack the ability to do so.
  CHECK(window != nullptr || context->IsServiceWorkerGlobalScope());

  if (RuntimeEnabledFeatures::TranslationAPIV1Enabled() &&
      !context->IsServiceWorkerGlobalScope() &&
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
