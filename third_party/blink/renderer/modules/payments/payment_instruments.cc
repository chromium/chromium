// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_instruments.h"

#include <utility>

#include "base/location.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_object.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_instrument.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/payments/payment_manager.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

// Maximum size of a PaymentInstrument icon's type when passed over mojo.
const size_t kMaxTypeLength = 4096;

static const char kPaymentManagerUnavailable[] = "Payment manager unavailable";

template <typename IDLType>
bool rejectError(ScriptPromiseResolver<IDLType>* resolver,
                 payments::mojom::blink::PaymentHandlerStatus status) {
  switch (status) {
    case payments::mojom::blink::PaymentHandlerStatus::SUCCESS:
      return false;
    case payments::mojom::blink::PaymentHandlerStatus::NOT_FOUND:
      resolver->Resolve();
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::NO_ACTIVE_WORKER:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       "No active service worker");
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::STORAGE_OPERATION_FAILED:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       "Storage operation is failed");
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::
        FETCH_INSTRUMENT_ICON_FAILED: {
      resolver->RejectWithTypeError("Fetch or decode instrument icon failed");
      return true;
    }
    case payments::mojom::blink::PaymentHandlerStatus::
        FETCH_PAYMENT_APP_INFO_FAILED:
      // FETCH_PAYMENT_APP_INFO_FAILED indicates everything works well except
      // fetching payment handler's name and/or icon from its web app manifest.
      // The origin or name will be used to label this payment handler in
      // UI in this case, so only show warnning message instead of reject the
      // promise. The warning message was printed by
      // payment_app_info_fetcher.cc.
      return false;
  }
}

bool AllowedToUsePaymentFeatures(ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return false;
  return ExecutionContext::From(script_state)
      ->GetSecurityContext()
      .GetPermissionsPolicy()
      ->IsFeatureEnabled(mojom::blink::PermissionsPolicyFeature::kPayment);
}

void ThrowNotAllowedToUsePaymentFeatures(ExceptionState& exception_state) {
  exception_state.ThrowSecurityError(
      "Must be in a top-level browsing context or an iframe needs to specify "
      "allow=\"payment\" explicitly");
}

ScriptPromise<IDLUndefined> RejectNotAllowedToUsePaymentFeatures(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ThrowNotAllowedToUsePaymentFeatures(exception_state);
  return EmptyPromise();
}

}  // namespace

PaymentInstruments::PaymentInstruments(const PaymentManager& payment_manager,
                                       ExecutionContext* context)
    : payment_manager_(payment_manager), permission_service_(context) {}

