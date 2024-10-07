// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_index.h"

#include <optional>

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_icon_definition.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/content_index/content_description_type_converter.h"
#include "third_party/blink/renderer/modules/content_index/content_index_icon_loader.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

// Validates |description|. If there is an error, an error message to be passed
// to a TypeError is passed. Otherwise a null string is returned.
WTF::String ValidateDescription(const ContentDescription& description,
                                ServiceWorkerRegistration* registration) {
  // TODO(crbug.com/973844): Should field sizes be capped?

  if (description.id().empty())
    return "ID cannot be empty";

  if (description.title().empty())
    return "Title cannot be empty";

  if (description.description().empty())
    return "Description cannot be empty";

  if (description.url().empty())
    return "Invalid launch URL provided";

  for (const auto& icon : description.icons()) {
    if (icon->src().empty())
      return "Invalid icon URL provided";
    KURL icon_url =
        registration->GetExecutionContext()->CompleteURL(icon->src());
    if (!icon_url.ProtocolIsInHTTPFamily())
      return "Invalid icon URL protocol";
  }

  KURL launch_url =
      registration->GetExecutionContext()->CompleteURL(description.url());
  auto* security_origin =
      registration->GetExecutionContext()->GetSecurityOrigin();
  if (!security_origin->CanRequest(launch_url))
    return "Service Worker cannot request provided launch URL";

  if (!launch_url.GetString().StartsWith(registration->scope()))
    return "Launch URL must belong to the Service Worker's scope";

  return WTF::String();
}

}  // namespace

ContentIndex::ContentIndex(ServiceWorkerRegistration* registration,
                           scoped_refptr<base::SequencedTaskRunner> task_runner)
    : registration_(registration),
      task_runner_(std::move(task_runner)),
      content_index_service_(registration->GetExecutionContext()) {
  DCHECK(registration_);
}

ContentIndex::~ContentIndex() = default;

ScriptPromise<IDLUndefined> ContentIndex::add(
    ScriptState* script_state,
    const ContentDescription* description,
    ExceptionState& exception_state) {
  if (!registration_->active()) {
    exception_state.ThrowTypeError(
        "No active registration available on the ServiceWorkerRegistration.");
    return EmptyPromise();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "ContentIndex is not allowed in fenced frames.");
    return EmptyPromise();
  }

  WTF::String description_error =
      ValidateDescription(*description, registration_.Get());
  if (!description_error.IsNull()) {
    exception_state.ThrowTypeError(description_error);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  auto mojo_description = mojom::blink::ContentDescription::From(description);
  auto category = mojo_description->category;
  GetService()->GetIconSizes(
      category,
      WTF::BindOnce(&ContentIndex::DidGetIconSizes, WrapPersistent(this),
                    std::move(mojo_description), WrapPersistent(resolver)));

  return promise;
}

void ContentIndex::DidGetIconSizes(
    mojom::blink::ContentDescriptionPtr description,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    const Vector<gfx::Size>& icon_sizes) {
  if (!icon_sizes.empty() && description->icons.empty()) {
    resolver->RejectWithTypeError("icons must be provided");
    return;
  }

  if (!registration_->GetExecutionContext()) {
    // The SW execution context is not valid for some reason. Bail out.
    resolver->RejectWithTypeError("Service worker is no longer valid.");
    return;
  }

  if (icon_sizes.empty()) {
    DidGetIcons(resolver, std::move(description), /* icons= */ {});
    return;
  }

  auto* icon_loader = MakeGarbageCollected<ContentIndexIconLoader>();
  icon_loader->Start(
      registration_->GetExecutionContext(), std::move(description), icon_sizes,
      WTF::BindOnce(&ContentIndex::DidGetIcons, WrapPersistent(this),
                    WrapPersistent(resolver)));
}

void ContentIndex::DidGetIcons(ScriptPromiseResolver<IDLUndefined>* resolver,
                               mojom::blink::ContentDescriptionPtr description,
                               Vector<SkBitmap> icons) {
  for (const auto& icon : icons) {
    if (icon.isNull()) {
      resolver->RejectWithTypeError("Icon could not be loaded");
      return;
    }
  }

  if (!registration_->GetExecutionContext()) {
    // The SW execution context is not valid for some reason. Bail out.
    resolver->RejectWithTypeError("Service worker is no longer valid.");
    return;
  }

  KURL launch_url = registration_->GetExecutionContext()->CompleteURL(
      description->launch_url);

  GetService()->Add(
      registration_->RegistrationId(), std::move(description), icons,
      launch_url,
      WTF::BindOnce(&ContentIndex::DidAdd, WrapPersistent(resolver)));
}

