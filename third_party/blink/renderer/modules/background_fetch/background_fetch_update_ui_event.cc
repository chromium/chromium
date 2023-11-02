// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_update_ui_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_background_fetch_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_background_fetch_ui_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_resource.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_icon_loader.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/event_interface_modules_names.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

BackgroundFetchUpdateUIEvent::BackgroundFetchUpdateUIEvent(
    const AtomicString& type,
    const BackgroundFetchEventInit* initializer)
    : BackgroundFetchEvent(type, initializer, nullptr /* observer */) {}

BackgroundFetchUpdateUIEvent::BackgroundFetchUpdateUIEvent(
    const AtomicString& type,
    const BackgroundFetchEventInit* initializer,
    WaitUntilObserver* observer,
    ServiceWorkerRegistration* registration)
    : BackgroundFetchEvent(type, initializer, observer),
      service_worker_registration_(registration) {}

BackgroundFetchUpdateUIEvent::~BackgroundFetchUpdateUIEvent() = default;

void BackgroundFetchUpdateUIEvent::Trace(Visitor* visitor) const {
  visitor->Trace(service_worker_registration_);
  visitor->Trace(loader_);
  BackgroundFetchEvent::Trace(visitor);
}

ScriptPromise BackgroundFetchUpdateUIEvent::updateUI(
    ScriptState* script_state,
    const BackgroundFetchUIOptions* ui_options,
    ExceptionState& exception_state) {
  if (observer_ && !observer_->IsEventActive()) {
    // Return a rejected promise as the event is no longer active.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "ExtendableEvent is no longer active.");
    return ScriptPromise();
  }
  if (update_ui_called_) {
    // Return a rejected promise as this method should only be called once.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "updateUI may only be called once.");
    return ScriptPromise();
  }

  update_ui_called_ = true;

  if (!service_worker_registration_) {
    // Return a Promise that will never settle when a developer calls this
    // method on a BackgroundFetchSuccessEvent instance they created themselves.
    // TODO(crbug.com/872768): Figure out if this is the right thing to do
    // vs reacting eagerly.
    return ScriptPromise();
  }

  if (!ui_options->hasTitle() && ui_options->icons().empty()) {
    // Nothing to update, just return a resolved promise.
    return ScriptPromise::CastUndefined(script_state);
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (ui_options->icons().empty()) {
    DidGetIcon(resolver, ui_options->title(), SkBitmap(),
               -1 /* ideal_to_chosen_icon_size */);
  } else {
    DCHECK(!loader_);
    loader_ = MakeGarbageCollected<BackgroundFetchIconLoader>();
    DCHECK(loader_);
    loader_->Start(BackgroundFetchBridge::From(service_worker_registration_),
                   ExecutionContext::From(script_state), ui_options->icons(),
                   WTF::BindOnce(&BackgroundFetchUpdateUIEvent::DidGetIcon,
                                 WrapPersistent(this), WrapPersistent(resolver),
                                 ui_options->title()));
  }

  return promise;
}

void BackgroundFetchUpdateUIEvent::DidGetIcon(
    ScriptPromiseResolver* resolver,
    const String& title,
    const SkBitmap& icon,
    int64_t ideal_to_chosen_icon_size) {
  registration()->UpdateUI(
      title, icon,
      WTF::BindOnce(&BackgroundFetchUpdateUIEvent::DidUpdateUI,
                    WrapPersistent(this), WrapPersistent(resolver)));
}

void BackgroundFetchUpdateUIEvent::DidUpdateUI(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error) {
  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
    case mojom::blink::BackgroundFetchError::INVALID_ID:
      resolver->Resolve();
      return;
    case mojom::blink::BackgroundFetchError::STORAGE_ERROR:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Failed to update UI due to I/O error."));
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_DEVELOPER_ID:
    case mojom::blink::BackgroundFetchError::INVALID_ARGUMENT:
    case mojom::blink::BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE:
    case mojom::blink::BackgroundFetchError::PERMISSION_DENIED:
    case mojom::blink::BackgroundFetchError::QUOTA_EXCEEDED:
    case mojom::blink::BackgroundFetchError::REGISTRATION_LIMIT_EXCEEDED:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

}  // namespace blink