ScriptPromise<IDLBoolean> PaymentInstruments::deleteInstrument(
    ScriptState* script_state,
    const String& instrument_key,
    ExceptionState& exception_state) {
  if (!AllowedToUsePaymentFeatures(script_state)) {
    ThrowNotAllowedToUsePaymentFeatures(exception_state);
    return EmptyPromise();
  }

  if (!payment_manager_->manager().is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kPaymentManagerUnavailable);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  payment_manager_->manager()->DeletePaymentInstrument(
      instrument_key,
      WTF::BindOnce(&PaymentInstruments::onDeletePaymentInstrument,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<PaymentInstrument> PaymentInstruments::get(
    ScriptState* script_state,
    const String& instrument_key,
    ExceptionState& exception_state) {
  if (!AllowedToUsePaymentFeatures(script_state)) {
    ThrowNotAllowedToUsePaymentFeatures(exception_state);
    return EmptyPromise();
  }

  if (!payment_manager_->manager().is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kPaymentManagerUnavailable);
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PaymentInstrument>>(
          script_state, exception_state.GetContext());

  payment_manager_->manager()->GetPaymentInstrument(
      instrument_key,
      WTF::BindOnce(&PaymentInstruments::onGetPaymentInstrument,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<IDLSequence<IDLString>> PaymentInstruments::keys(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!AllowedToUsePaymentFeatures(script_state)) {
    ThrowNotAllowedToUsePaymentFeatures(exception_state);
    return ScriptPromise<IDLSequence<IDLString>>();
  }

  if (!payment_manager_->manager().is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kPaymentManagerUnavailable);
    return ScriptPromise<IDLSequence<IDLString>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<IDLString>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  payment_manager_->manager()->KeysOfPaymentInstruments(
      WTF::BindOnce(&PaymentInstruments::onKeysOfPaymentInstruments,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLBoolean> PaymentInstruments::has(
    ScriptState* script_state,
    const String& instrument_key,
    ExceptionState& exception_state) {
  if (!AllowedToUsePaymentFeatures(script_state)) {
    ThrowNotAllowedToUsePaymentFeatures(exception_state);
    return EmptyPromise();
  }

  if (!payment_manager_->manager().is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kPaymentManagerUnavailable);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  payment_manager_->manager()->HasPaymentInstrument(
      instrument_key,
      WTF::BindOnce(&PaymentInstruments::onHasPaymentInstrument,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLUndefined> PaymentInstruments::set(
    ScriptState* script_state,
    const String& instrument_key,
    const PaymentInstrument* details,
    ExceptionState& exception_state) {
  if (!AllowedToUsePaymentFeatures(script_state))
    return RejectNotAllowedToUsePaymentFeatures(script_state, exception_state);

  if (!payment_manager_->manager().is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kPaymentManagerUnavailable);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  // TODO(crbug.com/1311953): A service worker can get here without a frame to
  // check for a user gesture. We should consider either removing the user
  // gesture requirement or not exposing PaymentInstruments to service workers.
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  bool user_gesture =
      window ? LocalFrame::HasTransientUserActivation(window->GetFrame())
             : false;

  // Should move this permission check to browser process.
  // Please see http://crbug.com/795929
  GetPermissionService(script_state)
      ->RequestPermission(
          CreatePermissionDescriptor(
              mojom::blink::PermissionName::PAYMENT_HANDLER),
          user_gesture,
          WTF::BindOnce(&PaymentInstruments::OnRequestPermission,
                        WrapPersistent(this), WrapPersistent(resolver),
                        instrument_key, WrapPersistent(details)));
  return resolver->Promise();
}

ScriptPromise<IDLUndefined> PaymentInstruments::clear(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!AllowedToUsePaymentFeatures(script_state))
    return RejectNotAllowedToUsePaymentFeatures(script_state, exception_state);

  if (!payment_manager_->manager().is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kPaymentManagerUnavailable);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  payment_manager_->manager()->ClearPaymentInstruments(
      WTF::BindOnce(&PaymentInstruments::onClearPaymentInstruments,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void PaymentInstruments::Trace(Visitor* visitor) const {
  visitor->Trace(payment_manager_);
  visitor->Trace(permission_service_);
  ScriptWrappable::Trace(visitor);
}

mojom::blink::PermissionService* PaymentInstruments::GetPermissionService(
    ScriptState* script_state) {
  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(
        ExecutionContext::From(script_state),
        permission_service_.BindNewPipeAndPassReceiver(
            ExecutionContext::From(script_state)
                ->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return permission_service_.get();
}

void PaymentInstruments::OnRequestPermission(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    const String& instrument_key,
    const PaymentInstrument* details,
    mojom::blink::PermissionStatus status) {
  DCHECK(resolver);
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  if (status != mojom::blink::PermissionStatus::GRANTED) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Not allowed to install this payment handler");
    return;
  }

  payments::mojom::blink::PaymentInstrumentPtr instrument =
      payments::mojom::blink::PaymentInstrument::New();
  instrument->name = details->hasName() ? details->name() : WTF::g_empty_string;
  if (details->hasIcons()) {
    ExecutionContext* context =
        ExecutionContext::From(resolver->GetScriptState());
    for (const ImageObject* image_object : details->icons()) {
      KURL parsed_url = context->CompleteURL(image_object->src());
      if (!parsed_url.IsValid() || !parsed_url.ProtocolIsInHTTPFamily()) {
        resolver->RejectWithTypeError("'" + image_object->src() +
                                      "' is not a valid URL.");
        return;
      }

      mojom::blink::ManifestImageResourcePtr icon =
          mojom::blink::ManifestImageResource::New();
      icon->src = parsed_url;
      // Truncate the type to avoid passing too-large strings to Mojo (see
      // https://crbug.com/810792). We could additionally verify that the type
      // is a MIME type, but the browser side will do that anyway.
      icon->type = image_object->getTypeOr("").Left(kMaxTypeLength);
      icon->purpose.push_back(blink::mojom::ManifestImageResource_Purpose::ANY);
      WebVector<gfx::Size> web_sizes =
          WebIconSizesParser::ParseIconSizes(image_object->getSizesOr(""));
      for (const auto& web_size : web_sizes) {
        icon->sizes.push_back(web_size);
      }
      instrument->icons.push_back(std::move(icon));
    }
  }

  instrument->method =
      details->hasMethod() ? details->method() : WTF::g_empty_string;
  // TODO(crbug.com/1209835): Remove stringified_capabilities entirely.
  instrument->stringified_capabilities = WTF::g_empty_string;

  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kPaymentHandler);

  payment_manager_->manager()->SetPaymentInstrument(
      instrument_key, std::move(instrument),
      WTF::BindOnce(&PaymentInstruments::onSetPaymentInstrument,
                    WrapPersistent(this), WrapPersistent(resolver)));
}

void PaymentInstruments::onDeletePaymentInstrument(
    ScriptPromiseResolver<IDLBoolean>* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  resolver->Resolve(status ==
                    payments::mojom::blink::PaymentHandlerStatus::SUCCESS);
}

void PaymentInstruments::onGetPaymentInstrument(
    ScriptPromiseResolver<PaymentInstrument>* resolver,
    payments::mojom::blink::PaymentInstrumentPtr stored_instrument,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  ScriptState::Scope scope(resolver->GetScriptState());

  if (rejectError(resolver, status))
    return;
  PaymentInstrument* instrument = PaymentInstrument::Create();
  instrument->setName(stored_instrument->name);

  HeapVector<Member<ImageObject>> icons;
  for (const auto& icon : stored_instrument->icons) {
    ImageObject* image_object = ImageObject::Create();
    image_object->setSrc(icon->src.GetString());
    image_object->setType(icon->type);
    String sizes = WTF::g_empty_string;
    for (const auto& size : icon->sizes) {
      sizes = sizes + String::Format("%dx%d ", size.width(), size.height());
    }
    image_object->setSizes(sizes.StripWhiteSpace());
    icons.push_back(image_object);
  }
  instrument->setIcons(icons);
  instrument->setMethod(stored_instrument->method);

  resolver->Resolve(instrument);
}

void PaymentInstruments::onKeysOfPaymentInstruments(
    ScriptPromiseResolver<IDLSequence<IDLString>>* resolver,
    const Vector<String>& keys,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  resolver->Resolve(keys);
}

void PaymentInstruments::onHasPaymentInstrument(
    ScriptPromiseResolver<IDLBoolean>* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  resolver->Resolve(status ==
                    payments::mojom::blink::PaymentHandlerStatus::SUCCESS);
}

void PaymentInstruments::onSetPaymentInstrument(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  resolver->Resolve();
}

void PaymentInstruments::onClearPaymentInstruments(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  resolver->Resolve();
}

}  // namespace blink
