// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"

#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_advertising_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_le_scan_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_request_device_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_le_scan.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_manufacturer_data_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_service_data_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_uuid.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
// Per the Bluetooth Spec: The name is a user-friendly name associated with the
// device and consists of a maximum of 248 bytes coded according to the UTF-8
// standard.
const size_t kMaxDeviceNameLength = 248;
const char kDeviceNameTooLong[] =
    "A device name can't be longer than 248 bytes.";
const char kInactiveDocumentError[] = "Document not active";
const char kHandleGestureForPermissionRequest[] =
    "Must be handling a user gesture to show a permission request.";
}  // namespace

// Remind developers when they are using Web Bluetooth on unsupported platforms.
// TODO(https://crbug.com/570344): Remove this method when all platforms are
// supported.
void AddUnsupportedPlatformConsoleMessage(ExecutionContext* context) {
#if !BUILDFLAG(IS_ASH) && !defined(OS_ANDROID) && !defined(OS_MAC) && \
    !defined(OS_WIN)
  context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kInfo,
      "Web Bluetooth is experimental on this platform. See "
      "https://github.com/WebBluetoothCG/web-bluetooth/blob/gh-pages/"
      "implementation-status.md"));
#endif
}

static void CanonicalizeFilter(
    const BluetoothLEScanFilterInit* filter,
    mojom::blink::WebBluetoothLeScanFilterPtr& canonicalized_filter,
    ExceptionState& exception_state) {
  if (!(filter->hasServices() || filter->hasName() ||
        filter->hasNamePrefix())) {
    exception_state.ThrowTypeError(
        "A filter must restrict the devices in some way.");
    return;
  }

  if (filter->hasServices()) {
    if (filter->services().size() == 0) {
      exception_state.ThrowTypeError(
          "'services', if present, must contain at least one service.");
      return;
    }
    canonicalized_filter->services.emplace();
    for (const StringOrUnsignedLong& service : filter->services()) {
      const String& validated_service =
          BluetoothUUID::getService(service, exception_state);
      if (exception_state.HadException())
        return;
      canonicalized_filter->services->push_back(validated_service);
    }
  }

  if (filter->hasName()) {
    size_t name_length = filter->name().Utf8().length();
    if (name_length > kMaxDeviceNameLength) {
      exception_state.ThrowTypeError(kDeviceNameTooLong);
      return;
    }
    canonicalized_filter->name = filter->name();
  }

  if (filter->hasNamePrefix()) {
    size_t name_prefix_length = filter->namePrefix().Utf8().length();
    if (name_prefix_length > kMaxDeviceNameLength) {
      exception_state.ThrowTypeError(kDeviceNameTooLong);
      return;
    }
    if (filter->namePrefix().length() == 0) {
      exception_state.ThrowTypeError(
          "'namePrefix', if present, must me non-empty.");
      return;
    }
    canonicalized_filter->name_prefix = filter->namePrefix();
  }
}

static void ConvertRequestDeviceOptions(
    const RequestDeviceOptions* options,
    mojom::blink::WebBluetoothRequestDeviceOptionsPtr& result,
    ExceptionState& exception_state) {
  if (!(options->hasFilters() ^ options->acceptAllDevices())) {
    exception_state.ThrowTypeError(
        "Either 'filters' should be present or 'acceptAllDevices' should be "
        "true, but not both.");
    return;
  }

  result->accept_all_devices = options->acceptAllDevices();

  if (options->hasFilters()) {
    if (options->filters().IsEmpty()) {
      exception_state.ThrowTypeError(
          "'filters' member must be non-empty to find any devices.");
      return;
    }

    result->filters.emplace();

    for (const BluetoothLEScanFilterInit* filter : options->filters()) {
      auto canonicalized_filter = mojom::blink::WebBluetoothLeScanFilter::New();

      CanonicalizeFilter(filter, canonicalized_filter, exception_state);

      if (exception_state.HadException())
        return;

      result->filters->push_back(std::move(canonicalized_filter));
    }
  }

  if (options->hasOptionalServices()) {
    for (const StringOrUnsignedLong& optional_service :
         options->optionalServices()) {
      const String& validated_optional_service =
          BluetoothUUID::getService(optional_service, exception_state);
      if (exception_state.HadException())
        return;
      result->optional_services.push_back(validated_optional_service);
    }
  }

  if (options->hasOptionalManufacturerData()) {
    for (const uint16_t manufacturer_code :
         options->optionalManufacturerData()) {
      result->optional_manufacturer_data.push_back(manufacturer_code);
    }
  }
}

