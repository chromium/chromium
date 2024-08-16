// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_event.h"

#include <utility>

#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_address_errors.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_currency_amount.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_details_modifier.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_item.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_method_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_request_details_update.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_shipping_option.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/payments/address_init_type_converter.h"
#include "third_party/blink/renderer/modules/payments/payments_validators.h"
#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

PaymentRequestEvent* PaymentRequestEvent::Create(
    const AtomicString& type,
    const PaymentRequestEventInit* initializer,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost> host,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer,
    ExecutionContext* execution_context) {
  return MakeGarbageCollected<PaymentRequestEvent>(
      type, initializer, std::move(host), respond_with_observer,
      wait_until_observer, execution_context);
}

// TODO(crbug.com/1070871): Use fooOr() in members' initializers.
PaymentRequestEvent::PaymentRequestEvent(
    const AtomicString& type,
    const PaymentRequestEventInit* initializer,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost> host,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer,
    ExecutionContext* execution_context)
    : ExtendableEvent(type, initializer, wait_until_observer),
      top_origin_(initializer->hasTopOrigin() ? initializer->topOrigin()
                                              : String()),
      payment_request_origin_(initializer->hasPaymentRequestOrigin()
                                  ? initializer->paymentRequestOrigin()
                                  : String()),
      payment_request_id_(initializer->hasPaymentRequestId()
                              ? initializer->paymentRequestId()
                              : String()),
      method_data_(initializer->hasMethodData()
                       ? initializer->methodData()
                       : HeapVector<Member<PaymentMethodData>>()),
      total_(initializer->hasTotal() ? initializer->total()
                                     : PaymentCurrencyAmount::Create()),
      modifiers_(initializer->hasModifiers()
                     ? initializer->modifiers()
                     : HeapVector<Member<PaymentDetailsModifier>>()),
      instrument_key_(initializer->hasInstrumentKey()
                          ? initializer->instrumentKey()
                          : String()),
      payment_options_(initializer->hasPaymentOptions()
                           ? initializer->paymentOptions()
                           : PaymentOptions::Create()),
      shipping_options_(initializer->hasShippingOptions()
                            ? initializer->shippingOptions()
                            : HeapVector<Member<PaymentShippingOption>>()),
      observer_(respond_with_observer),
      payment_handler_host_(execution_context) {
  if (!host.is_valid())
    return;

  if (execution_context) {
    payment_handler_host_.Bind(
        std::move(host),
        execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
    payment_handler_host_.set_disconnect_handler(WTF::BindOnce(
        &PaymentRequestEvent::OnHostConnectionError, WrapWeakPersistent(this)));
  }
}

PaymentRequestEvent::~PaymentRequestEvent() = default;

const AtomicString& PaymentRequestEvent::InterfaceName() const {
  return event_interface_names::kPaymentRequestEvent;
}

const String& PaymentRequestEvent::topOrigin() const {
  return top_origin_;
}

const String& PaymentRequestEvent::paymentRequestOrigin() const {
  return payment_request_origin_;
}

const String& PaymentRequestEvent::paymentRequestId() const {
  return payment_request_id_;
}

const HeapVector<Member<PaymentMethodData>>& PaymentRequestEvent::methodData()
    const {
  return method_data_;
}

const ScriptValue PaymentRequestEvent::total(ScriptState* script_state) const {
  return ScriptValue::From(script_state, total_.Get());
}

const HeapVector<Member<PaymentDetailsModifier>>&
PaymentRequestEvent::modifiers() const {
  return modifiers_;
}

const String& PaymentRequestEvent::instrumentKey() const {
  return instrument_key_;
}

const ScriptValue PaymentRequestEvent::paymentOptions(
    ScriptState* script_state) const {
  if (!payment_options_)
    return ScriptValue::CreateNull(script_state->GetIsolate());
  return ScriptValue::From(script_state, payment_options_.Get());
}

std::optional<HeapVector<Member<PaymentShippingOption>>>
PaymentRequestEvent::shippingOptions() const {
  if (shipping_options_.empty())
    return std::nullopt;
  return shipping_options_;
}

ScriptPromise<IDLNullable<ServiceWorkerWindowClient>>
PaymentRequestEvent::openWindow(ScriptState* script_state, const String& url) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<ServiceWorkerWindowClient>>>(
      script_state);
  auto promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!isTrusted()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Cannot open a window when the event is not trusted"));
    return promise;
  }

  KURL parsed_url_to_open = context->CompleteURL(url);
  if (!parsed_url_to_open.IsValid()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "'" + url + "' is not a valid URL."));
    return promise;
  }

  if (!context->GetSecurityOrigin()->IsSameOriginWith(
          SecurityOrigin::Create(parsed_url_to_open).get())) {
    resolver->Resolve(nullptr);
    return promise;
  }

  if (!context->IsWindowInteractionAllowed()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Not allowed to open a window without user activation"));
    return promise;
  }
  context->ConsumeWindowInteraction();

  To<ServiceWorkerGlobalScope>(context)
      ->GetServiceWorkerHost()
      ->OpenPaymentHandlerWindow(
          parsed_url_to_open,
          ServiceWorkerWindowClient::CreateResolveWindowClientCallback(
              resolver));
  return promise;
}

ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>
PaymentRequestEvent::changePaymentMethod(ScriptState* script_state,
                                         const String& method_name,
                                         const ScriptValue& method_details,
                                         ExceptionState& exception_state) {
  if (change_payment_request_details_resolver_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Waiting for response to the previous "
                                      "payment request details change");
    return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }

  if (!payment_handler_host_.is_bound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "No corresponding PaymentRequest object found");
    return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }

  auto method_data = payments::mojom::blink::PaymentHandlerMethodData::New();
  if (!method_details.IsNull()) {
    DCHECK(!method_details.IsEmpty());
    PaymentsValidators::ValidateAndStringifyObject(
        script_state->GetIsolate(), method_details,
        method_data->stringified_data, exception_state);
    if (exception_state.HadException())
      return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }

  method_data->method_name = method_name;
  payment_handler_host_->ChangePaymentMethod(
      std::move(method_data),
      WTF::BindOnce(&PaymentRequestEvent::OnChangePaymentRequestDetailsResponse,
                    WrapWeakPersistent(this)));
  change_payment_request_details_resolver_ = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<PaymentRequestDetailsUpdate>>>(
      script_state);
  return change_payment_request_details_resolver_->Promise();
}

ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>
PaymentRequestEvent::changeShippingAddress(ScriptState* script_state,
                                           AddressInit* shipping_address,
                                           ExceptionState& exception_state) {
  if (change_payment_request_details_resolver_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Waiting for response to the previous "
                                      "payment request details change");
    return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }

  if (!payment_handler_host_.is_bound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "No corresponding PaymentRequest object found");
    return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }
  if (!shipping_address) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Shipping address cannot be null");
    return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }

  auto shipping_address_ptr =
      payments::mojom::blink::PaymentAddress::From(shipping_address);
  String shipping_address_error;
  if (!PaymentsValidators::IsValidShippingAddress(script_state->GetIsolate(),
                                                  shipping_address_ptr,
                                                  &shipping_address_error)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      shipping_address_error);
    return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }

  payment_handler_host_->ChangeShippingAddress(
      std::move(shipping_address_ptr),
      WTF::BindOnce(&PaymentRequestEvent::OnChangePaymentRequestDetailsResponse,
                    WrapWeakPersistent(this)));
  change_payment_request_details_resolver_ = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<PaymentRequestDetailsUpdate>>>(
      script_state);
  return change_payment_request_details_resolver_->Promise();
}

ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>
PaymentRequestEvent::changeShippingOption(ScriptState* script_state,
                                          const String& shipping_option_id,
                                          ExceptionState& exception_state) {
  if (change_payment_request_details_resolver_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Waiting for response to the previous payment request details change");
    return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }

  if (!payment_handler_host_.is_bound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "No corresponding PaymentRequest object found");
    return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }

  bool shipping_option_id_is_valid = false;
  for (const auto& option : shipping_options_) {
    if (option->id() == shipping_option_id) {
      shipping_option_id_is_valid = true;
      break;
    }
  }
  if (!shipping_option_id_is_valid) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Shipping option identifier is invalid");
    return ScriptPromise<IDLNullable<PaymentRequestDetailsUpdate>>();
  }

  payment_handler_host_->ChangeShippingOption(
      shipping_option_id,
      WTF::BindOnce(&PaymentRequestEvent::OnChangePaymentRequestDetailsResponse,
                    WrapWeakPersistent(this)));
  change_payment_request_details_resolver_ = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<PaymentRequestDetailsUpdate>>>(
      script_state);
  return change_payment_request_details_resolver_->Promise();
}

