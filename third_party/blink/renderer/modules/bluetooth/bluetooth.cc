// Copyright 2015 The Chromium Authors
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
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_unsignedlong.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_advertising_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_data_filter_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_le_scan_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_manufacturer_data_filter_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_request_device_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_le_scan.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_manufacturer_data_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_service_data_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_uuid.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

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
const char kFencedFrameError[] =
    "Web Bluetooth is not allowed in a fenced frame tree.";
const char kPermissionsPolicyBlocked[] =
    "Access to the feature \"bluetooth\" is disallowed by permissions policy.";

// Does basic checks that are common to all IDL calls, mainly that the window is
// valid, and the request is not being done from a fenced frame tree. Returns
// true if exceptions have been flagged, and false otherwise.
bool IsRequestDenied(LocalDOMWindow* window, ExceptionState& exception_state) {
  if (!window) {
    exception_state.ThrowTypeError(kInactiveDocumentError);
  } else if (window->GetFrame()->IsInFencedFrameTree()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kFencedFrameError);
  } else if (window->GetFrame()
                 ->Top()
                 ->GetSecurityContext()
                 ->GetSecurityOrigin()
                 ->IsOpaque()) {
    exception_state.ThrowSecurityError(
        "Access to the Web Bluetooth API is denied from contexts where the "
        "top-level document has an opaque origin.");
  }

  return exception_state.HadException();
}

// Checks whether the document is allowed by Permissions Policy to call Web
// Bluetooth API methods.
bool IsFeatureEnabled(LocalDOMWindow* window) {
  return window->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kBluetooth,
      ReportOptions::kReportOnFailure);
}

// Remind developers when they are using Web Bluetooth on unsupported platforms.
// TODO(https://crbug.com/570344): Remove this method when all platforms are
// supported.
void AddUnsupportedPlatformConsoleMessage(ExecutionContext* context) {
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID) && \
    !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kInfo,
      "Web Bluetooth is experimental on this platform. See "
      "https://github.com/WebBluetoothCG/web-bluetooth/blob/main/"
      "implementation-status.md"));
#endif
}

void CanonicalizeFilter(
    const BluetoothLEScanFilterInit* filter,
    mojom::blink::WebBluetoothLeScanFilterPtr& canonicalized_filter,
    ExceptionState& exception_state) {
  if (!(filter->hasServices() || filter->hasName() || filter->hasNamePrefix() ||
        filter->hasManufacturerData())) {
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
    for (const V8UnionStringOrUnsignedLong* service : filter->services()) {
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
          "'namePrefix', if present, must be non-empty.");
      return;
    }
    canonicalized_filter->name_prefix = filter->namePrefix();
  }

  if (filter->hasManufacturerData()) {
    if (filter->manufacturerData().size() == 0) {
      exception_state.ThrowTypeError(
          "'manufacturerData', if present, must be non-empty.");
      return;
    }
    canonicalized_filter->manufacturer_data.emplace();
    for (const auto& manufacturer_data : filter->manufacturerData()) {
      DOMArrayPiece mask_buffer = manufacturer_data->hasMask()
                                      ? DOMArrayPiece(manufacturer_data->mask())
                                      : DOMArrayPiece();
      DOMArrayPiece data_prefix_buffer =
          manufacturer_data->hasDataPrefix()
              ? DOMArrayPiece(manufacturer_data->dataPrefix())
              : DOMArrayPiece();

      base::span<const uint8_t> mask_bytes;
      if (manufacturer_data->hasMask()) {
        if (mask_buffer.IsDetached()) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kInvalidStateError,
              "'mask' value buffer has been detached.");
          return;
        }

        if (!manufacturer_data->hasDataPrefix()) {
          exception_state.ThrowTypeError(
              "'dataPrefix' must be non-empty when 'mask' is present.");
          return;
        }

        if (data_prefix_buffer.ByteLength() != mask_buffer.ByteLength()) {
          exception_state.ThrowTypeError(
              "'mask' size must be equal to 'dataPrefix' size.");
          return;
        }
        mask_bytes = mask_buffer.ByteSpan();
      }

      Vector<mojom::blink::WebBluetoothDataFilterPtr> data_filters_vector;
      if (manufacturer_data->hasDataPrefix()) {
        if (data_prefix_buffer.IsDetached()) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kInvalidStateError,
              "'dataPrefix' value buffer has been detached.");
          return;
        }

        if (data_prefix_buffer.ByteLength() == 0) {
          exception_state.ThrowTypeError(
              "'dataPrefix', if present, must be non-empty.");
          return;
        }

        // Iterate by index here since we're iterating through two arrays.
        auto prefix_bytes = data_prefix_buffer.ByteSpan();
        for (size_t i = 0; i < prefix_bytes.size(); ++i) {
          const uint8_t data = prefix_bytes[i];
          const uint8_t mask = mask_bytes.empty() ? 0xff : mask_bytes[i];
          data_filters_vector.push_back(
              mojom::blink::WebBluetoothDataFilter::New(data, mask));
        }
      }

      auto company = mojom::blink::WebBluetoothCompany::New();
      company->id = manufacturer_data->companyIdentifier();
      auto result = canonicalized_filter->manufacturer_data->insert(
          std::move(company), std::move(data_filters_vector));
      if (!result.is_new_entry) {
        exception_state.ThrowTypeError("'companyIdentifier' must be unique.");
        return;
      }
    }
  }
}