ScriptPromise Bluetooth::getAvailability(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  if (window_->IsContextDestroyed()) {
    exception_state.ThrowTypeError(kInactiveDocumentError);
    return ScriptPromise();
  }

  CHECK(window_->IsSecureContext());
  EnsureServiceConnection(window_);

  // Subsequent steps are handled in the browser process.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  service_->GetAvailability(
      WTF::Bind([](ScriptPromiseResolver* resolver,
                   bool result) { resolver->Resolve(result); },
                WrapPersistent(resolver)));
  return promise;
}

void Bluetooth::GetDevicesCallback(
    ScriptPromiseResolver* resolver,
    Vector<mojom::blink::WebBluetoothDevicePtr> devices) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  HeapVector<Member<BluetoothDevice>> bluetooth_devices;
  for (auto& device : devices) {
    BluetoothDevice* bluetooth_device = GetBluetoothDeviceRepresentingDevice(
        std::move(device), resolver->GetExecutionContext());
    bluetooth_devices.push_back(*bluetooth_device);
  }
  resolver->Resolve(bluetooth_devices);
}

void Bluetooth::RequestDeviceCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    mojom::blink::WebBluetoothDevicePtr device) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    BluetoothDevice* bluetooth_device = GetBluetoothDeviceRepresentingDevice(
        std::move(device), resolver->GetExecutionContext());
    resolver->Resolve(bluetooth_device);
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise Bluetooth::getDevices(ScriptState* script_state,
                                    ExceptionState& exception_state) {
  if (window_->IsContextDestroyed()) {
    exception_state.ThrowTypeError(kInactiveDocumentError);
    return ScriptPromise();
  }

  AddUnsupportedPlatformConsoleMessage(window_);
  CHECK(window_->IsSecureContext());

  EnsureServiceConnection(window_);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  service_->GetDevices(WTF::Bind(&Bluetooth::GetDevicesCallback,
                                 WrapPersistent(this),
                                 WrapPersistent(resolver)));
  return promise;
}

// https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetooth-requestdevice
ScriptPromise Bluetooth::requestDevice(ScriptState* script_state,
                                       const RequestDeviceOptions* options,
                                       ExceptionState& exception_state) {
  if (window_->IsContextDestroyed()) {
    exception_state.ThrowTypeError(kInactiveDocumentError);
    return ScriptPromise();
  }

  AddUnsupportedPlatformConsoleMessage(window_);
  CHECK(window_->IsSecureContext());

  // If the algorithm is not allowed to show a popup, reject promise with a
  // SecurityError and abort these steps.
  auto* frame = window_->GetFrame();
  DCHECK(frame);
  if (!LocalFrame::HasTransientUserActivation(frame)) {
    exception_state.ThrowSecurityError(kHandleGestureForPermissionRequest);
    return ScriptPromise();
  }

  EnsureServiceConnection(window_);

  // In order to convert the arguments from service names and aliases to just
  // UUIDs, do the following substeps:
  auto device_options = mojom::blink::WebBluetoothRequestDeviceOptions::New();
  ConvertRequestDeviceOptions(options, device_options, exception_state);

  if (exception_state.HadException())
    return ScriptPromise();

  // Subsequent steps are handled in the browser process.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  service_->RequestDevice(
      std::move(device_options),
      WTF::Bind(&Bluetooth::RequestDeviceCallback, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

static void ConvertRequestLEScanOptions(
    const BluetoothLEScanOptions* options,
    mojom::blink::WebBluetoothRequestLEScanOptionsPtr& result,
    ExceptionState& exception_state) {
  if (!(options->hasFilters() ^ options->acceptAllAdvertisements())) {
    exception_state.ThrowTypeError(
        "Either 'filters' should be present or 'acceptAllAdvertisements' "
        "should be true, but not both.");
    return;
  }

  result->accept_all_advertisements = options->acceptAllAdvertisements();
  result->keep_repeated_devices = options->keepRepeatedDevices();

  if (options->hasFilters()) {
    if (options->filters().IsEmpty()) {
      exception_state.ThrowTypeError(
          "'filters' member must be non-empty to find any devices.");
      return;
    }

    result->filters.emplace();

    for (const BluetoothLEScanFilterInit* filter : options->filters()) {
      auto canonicalized_filter = mojom::blink::WebBluetoothLeScanFilter::New();

      CanonicalizeFilter(filter, canonicalized_filter, exception_state);

      if (exception_state.HadException())
        return;

      result->filters->push_back(std::move(canonicalized_filter));
    }
  }
}

void Bluetooth::RequestScanningCallback(
    ScriptPromiseResolver* resolver,
    mojo::ReceiverId id,
    mojom::blink::WebBluetoothRequestLEScanOptionsPtr options,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (result != mojom::blink::WebBluetoothResult::SUCCESS) {
    resolver->Reject(BluetoothError::CreateDOMException(result));
    return;
  }

  auto* scan =
      MakeGarbageCollected<BluetoothLEScan>(id, this, std::move(options));
  resolver->Resolve(scan);
}

// https://webbluetoothcg.github.io/web-bluetooth/scanning.html#dom-bluetooth-requestlescan
ScriptPromise Bluetooth::requestLEScan(ScriptState* script_state,
                                       const BluetoothLEScanOptions* options,
                                       ExceptionState& exception_state) {
  if (window_->IsContextDestroyed()) {
    exception_state.ThrowTypeError(kInactiveDocumentError);
    return ScriptPromise();
  }

  // Remind developers when they are using Web Bluetooth on unsupported
  // platforms.
  window_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kInfo,
      "Web Bluetooth Scanning is experimental on this platform. See "
      "https://github.com/WebBluetoothCG/web-bluetooth/blob/gh-pages/"
      "implementation-status.md"));

  CHECK(window_->IsSecureContext());

  // If the algorithm is not allowed to show a popup, reject promise with a
  // SecurityError and abort these steps.
  auto* frame = window_->GetFrame();
  // If !window_->IsContextDestroyed() then GetFrame() should be valid.
  DCHECK(frame);
  if (!LocalFrame::HasTransientUserActivation(frame)) {
    exception_state.ThrowSecurityError(kHandleGestureForPermissionRequest);
    return ScriptPromise();
  }

  EnsureServiceConnection(window_);

  auto scan_options = mojom::blink::WebBluetoothRequestLEScanOptions::New();
  ConvertRequestLEScanOptions(options, scan_options, exception_state);

  if (exception_state.HadException())
    return ScriptPromise();

  // Subsequent steps are handled in the browser process.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  mojo::PendingAssociatedRemote<mojom::blink::WebBluetoothAdvertisementClient>
      client;
  // See https://bit.ly/2S0zRAS for task types.
  mojo::ReceiverId id =
      client_receivers_.Add(client.InitWithNewEndpointAndPassReceiver(),
                            window_->GetTaskRunner(TaskType::kMiscPlatformAPI));

  auto scan_options_copy = scan_options->Clone();
  service_->RequestScanningStart(
      std::move(client), std::move(scan_options),
      WTF::Bind(&Bluetooth::RequestScanningCallback, WrapPersistent(this),
                WrapPersistent(resolver), id, std::move(scan_options_copy)));

  return promise;
}