void PaymentRequestEvent::respondWith(ScriptState* script_state,
                                      ScriptPromiseUntyped script_promise,
                                      ExceptionState& exception_state) {
  if (!isTrusted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot respond with data when the event is not trusted");
    return;
  }

  stopImmediatePropagation();
  if (observer_) {
    observer_->RespondWith(script_state, script_promise, exception_state);
  }
}

void PaymentRequestEvent::Trace(Visitor* visitor) const {
  visitor->Trace(method_data_);
  visitor->Trace(total_);
  visitor->Trace(modifiers_);
  visitor->Trace(payment_options_);
  visitor->Trace(shipping_options_);
  visitor->Trace(change_payment_request_details_resolver_);
  visitor->Trace(observer_);
  visitor->Trace(payment_handler_host_);
  ExtendableEvent::Trace(visitor);
}

void PaymentRequestEvent::OnChangePaymentRequestDetailsResponse(
    payments::mojom::blink::PaymentRequestDetailsUpdatePtr response) {
  if (!change_payment_request_details_resolver_)
    return;

  auto* dictionary = MakeGarbageCollected<PaymentRequestDetailsUpdate>();
  if (!response->error.IsNull() && !response->error.empty()) {
    dictionary->setError(response->error);
  }

  if (response->total) {
    auto* total = MakeGarbageCollected<PaymentCurrencyAmount>();
    total->setCurrency(response->total->currency);
    total->setValue(response->total->value);
    dictionary->setTotal(total);
  }

  ScriptState* script_state =
      change_payment_request_details_resolver_->GetScriptState();
  ScriptState::Scope scope(script_state);
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kConstructor,
                                 "PaymentDetailsModifier");

  if (response->modifiers) {
    HeapVector<Member<PaymentDetailsModifier>> modifiers;
    for (const auto& response_modifier : *response->modifiers) {
      if (!response_modifier)
        continue;

      auto* mod = MakeGarbageCollected<PaymentDetailsModifier>();
      mod->setSupportedMethod(response_modifier->method_data->method_name);

      if (response_modifier->total) {
        auto* amount = MakeGarbageCollected<PaymentCurrencyAmount>();
        amount->setCurrency(response_modifier->total->currency);
        amount->setValue(response_modifier->total->value);
        auto* total = MakeGarbageCollected<PaymentItem>();
        total->setAmount(amount);
        total->setLabel("");
        mod->setTotal(total);
      }

      if (!response_modifier->method_data->stringified_data.empty()) {
        v8::Local<v8::Value> parsed_value = FromJSONString(
            script_state->GetIsolate(), script_state->GetContext(),
            response_modifier->method_data->stringified_data, exception_state);
        if (exception_state.HadException()) {
          change_payment_request_details_resolver_->Reject(
              MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                                 exception_state.Message()));
          change_payment_request_details_resolver_.Clear();
          return;
        }
        mod->setData(ScriptValue(script_state->GetIsolate(), parsed_value));
        modifiers.emplace_back(mod);
      }
    }
    dictionary->setModifiers(modifiers);
  }

  if (response->shipping_options) {
    HeapVector<Member<PaymentShippingOption>> shipping_options;
    for (const auto& response_shipping_option : *response->shipping_options) {
      if (!response_shipping_option)
        continue;

      auto* shipping_option = MakeGarbageCollected<PaymentShippingOption>();
      auto* amount = MakeGarbageCollected<PaymentCurrencyAmount>();
      amount->setCurrency(response_shipping_option->amount->currency);
      amount->setValue(response_shipping_option->amount->value);
      shipping_option->setAmount(amount);
      shipping_option->setId(response_shipping_option->id);
      shipping_option->setLabel(response_shipping_option->label);
      shipping_option->setSelected(response_shipping_option->selected);
      shipping_options.emplace_back(shipping_option);
    }
    dictionary->setShippingOptions(shipping_options);
  }

  if (response->stringified_payment_method_errors &&
      !response->stringified_payment_method_errors.empty()) {
    v8::Local<v8::Value> parsed_value = FromJSONString(
        script_state->GetIsolate(), script_state->GetContext(),
        response->stringified_payment_method_errors, exception_state);
    if (exception_state.HadException()) {
      change_payment_request_details_resolver_->Reject(
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                             exception_state.Message()));
      change_payment_request_details_resolver_.Clear();
      return;
    }
    dictionary->setPaymentMethodErrors(
        ScriptValue(script_state->GetIsolate(), parsed_value));
  }

  if (response->shipping_address_errors) {
    auto* shipping_address_errors = MakeGarbageCollected<AddressErrors>();
    shipping_address_errors->setAddressLine(
        response->shipping_address_errors->address_line);
    shipping_address_errors->setCity(response->shipping_address_errors->city);
    shipping_address_errors->setCountry(
        response->shipping_address_errors->country);
    shipping_address_errors->setDependentLocality(
        response->shipping_address_errors->dependent_locality);
    shipping_address_errors->setOrganization(
        response->shipping_address_errors->organization);
    shipping_address_errors->setPhone(response->shipping_address_errors->phone);
    shipping_address_errors->setPostalCode(
        response->shipping_address_errors->postal_code);
    shipping_address_errors->setRecipient(
        response->shipping_address_errors->recipient);
    shipping_address_errors->setRegion(
        response->shipping_address_errors->region);
    shipping_address_errors->setSortingCode(
        response->shipping_address_errors->sorting_code);
    dictionary->setShippingAddressErrors(shipping_address_errors);
  }

  change_payment_request_details_resolver_->Resolve(
      dictionary->hasError() || dictionary->hasTotal() ||
              dictionary->hasModifiers() ||
              dictionary->hasPaymentMethodErrors() ||
              dictionary->hasShippingOptions() ||
              dictionary->hasShippingAddressErrors()
          ? dictionary
          : nullptr);
  change_payment_request_details_resolver_.Clear();
}

void PaymentRequestEvent::OnHostConnectionError() {
  if (change_payment_request_details_resolver_) {
    change_payment_request_details_resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "Browser process disconnected"));
  }
  change_payment_request_details_resolver_.Clear();
  payment_handler_host_.reset();
}

}  // namespace blink
