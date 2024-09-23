// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_watch_advertisements_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_attribute_instance_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_server.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

const char kAbortErrorMessage[] = "The Bluetooth operation was cancelled.";
const char kInactiveDocumentError[] = "Document not active";
const char kInvalidStateErrorMessage[] =
    "Pending watch advertisements operation.";

BluetoothDevice::BluetoothDevice(ExecutionContext* context,
                                 mojom::blink::WebBluetoothDevicePtr device,
                                 Bluetooth* bluetooth)
    : ExecutionContextClient(context),
      ActiveScriptWrappable<BluetoothDevice>({}),
      attribute_instance_map_(
          MakeGarbageCollected<BluetoothAttributeInstanceMap>(this)),
      device_(std::move(device)),
      gatt_(MakeGarbageCollected<BluetoothRemoteGATTServer>(context, this)),
      bluetooth_(bluetooth),
      client_receiver_(this, context) {}

BluetoothRemoteGATTService* BluetoothDevice::GetOrCreateRemoteGATTService(
    mojom::blink::WebBluetoothRemoteGATTServicePtr service,
    bool is_primary,
    const String& device_instance_id) {
  return attribute_instance_map_->GetOrCreateRemoteGATTService(
      std::move(service), is_primary, device_instance_id);
}

bool BluetoothDevice::IsValidService(const String& service_instance_id) {
  return attribute_instance_map_->ContainsService(service_instance_id);
}

BluetoothRemoteGATTCharacteristic*
BluetoothDevice::GetOrCreateRemoteGATTCharacteristic(
    ExecutionContext* context,
    mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr characteristic,
    BluetoothRemoteGATTService* service) {
  return attribute_instance_map_->GetOrCreateRemoteGATTCharacteristic(
      context, std::move(characteristic), service);
}

bool BluetoothDevice::IsValidCharacteristic(
    const String& characteristic_instance_id) {
  return attribute_instance_map_->ContainsCharacteristic(
      characteristic_instance_id);
}

BluetoothRemoteGATTDescriptor*
BluetoothDevice::GetOrCreateBluetoothRemoteGATTDescriptor(
    mojom::blink::WebBluetoothRemoteGATTDescriptorPtr descriptor,
    BluetoothRemoteGATTCharacteristic* characteristic) {
  return attribute_instance_map_->GetOrCreateBluetoothRemoteGATTDescriptor(
      std::move(descriptor), characteristic);
}

bool BluetoothDevice::IsValidDescriptor(const String& descriptor_instance_id) {
  return attribute_instance_map_->ContainsDescriptor(descriptor_instance_id);
}

void BluetoothDevice::ClearAttributeInstanceMapAndFireEvent() {
  attribute_instance_map_->Clear();
  DispatchEvent(
      *Event::CreateBubble(event_type_names::kGattserverdisconnected));
}

const WTF::AtomicString& BluetoothDevice::InterfaceName() const {
  return event_target_names::kBluetoothDevice;
}

ExecutionContext* BluetoothDevice::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void BluetoothDevice::Trace(Visitor* visitor) const {
  visitor->Trace(attribute_instance_map_);
  visitor->Trace(gatt_);
  visitor->Trace(bluetooth_);
  visitor->Trace(watch_advertisements_resolver_);
  visitor->Trace(client_receiver_);
  visitor->Trace(abort_handle_map_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

// https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothdevice-watchadvertisements
ScriptPromise<IDLUndefined> BluetoothDevice::watchAdvertisements(
    ScriptState* script_state,
    const WatchAdvertisementsOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    exception_state.ThrowTypeError(kInactiveDocumentError);
    return EmptyPromise();
  }

  CHECK(context->IsSecureContext());

  // 1. If options.signal is present, perform the following sub-steps:
  if (options->hasSignal()) {
    // 1.1. If options.signal’s aborted flag is set, then abort
    // watchAdvertisements with this and abort these steps.
    if (options->signal()->aborted()) {
      AbortWatchAdvertisements(options->signal());
      exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                        kAbortErrorMessage);
      return EmptyPromise();
    }

    // 1.2. Add the following abort steps to options.signal:
    // 1.2.1. Abort watchAdvertisements with this.
    // 1.2.2. Reject promise with AbortError.
    if (!abort_handle_map_.Contains(options->signal())) {
      auto* handle = options->signal()->AddAlgorithm(WTF::BindOnce(
          &BluetoothDevice::AbortWatchAdvertisements, WrapWeakPersistent(this),
          WrapWeakPersistent(options->signal())));
      abort_handle_map_.insert(options->signal(), handle);
    }
  }

  // 2. If this.[[watchAdvertisementsState]] is 'pending-watch':
  if (client_receiver_.is_bound() && watch_advertisements_resolver_) {
    // 'pending-watch' 2.1. Reject promise with InvalidStateError.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidStateErrorMessage);
    return EmptyPromise();
  }

  // 2. If this.[[watchAdvertisementsState]] is 'watching':
  // 'watching' 2.1. Resolve promise with undefined.
  if (client_receiver_.is_bound() && !watch_advertisements_resolver_)
    return ToResolvedUndefinedPromise(script_state);

  // 2. If this.[[watchAdvertisementsState]] is 'not-watching':
  DCHECK(!client_receiver_.is_bound());

  // 'not-watching' 2.1. Set this.[[watchAdvertisementsState]] to
  // 'pending-watch'.
  watch_advertisements_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state, exception_state.GetContext());
  mojo::PendingAssociatedRemote<mojom::blink::WebBluetoothAdvertisementClient>
      client;
  client_receiver_.Bind(client.InitWithNewEndpointAndPassReceiver(),
                        context->GetTaskRunner(TaskType::kMiscPlatformAPI));

  // 'not-watching' 2.2.1. Ensure that the UA is scanning for this device’s
  // advertisements. The UA SHOULD NOT filter out "duplicate" advertisements for
  // the same device.
  bluetooth_->Service()->WatchAdvertisementsForDevice(
      device_->id, std::move(client),
      WTF::BindOnce(&BluetoothDevice::WatchAdvertisementsCallback,
                    WrapPersistent(this)));
  return watch_advertisements_resolver_->Promise();
}