void ContentIndex::DidAdd(ScriptPromiseResolver<IDLUndefined>* resolver,
                          mojom::blink::ContentIndexError error) {
  switch (error) {
    case mojom::blink::ContentIndexError::NONE:
      resolver->Resolve();
      return;
    case mojom::blink::ContentIndexError::STORAGE_ERROR:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kAbortError,
          "Failed to add description due to I/O error.");
      return;
    case mojom::blink::ContentIndexError::INVALID_PARAMETER:
      // The renderer should have been killed.
      NOTREACHED_IN_MIGRATION();
      return;
    case mojom::blink::ContentIndexError::NO_SERVICE_WORKER:
      resolver->RejectWithTypeError("Service worker must be active");
      return;
  }
}

ScriptPromise<IDLUndefined> ContentIndex::deleteDescription(
    ScriptState* script_state,
    const String& id,
    ExceptionState& exception_state) {
  if (!registration_->active()) {
    exception_state.ThrowTypeError(
        "No active registration available on the ServiceWorkerRegistration.");
    return EmptyPromise();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "ContentIndex is not allowed in fenced frames.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  GetService()->Delete(registration_->RegistrationId(), id,
                       WTF::BindOnce(&ContentIndex::DidDeleteDescription,
                                     WrapPersistent(resolver)));

  return promise;
}

void ContentIndex::DidDeleteDescription(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    mojom::blink::ContentIndexError error) {
  switch (error) {
    case mojom::blink::ContentIndexError::NONE:
      resolver->Resolve();
      return;
    case mojom::blink::ContentIndexError::STORAGE_ERROR:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kAbortError,
          "Failed to delete description due to I/O error.");
      return;
    case mojom::blink::ContentIndexError::INVALID_PARAMETER:
      // The renderer should have been killed.
      NOTREACHED_IN_MIGRATION();
      return;
    case mojom::blink::ContentIndexError::NO_SERVICE_WORKER:
      // This value shouldn't apply to this callback.
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

ScriptPromise<IDLSequence<ContentDescription>> ContentIndex::getDescriptions(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!registration_->active()) {
    exception_state.ThrowTypeError(
        "No active registration available on the ServiceWorkerRegistration.");
    return ScriptPromise<IDLSequence<ContentDescription>>();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "ContentIndex is not allowed in fenced frames.");
    return ScriptPromise<IDLSequence<ContentDescription>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<ContentDescription>>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  GetService()->GetDescriptions(registration_->RegistrationId(),
                                WTF::BindOnce(&ContentIndex::DidGetDescriptions,
                                              WrapPersistent(resolver)));

  return promise;
}

void ContentIndex::DidGetDescriptions(
    ScriptPromiseResolver<IDLSequence<ContentDescription>>* resolver,
    mojom::blink::ContentIndexError error,
    Vector<mojom::blink::ContentDescriptionPtr> descriptions) {
  HeapVector<Member<ContentDescription>> blink_descriptions;
  blink_descriptions.reserve(descriptions.size());
  for (const auto& description : descriptions)
    blink_descriptions.push_back(description.To<blink::ContentDescription*>());

  switch (error) {
    case mojom::blink::ContentIndexError::NONE:
      resolver->Resolve(std::move(blink_descriptions));
      return;
    case mojom::blink::ContentIndexError::STORAGE_ERROR:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kAbortError,
          "Failed to get descriptions due to I/O error."));
      return;
    case mojom::blink::ContentIndexError::INVALID_PARAMETER:
      // The renderer should have been killed.
      NOTREACHED_IN_MIGRATION();
      return;
    case mojom::blink::ContentIndexError::NO_SERVICE_WORKER:
      // This value shouldn't apply to this callback.
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void ContentIndex::Trace(Visitor* visitor) const {
  visitor->Trace(registration_);
  visitor->Trace(content_index_service_);
  ScriptWrappable::Trace(visitor);
}

mojom::blink::ContentIndexService* ContentIndex::GetService() {
  if (!content_index_service_.is_bound()) {
    registration_->GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .GetInterface(
            content_index_service_.BindNewPipeAndPassReceiver(task_runner_));
  }
  return content_index_service_.get();
}

}  // namespace blink