void ConvertRequestDeviceOptions(
    const RequestDeviceOptions* options,
    mojom::blink::WebBluetoothRequestDeviceOptionsPtr& result,
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  if (options->hasExclusionFilters() && !options->hasFilters()) {
    exception_state.ThrowTypeError(
        "'filters' member must be present if 'exclusionFilters' is present.");
    return;
  }

  if (!(options->hasFilters() ^ options->acceptAllDevices())) {
    exception_state.ThrowTypeError(
        "Either 'filters' should be present or 'acceptAllDevices' should be "
        "true, but not both.");
    return;
  }

  result->accept_all_devices = options->acceptAllDevices();

  if (options->hasFilters()) {
    if (options->filters().empty()) {
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

      if (canonicalized_filter->manufacturer_data) {
        UseCounter::Count(execution_context,
                          WebFeature::kWebBluetoothManufacturerDataFilter);
      }

      result->filters->push_back(std::move(canonicalized_filter));
    }
  }

  if (options->hasExclusionFilters()) {
    if (options->exclusionFilters().empty()) {
      exception_state.ThrowTypeError(
          "'exclusionFilters' member must be non-empty to exclude any device.");
      return;
    }

    result->exclusion_filters.emplace();

    for (const BluetoothLEScanFilterInit* filter :
         options->exclusionFilters()) {
      auto canonicalized_filter = mojom::blink::WebBluetoothLeScanFilter::New();

      CanonicalizeFilter(filter, canonicalized_filter, exception_state);
      if (exception_state.HadException()) {
        return;
      }

      result->exclusion_filters->push_back(std::move(canonicalized_filter));
    }
  }

  if (options->hasOptionalServices()) {
    for (const V8UnionStringOrUnsignedLong* optional_service :
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

}  // namespace

ScriptPromise<IDLBoolean> Bluetooth::getAvailability(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();

  if (IsRequestDenied(window, exception_state)) {
    return EmptyPromise();
  }

  // If Bluetooth is disallowed by Permissions Policy, getAvailability should
  // return false.
  if (!IsFeatureEnabled(window)) {
    return ToResolvedPromise<IDLBoolean>(script_state, false);
  }

  CHECK(window->IsSecureContext());
  EnsureServiceConnection(window);

  // Subsequent steps are handled in the browser process.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  service_->GetAvailability(
      WTF::BindOnce([](ScriptPromiseResolver<IDLBoolean>* resolver,
                       bool result) { resolver->Resolve(result); },
                    WrapPersistent(resolver)));
  return promise;
}

void Bluetooth::GetDevicesCallback(
    ScriptPromiseResolver<IDLSequence<BluetoothDevice>>* resolver,
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
    ScriptPromiseResolver<BluetoothDevice>* resolver,
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

ScriptPromise<IDLSequence<BluetoothDevice>> Bluetooth::getDevices(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();

  if (IsRequestDenied(window, exception_state)) {
    return ScriptPromise<IDLSequence<BluetoothDevice>>();
  }

  if (!IsFeatureEnabled(window)) {
    exception_state.ThrowSecurityError(kPermissionsPolicyBlocked);
    return ScriptPromise<IDLSequence<BluetoothDevice>>();
  }

  AddUnsupportedPlatformConsoleMessage(window);
  CHECK(window->IsSecureContext());

  EnsureServiceConnection(window);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<BluetoothDevice>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  service_->GetDevices(WTF::BindOnce(&Bluetooth::GetDevicesCallback,
                                     WrapPersistent(this),
                                     WrapPersistent(resolver)));
  return promise;
}

// https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetooth-requestdevice
ScriptPromise<BluetoothDevice> Bluetooth::requestDevice(
    ScriptState* script_state,
    const RequestDeviceOptions* options,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();

  if (IsRequestDenied(window, exception_state)) {
    return EmptyPromise();
  }

  if (!IsFeatureEnabled(window)) {
    exception_state.ThrowSecurityError(kPermissionsPolicyBlocked);
    return EmptyPromise();
  }

  AddUnsupportedPlatformConsoleMessage(window);
  CHECK(window->IsSecureContext());

  // If the algorithm is not allowed to show a popup, reject promise with a
  // SecurityError and abort these steps.
  auto* frame = window->GetFrame();
  DCHECK(frame);
  if (!LocalFrame::HasTransientUserActivation(frame)) {
    exception_state.ThrowSecurityError(kHandleGestureForPermissionRequest);
    return EmptyPromise();
  }

  EnsureServiceConnection(window);

  // In order to convert the arguments from service names and aliases to just
  // UUIDs, do the following substeps:
  auto device_options = mojom::blink::WebBluetoothRequestDeviceOptions::New();
  ConvertRequestDeviceOptions(options, device_options, GetExecutionContext(),
                              exception_state);

  if (exception_state.HadException())
    return EmptyPromise();

  // Subsequent steps are handled in the browser process.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<BluetoothDevice>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  service_->RequestDevice(
      std::move(device_options),
      WTF::BindOnce(&Bluetooth::RequestDeviceCallback, WrapPersistent(this),
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
    if (options->filters().empty()) {
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
    ScriptPromiseResolver<BluetoothLEScan>* resolver,
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
ScriptPromise<BluetoothLEScan> Bluetooth::requestLEScan(
    ScriptState* script_state,
    const BluetoothLEScanOptions* options,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();

  if (IsRequestDenied(window, exception_state)) {
    return EmptyPromise();
  }

  if (!IsFeatureEnabled(window)) {
    exception_state.ThrowSecurityError(kPermissionsPolicyBlocked);
    return EmptyPromise();
  }

  // Remind developers when they are using Web Bluetooth on unsupported
  // platforms.
  window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kInfo,
      "Web Bluetooth Scanning is experimental on this platform. See "
      "https://github.com/WebBluetoothCG/web-bluetooth/blob/main/"
      "implementation-status.md"));

  CHECK(window->IsSecureContext());

  // If the algorithm is not allowed to show a popup, reject promise with a
  // SecurityError and abort these steps.
  auto* frame = window->GetFrame();
  // If Navigator::DomWindow() returned a non-null |window|, GetFrame() should
  // be valid.
  DCHECK(frame);
  if (!LocalFrame::HasTransientUserActivation(frame)) {
    exception_state.ThrowSecurityError(kHandleGestureForPermissionRequest);
    return EmptyPromise();
  }

  EnsureServiceConnection(window);

  auto scan_options = mojom::blink::WebBluetoothRequestLEScanOptions::New();
  ConvertRequestLEScanOptions(options, scan_options, exception_state);

  if (exception_state.HadException())
    return EmptyPromise();

  // Subsequent steps are handled in the browser process.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<BluetoothLEScan>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  mojo::PendingAssociatedRemote<mojom::blink::WebBluetoothAdvertisementClient>
      client;
  // See https://bit.ly/2S0zRAS for task types.
  mojo::ReceiverId id =
      client_receivers_.Add(client.InitWithNewEndpointAndPassReceiver(),
                            window->GetTaskRunner(TaskType::kMiscPlatformAPI));

  auto scan_options_copy = scan_options->Clone();
  service_->RequestScanningStart(
      std::move(client), std::move(scan_options),
      WTF::BindOnce(&Bluetooth::RequestScanningCallback, WrapPersistent(this),
                    WrapPersistent(resolver), id,
                    std::move(scan_options_copy)));

  return promise;
}

void Bluetooth::AdvertisingEvent(
    mojom::blink::WebBluetoothAdvertisingEventPtr advertising_event) {
  auto* event = MakeGarbageCollected<BluetoothAdvertisingEvent>(
      event_type_names::kAdvertisementreceived,
      GetBluetoothDeviceRepresentingDevice(std::move(advertising_event->device),
                                           GetExecutionContext()),
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
  return GetSupplementable()->DomWindow();
}

void Bluetooth::Trace(Visitor* visitor) const {
  visitor->Trace(device_instance_map_);
  visitor->Trace(client_receivers_);
  visitor->Trace(service_);
  EventTarget::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

// static
const char Bluetooth::kSupplementName[] = "Bluetooth";

Bluetooth* Bluetooth::bluetooth(Navigator& navigator) {
  if (!navigator.DomWindow())
    return nullptr;

  Bluetooth* supplement = Supplement<Navigator>::From<Bluetooth>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<Bluetooth>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

Bluetooth::Bluetooth(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      PageVisibilityObserver(navigator.DomWindow()->GetFrame()->GetPage()),
      client_receivers_(this, navigator.DomWindow()),
      service_(navigator.DomWindow()) {}

Bluetooth::~Bluetooth() = default;

BluetoothDevice* Bluetooth::GetBluetoothDeviceRepresentingDevice(
    mojom::blink::WebBluetoothDevicePtr device_ptr,
    ExecutionContext* context) {
  // TODO(crbug.com/1275634): convert device_instance_map_ to use
  // WebBluetoothDeviceId as key
  auto it =
      device_instance_map_.find(device_ptr->id.DeviceIdInBase64().c_str());
  if (it != device_instance_map_.end()) {
    return it->value.Get();
  }

  BluetoothDevice* device = MakeGarbageCollected<BluetoothDevice>(
      context, std::move(device_ptr), this);
  auto result = device_instance_map_.insert(
      device->GetDevice()->id.DeviceIdInBase64().c_str(), device);
  DCHECK(result.is_new_entry);
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