// https://webbluetoothcg.github.io/web-bluetooth/#abort-watchadvertisements
void BluetoothDevice::AbortWatchAdvertisements(AbortSignal* signal) {
  // 1. Set this.[[watchAdvertisementsState]] to 'not-watching'.
  // 2. Set device.watchingAdvertisements to false.
  // 3.1. If no more BluetoothDevices in the whole UA have
  // watchingAdvertisements set to true, the UA SHOULD stop scanning for
  // advertisements. Otherwise, if no more BluetoothDevices representing the
  // same device as this have watchingAdvertisements set to true, the UA SHOULD
  // reconfigure the scan to avoid receiving reports for this device.
  client_receiver_.reset();

  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothdevice-watchadvertisements
  // 1.2.2. Reject promise with AbortError
  if (watch_advertisements_resolver_) {
    auto* script_state = watch_advertisements_resolver_->GetScriptState();
    watch_advertisements_resolver_->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kAbortError,
        kAbortErrorMessage));
    watch_advertisements_resolver_.Clear();
  }

  DCHECK(signal);
  abort_handle_map_.erase(signal);
}

ScriptPromise<IDLUndefined> BluetoothDevice::forget(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!GetExecutionContext()) {
    exception_state.ThrowTypeError(kInactiveDocumentError);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  bluetooth_->Service()->ForgetDevice(
      device_->id, WTF::BindOnce(
                       [](ScriptPromiseResolver<IDLUndefined>* resolver) {
                         resolver->Resolve();
                       },
                       WrapPersistent(resolver)));

  return promise;
}

void BluetoothDevice::AdvertisingEvent(
    mojom::blink::WebBluetoothAdvertisingEventPtr advertising_event) {
  auto* event = MakeGarbageCollected<BluetoothAdvertisingEvent>(
      event_type_names::kAdvertisementreceived, this,
      std::move(advertising_event));
  DispatchEvent(*event);
}

bool BluetoothDevice::HasPendingActivity() const {
  return GetExecutionContext() && HasEventListeners();
}

void BluetoothDevice::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  if (event_type == event_type_names::kGattserverdisconnected) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kGATTServerDisconnectedEvent);
  }
}

void BluetoothDevice::WatchAdvertisementsCallback(
    mojom::blink::WebBluetoothResult result) {
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothdevice-watchadvertisements
  // 2.2.3. Queue a task to perform the following steps, but abort when
  // this.[[watchAdvertisementsState]] becomes not-watching:
  if (!watch_advertisements_resolver_)
    return;

  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothdevice-watchadvertisements
  // 2.2.2. If the UA fails to enable scanning, queue a task to perform the
  // following steps, and abort these steps:
  if (result != mojom::blink::WebBluetoothResult::SUCCESS) {
    // 2.2.2.1. Set this.[[watchAdvertisementsState]] to 'not-watching'.
    client_receiver_.reset();

    // 2.2.2.2. Reject promise with one of the following errors:
    watch_advertisements_resolver_->Reject(
        BluetoothError::CreateDOMException(result));
    watch_advertisements_resolver_.Clear();
    return;
  }

  // 2.2.3.3. Resolve promise with undefined.
  watch_advertisements_resolver_->Resolve();
  watch_advertisements_resolver_.Clear();
}

}  // namespace blink