void Bluetooth::AdvertisingEvent(
    mojom::blink::WebBluetoothAdvertisingEventPtr advertising_event) {
  auto* event = MakeGarbageCollected<BluetoothAdvertisingEvent>(
      event_type_names::kAdvertisementreceived,
      GetBluetoothDeviceRepresentingDevice(std::move(advertising_event->device),
                                           window_),
      std::move(advertising_event));
  DispatchEvent(*event);
}

void Bluetooth::PageVisibilityChanged() {
  client_receivers_.Clear();
}

void Bluetooth::CancelScan(mojo::ReceiverId id) {
  client_receivers_.Remove(id);
}

bool Bluetooth::IsScanActive(mojo::ReceiverId id) const {
  return client_receivers_.HasReceiver(id);
}

const WTF::AtomicString& Bluetooth::InterfaceName() const {
  return event_type_names::kAdvertisementreceived;
}

ExecutionContext* Bluetooth::GetExecutionContext() const {
  return window_;
}

void Bluetooth::Trace(Visitor* visitor) const {
  visitor->Trace(device_instance_map_);
  visitor->Trace(window_);
  visitor->Trace(client_receivers_);
  visitor->Trace(service_);
  EventTargetWithInlineData::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

Bluetooth::Bluetooth(LocalDOMWindow* dom_window)
    : PageVisibilityObserver(dom_window->GetFrame()->GetPage()),
      window_(dom_window),
      client_receivers_(this, dom_window),
      service_(dom_window) {}

Bluetooth::~Bluetooth() {
  DCHECK(client_receivers_.empty());
}

BluetoothDevice* Bluetooth::GetBluetoothDeviceRepresentingDevice(
    mojom::blink::WebBluetoothDevicePtr device_ptr,
    ExecutionContext* context) {
  String& id = device_ptr->id;
  BluetoothDevice* device = device_instance_map_.at(id);
  if (!device) {
    device = MakeGarbageCollected<BluetoothDevice>(context,
                                                   std::move(device_ptr), this);
    auto result = device_instance_map_.insert(id, device);
    DCHECK(result.is_new_entry);
  }
  return device;
}

void Bluetooth::EnsureServiceConnection(ExecutionContext* context) {
  if (!service_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types.
    auto task_runner = context->GetTaskRunner(TaskType::kMiscPlatformAPI);
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(task_runner));
  }
}

}  // namespace blink
