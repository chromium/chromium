// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_device.h"

#include <algorithm>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/webusb/usb_configuration.h"
#include "third_party/blink/renderer/modules/webusb/usb_control_transfer_parameters.h"
#include "third_party/blink/renderer/modules/webusb/usb_in_transfer_result.h"
#include "third_party/blink/renderer/modules/webusb/usb_isochronous_in_transfer_result.h"
#include "third_party/blink/renderer/modules/webusb/usb_isochronous_out_transfer_result.h"
#include "third_party/blink/renderer/modules/webusb/usb_out_transfer_result.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using device::mojom::blink::UsbControlTransferParamsPtr;
using device::mojom::blink::UsbControlTransferType;
using device::mojom::blink::UsbControlTransferRecipient;
using device::mojom::blink::UsbDeviceInfoPtr;
using device::mojom::blink::UsbDevicePtr;
using device::mojom::blink::UsbIsochronousPacketPtr;
using device::mojom::blink::UsbOpenDeviceError;
using device::mojom::blink::UsbTransferDirection;
using device::mojom::blink::UsbTransferStatus;

namespace blink {

namespace {

const char kDeviceStateChangeInProgress[] =
    "An operation that changes the device state is in progress.";
const char kDeviceDisconnected[] = "The device was disconnected.";
const char kInterfaceNotFound[] =
    "The interface number provided is not supported by the device in its "
    "current configuration.";
const char kInterfaceStateChangeInProgress[] =
    "An operation that changes interface state is in progress.";
const char kOpenRequired[] = "The device must be opened first.";

DOMException* ConvertFatalTransferStatus(const UsbTransferStatus& status) {
  switch (status) {
    case UsbTransferStatus::TRANSFER_ERROR:
      return DOMException::Create(DOMExceptionCode::kNetworkError,
                                  "A transfer error has occurred.");
    case UsbTransferStatus::PERMISSION_DENIED:
      return DOMException::Create(DOMExceptionCode::kSecurityError,
                                  "The transfer was not allowed.");
    case UsbTransferStatus::TIMEOUT:
      return DOMException::Create(DOMExceptionCode::kTimeoutError,
                                  "The transfer timed out.");
    case UsbTransferStatus::CANCELLED:
      return DOMException::Create(DOMExceptionCode::kAbortError,
                                  "The transfer was cancelled.");
    case UsbTransferStatus::DISCONNECT:
      return DOMException::Create(DOMExceptionCode::kNotFoundError,
                                  kDeviceDisconnected);
    case UsbTransferStatus::COMPLETED:
    case UsbTransferStatus::STALLED:
    case UsbTransferStatus::BABBLE:
    case UsbTransferStatus::SHORT_PACKET:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}

String ConvertTransferStatus(const UsbTransferStatus& status) {
  switch (status) {
    case UsbTransferStatus::COMPLETED:
    case UsbTransferStatus::SHORT_PACKET:
      return "ok";
    case UsbTransferStatus::STALLED:
      return "stall";
    case UsbTransferStatus::BABBLE:
      return "babble";
    default:
      NOTREACHED();
      return "";
  }
}

Vector<uint8_t> ConvertBufferSource(
    const ArrayBufferOrArrayBufferView& buffer) {
  DCHECK(!buffer.IsNull());
  Vector<uint8_t> vector;
  if (buffer.IsArrayBuffer()) {
    vector.Append(static_cast<uint8_t*>(buffer.GetAsArrayBuffer()->Data()),
                  buffer.GetAsArrayBuffer()->ByteLength());
  } else {
    vector.Append(static_cast<uint8_t*>(
                      buffer.GetAsArrayBufferView().View()->BaseAddress()),
                  buffer.GetAsArrayBufferView().View()->byteLength());
  }
  return vector;
}

}  // namespace

USBDevice::USBDevice(UsbDeviceInfoPtr device_info,
                     UsbDevicePtr device,
                     ExecutionContext* context)
    : ContextLifecycleObserver(context),
      device_info_(std::move(device_info)),
      device_(std::move(device)),
      opened_(false),
      device_state_change_in_progress_(false),
      configuration_index_(kNotFound) {
  if (device_) {
    device_.set_connection_error_handler(
        WTF::Bind(&USBDevice::OnConnectionError, WrapWeakPersistent(this)));
  }
  wtf_size_t configuration_index =
      FindConfigurationIndex(Info().active_configuration);
  if (configuration_index != kNotFound)
    OnConfigurationSelected(true /* success */, configuration_index);
}

USBDevice::~USBDevice() {
  // |m_device| may still be valid but there should be no more outstanding
  // requests because each holds a persistent handle to this object.
  DCHECK(device_requests_.IsEmpty());
}

bool USBDevice::IsInterfaceClaimed(wtf_size_t configuration_index,
                                   wtf_size_t interface_index) const {
  return configuration_index_ != kNotFound &&
         configuration_index_ == configuration_index &&
         claimed_interfaces_.Get(interface_index);
}

wtf_size_t USBDevice::SelectedAlternateInterface(
    wtf_size_t interface_index) const {
  return selected_alternates_[interface_index];
}

USBConfiguration* USBDevice::configuration() const {
  if (configuration_index_ != kNotFound)
    return USBConfiguration::Create(this, configuration_index_);
  return nullptr;
}

HeapVector<Member<USBConfiguration>> USBDevice::configurations() const {
  wtf_size_t num_configurations = Info().configurations.size();
  HeapVector<Member<USBConfiguration>> configurations(num_configurations);
  for (wtf_size_t i = 0; i < num_configurations; ++i)
    configurations[i] = USBConfiguration::Create(this, i);
  return configurations;
}

ScriptPromise USBDevice::open(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureNoDeviceOrInterfaceChangeInProgress(resolver)) {
    if (opened_) {
      resolver->Resolve();
    } else {
      device_state_change_in_progress_ = true;
      device_requests_.insert(resolver);
      device_->Open(WTF::Bind(&USBDevice::AsyncOpen, WrapPersistent(this),
                              WrapPersistent(resolver)));
    }
  }
  return promise;
}

ScriptPromise USBDevice::close(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureNoDeviceOrInterfaceChangeInProgress(resolver)) {
    if (!opened_) {
      resolver->Resolve();
    } else {
      device_state_change_in_progress_ = true;
      device_requests_.insert(resolver);
      device_->Close(WTF::Bind(&USBDevice::AsyncClose, WrapPersistent(this),
                               WrapPersistent(resolver)));
    }
  }
  return promise;
}

ScriptPromise USBDevice::selectConfiguration(ScriptState* script_state,
                                             uint8_t configuration_value) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureNoDeviceOrInterfaceChangeInProgress(resolver)) {
    if (!opened_) {
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kInvalidStateError, kOpenRequired));
    } else {
      wtf_size_t configuration_index =
          FindConfigurationIndex(configuration_value);
      if (configuration_index == kNotFound) {
        resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                              "The configuration value "
                                              "provided is not supported by "
                                              "the device."));
      } else if (configuration_index_ == configuration_index) {
        resolver->Resolve();
      } else {
        device_state_change_in_progress_ = true;
        device_requests_.insert(resolver);
        device_->SetConfiguration(
            configuration_value,
            WTF::Bind(&USBDevice::AsyncSelectConfiguration,
                      WrapPersistent(this), configuration_index,
                      WrapPersistent(resolver)));
      }
    }
  }
  return promise;
}

ScriptPromise USBDevice::claimInterface(ScriptState* script_state,
                                        uint8_t interface_number) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureDeviceConfigured(resolver)) {
    wtf_size_t interface_index = FindInterfaceIndex(interface_number);
    if (interface_index == kNotFound) {
      resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                            kInterfaceNotFound));
    } else if (interface_state_change_in_progress_.Get(interface_index)) {
      resolver->Reject(
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kInterfaceStateChangeInProgress));
    } else if (claimed_interfaces_.Get(interface_index)) {
      resolver->Resolve();
    } else if (IsProtectedInterfaceClass(interface_index)) {
      GetExecutionContext()->AddConsoleMessage(
          ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                                 "An attempt to claim a USB device interface "
                                 "has been blocked because it "
                                 "implements a protected interface class."));
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kSecurityError,
          "The requested interface implements a protected class."));
    } else {
      interface_state_change_in_progress_.Set(interface_index);
      device_requests_.insert(resolver);
      device_->ClaimInterface(
          interface_number,
          WTF::Bind(&USBDevice::AsyncClaimInterface, WrapPersistent(this),
                    interface_index, WrapPersistent(resolver)));
    }
  }
  return promise;
}

ScriptPromise USBDevice::releaseInterface(ScriptState* script_state,
                                          uint8_t interface_number) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureDeviceConfigured(resolver)) {
    wtf_size_t interface_index = FindInterfaceIndex(interface_number);
    if (interface_index == kNotFound) {
      resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                            "The interface number provided is "
                                            "not supported by the device in "
                                            "its current configuration."));
    } else if (interface_state_change_in_progress_.Get(interface_index)) {
      resolver->Reject(
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kInterfaceStateChangeInProgress));
    } else if (!claimed_interfaces_.Get(interface_index)) {
      resolver->Resolve();
    } else {
      // Mark this interface's endpoints unavailable while its state is
      // changing.
      SetEndpointsForInterface(interface_index, false);
      interface_state_change_in_progress_.Set(interface_index);
      device_requests_.insert(resolver);
      device_->ReleaseInterface(
          interface_number,
          WTF::Bind(&USBDevice::AsyncReleaseInterface, WrapPersistent(this),
                    interface_index, WrapPersistent(resolver)));
    }
  }
  return promise;
}

ScriptPromise USBDevice::selectAlternateInterface(ScriptState* script_state,
                                                  uint8_t interface_number,
                                                  uint8_t alternate_setting) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureInterfaceClaimed(interface_number, resolver)) {
    // TODO(reillyg): This is duplicated work.
    wtf_size_t interface_index = FindInterfaceIndex(interface_number);
    DCHECK_NE(interface_index, kNotFound);
    wtf_size_t alternate_index =
        FindAlternateIndex(interface_index, alternate_setting);
    if (alternate_index == kNotFound) {
      resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                            "The alternate setting provided is "
                                            "not supported by the device in "
                                            "its current configuration."));
    } else {
      // Mark this old alternate interface's endpoints unavailable while
      // the change is in progress.
      SetEndpointsForInterface(interface_index, false);
      interface_state_change_in_progress_.Set(interface_index);
      device_requests_.insert(resolver);
      device_->SetInterfaceAlternateSetting(
          interface_number, alternate_setting,
          WTF::Bind(&USBDevice::AsyncSelectAlternateInterface,
                    WrapPersistent(this), interface_number, alternate_setting,
                    WrapPersistent(resolver)));
    }
  }
  return promise;
}

ScriptPromise USBDevice::controlTransferIn(
    ScriptState* script_state,
    const USBControlTransferParameters& setup,
    unsigned length) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureDeviceConfigured(resolver)) {
    auto parameters = ConvertControlTransferParameters(setup, resolver);
    if (parameters) {
      device_requests_.insert(resolver);
      device_->ControlTransferIn(
          std::move(parameters), length, 0,
          WTF::Bind(&USBDevice::AsyncControlTransferIn, WrapPersistent(this),
                    WrapPersistent(resolver)));
    }
  }
  return promise;
}

ScriptPromise USBDevice::controlTransferOut(
    ScriptState* script_state,
    const USBControlTransferParameters& setup) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureDeviceConfigured(resolver)) {
    auto parameters = ConvertControlTransferParameters(setup, resolver);
    if (parameters) {
      device_requests_.insert(resolver);
      device_->ControlTransferOut(
          std::move(parameters), Vector<uint8_t>(), 0,
          WTF::Bind(&USBDevice::AsyncControlTransferOut, WrapPersistent(this),
                    0, WrapPersistent(resolver)));
    }
  }
  return promise;
}

ScriptPromise USBDevice::controlTransferOut(
    ScriptState* script_state,
    const USBControlTransferParameters& setup,
    const ArrayBufferOrArrayBufferView& data) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureDeviceConfigured(resolver)) {
    auto parameters = ConvertControlTransferParameters(setup, resolver);
    if (parameters) {
      Vector<uint8_t> buffer = ConvertBufferSource(data);
      unsigned transfer_length = buffer.size();
      device_requests_.insert(resolver);
      device_->ControlTransferOut(
          std::move(parameters), buffer, 0,
          WTF::Bind(&USBDevice::AsyncControlTransferOut, WrapPersistent(this),
                    transfer_length, WrapPersistent(resolver)));
    }
  }
  return promise;
}

ScriptPromise USBDevice::clearHalt(ScriptState* script_state,
                                   String direction,
                                   uint8_t endpoint_number) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureEndpointAvailable(direction == "in", endpoint_number, resolver)) {
    device_requests_.insert(resolver);
    device_->ClearHalt(endpoint_number, WTF::Bind(&USBDevice::AsyncClearHalt,
                                                  WrapPersistent(this),
                                                  WrapPersistent(resolver)));
  }
  return promise;
}

ScriptPromise USBDevice::transferIn(ScriptState* script_state,
                                    uint8_t endpoint_number,
                                    unsigned length) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureEndpointAvailable(true /* in */, endpoint_number, resolver)) {
    device_requests_.insert(resolver);
    device_->GenericTransferIn(
        endpoint_number, length, 0,
        WTF::Bind(&USBDevice::AsyncTransferIn, WrapPersistent(this),
                  WrapPersistent(resolver)));
  }
  return promise;
}

ScriptPromise USBDevice::transferOut(ScriptState* script_state,
                                     uint8_t endpoint_number,
                                     const ArrayBufferOrArrayBufferView& data) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureEndpointAvailable(false /* out */, endpoint_number, resolver)) {
    Vector<uint8_t> buffer = ConvertBufferSource(data);
    unsigned transfer_length = buffer.size();
    device_requests_.insert(resolver);
    device_->GenericTransferOut(
        endpoint_number, buffer, 0,
        WTF::Bind(&USBDevice::AsyncTransferOut, WrapPersistent(this),
                  transfer_length, WrapPersistent(resolver)));
  }
  return promise;
}

ScriptPromise USBDevice::isochronousTransferIn(
    ScriptState* script_state,
    uint8_t endpoint_number,
    Vector<unsigned> packet_lengths) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureEndpointAvailable(true /* in */, endpoint_number, resolver)) {
    device_requests_.insert(resolver);
    device_->IsochronousTransferIn(
        endpoint_number, packet_lengths, 0,
        WTF::Bind(&USBDevice::AsyncIsochronousTransferIn, WrapPersistent(this),
                  WrapPersistent(resolver)));
  }
  return promise;
}

ScriptPromise USBDevice::isochronousTransferOut(
    ScriptState* script_state,
    uint8_t endpoint_number,
    const ArrayBufferOrArrayBufferView& data,
    Vector<unsigned> packet_lengths) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureEndpointAvailable(false /* out */, endpoint_number, resolver)) {
    device_requests_.insert(resolver);
    device_->IsochronousTransferOut(
        endpoint_number, ConvertBufferSource(data), packet_lengths, 0,
        WTF::Bind(&USBDevice::AsyncIsochronousTransferOut, WrapPersistent(this),
                  WrapPersistent(resolver)));
  }
  return promise;
}

ScriptPromise USBDevice::reset(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureNoDeviceOrInterfaceChangeInProgress(resolver)) {
    if (!opened_) {
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kInvalidStateError, kOpenRequired));
    } else {
      device_requests_.insert(resolver);
      device_->Reset(WTF::Bind(&USBDevice::AsyncReset, WrapPersistent(this),
                               WrapPersistent(resolver)));
    }
  }
  return promise;
}

void USBDevice::ContextDestroyed(ExecutionContext*) {
  device_.reset();
  device_requests_.clear();
}

void USBDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_requests_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

wtf_size_t USBDevice::FindConfigurationIndex(
    uint8_t configuration_value) const {
  const auto& configurations = Info().configurations;
  for (wtf_size_t i = 0; i < configurations.size(); ++i) {
    if (configurations[i]->configuration_value == configuration_value)
      return i;
  }
  return kNotFound;
}

wtf_size_t USBDevice::FindInterfaceIndex(uint8_t interface_number) const {
  DCHECK_NE(configuration_index_, kNotFound);
  const auto& interfaces =
      Info().configurations[configuration_index_]->interfaces;
  for (wtf_size_t i = 0; i < interfaces.size(); ++i) {
    if (interfaces[i]->interface_number == interface_number)
      return i;
  }
  return kNotFound;
}

wtf_size_t USBDevice::FindAlternateIndex(uint32_t interface_index,
                                         uint8_t alternate_setting) const {
  DCHECK_NE(configuration_index_, kNotFound);
  const auto& alternates = Info()
                               .configurations[configuration_index_]
                               ->interfaces[interface_index]
                               ->alternates;
  for (wtf_size_t i = 0; i < alternates.size(); ++i) {
    if (alternates[i]->alternate_setting == alternate_setting)
      return i;
  }
  return kNotFound;
}

bool USBDevice::IsProtectedInterfaceClass(wtf_size_t interface_index) const {
  DCHECK_NE(configuration_index_, kNotFound);
  DCHECK_NE(interface_index, kNotFound);

  // USB Class Codes are defined by the USB-IF:
  // http://www.usb.org/developers/defined_class
  const uint8_t kProtectedClasses[] = {
      0x01,  // Audio
      0x03,  // HID
      0x08,  // Mass Storage
      0x0B,  // Smart Card
      0x0E,  // Video
      0x10,  // Audio/Video
      0xE0,  // Wireless Controller (Bluetooth and Wireless USB)
  };
  DCHECK(std::is_sorted(std::begin(kProtectedClasses),
                        std::end(kProtectedClasses)));

  const auto& alternates = Info()
                               .configurations[configuration_index_]
                               ->interfaces[interface_index]
                               ->alternates;
  for (const auto& alternate : alternates) {
    if (std::binary_search(std::begin(kProtectedClasses),
                           std::end(kProtectedClasses),
                           alternate->class_code)) {
      return true;
    }
  }

  return false;
}

bool USBDevice::EnsureNoDeviceOrInterfaceChangeInProgress(
    ScriptPromiseResolver* resolver) const {
  if (!device_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kDeviceDisconnected));
  } else if (device_state_change_in_progress_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          kDeviceStateChangeInProgress));
  } else if (AnyInterfaceChangeInProgress()) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          kInterfaceStateChangeInProgress));
  } else {
    return true;
  }
  return false;
}

bool USBDevice::EnsureDeviceConfigured(ScriptPromiseResolver* resolver) const {
  if (!device_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kDeviceDisconnected));
  } else if (device_state_change_in_progress_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          kDeviceStateChangeInProgress));
  } else if (!opened_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          kOpenRequired));
  } else if (configuration_index_ == kNotFound) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "The device must have a configuration selected."));
  } else {
    return true;
  }
  return false;
}

bool USBDevice::EnsureInterfaceClaimed(uint8_t interface_number,
                                       ScriptPromiseResolver* resolver) const {
  if (!EnsureDeviceConfigured(resolver))
    return false;
  wtf_size_t interface_index = FindInterfaceIndex(interface_number);
  if (interface_index == kNotFound) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kInterfaceNotFound));
  } else if (interface_state_change_in_progress_.Get(interface_index)) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          kInterfaceStateChangeInProgress));
  } else if (!claimed_interfaces_.Get(interface_index)) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "The specified interface has not been claimed."));
  } else {
    return true;
  }
  return false;
}

bool USBDevice::EnsureEndpointAvailable(bool in_transfer,
                                        uint8_t endpoint_number,
                                        ScriptPromiseResolver* resolver) const {
  if (!EnsureDeviceConfigured(resolver))
    return false;
  if (endpoint_number == 0 || endpoint_number >= 16) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kIndexSizeError,
                             "The specified endpoint number is out of range."));
    return false;
  }
  auto& bit_vector = in_transfer ? in_endpoints_ : out_endpoints_;
  if (!bit_vector.Get(endpoint_number - 1)) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          "The specified endpoint is not part "
                                          "of a claimed and selected alternate "
                                          "interface."));
    return false;
  }
  return true;
}

bool USBDevice::AnyInterfaceChangeInProgress() const {
  for (wtf_size_t i = 0; i < interface_state_change_in_progress_.size(); ++i) {
    if (interface_state_change_in_progress_.QuickGet(i))
      return true;
  }
  return false;
}

UsbControlTransferParamsPtr USBDevice::ConvertControlTransferParameters(
    const USBControlTransferParameters& parameters,
    ScriptPromiseResolver* resolver) const {
  auto mojo_parameters = device::mojom::blink::UsbControlTransferParams::New();

  if (parameters.requestType() == "standard") {
    mojo_parameters->type = UsbControlTransferType::STANDARD;
  } else if (parameters.requestType() == "class") {
    mojo_parameters->type = UsbControlTransferType::CLASS;
  } else if (parameters.requestType() == "vendor") {
    mojo_parameters->type = UsbControlTransferType::VENDOR;
  } else {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kTypeMismatchError,
        "The control transfer requestType parameter is invalid."));
    return nullptr;
  }

  if (parameters.recipient() == "device") {
    mojo_parameters->recipient = UsbControlTransferRecipient::DEVICE;
  } else if (parameters.recipient() == "interface") {
    uint8_t interface_number = parameters.index() & 0xff;
    if (!EnsureInterfaceClaimed(interface_number, resolver))
      return nullptr;
    mojo_parameters->recipient = UsbControlTransferRecipient::INTERFACE;
  } else if (parameters.recipient() == "endpoint") {
    bool in_transfer = parameters.index() & 0x80;
    uint8_t endpoint_number = parameters.index() & 0x0f;
    if (!EnsureEndpointAvailable(in_transfer, endpoint_number, resolver))
      return nullptr;
    mojo_parameters->recipient = UsbControlTransferRecipient::ENDPOINT;
  } else if (parameters.recipient() == "other") {
    mojo_parameters->recipient = UsbControlTransferRecipient::OTHER;
  } else {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kTypeMismatchError,
        "The control transfer recipient parameter is invalid."));
    return nullptr;
  }

  mojo_parameters->request = parameters.request();
  mojo_parameters->value = parameters.value();
  mojo_parameters->index = parameters.index();
  return mojo_parameters;
}

void USBDevice::SetEndpointsForInterface(wtf_size_t interface_index, bool set) {
  const auto& configuration = *Info().configurations[configuration_index_];
  const auto& interface = *configuration.interfaces[interface_index];
  const auto& alternate =
      *interface.alternates[selected_alternates_[interface_index]];
  for (const auto& endpoint : alternate.endpoints) {
    uint8_t endpoint_number = endpoint->endpoint_number;
    if (endpoint_number == 0 || endpoint_number >= 16)
      continue;  // Ignore endpoints with invalid indices.
    auto& bit_vector = endpoint->direction == UsbTransferDirection::INBOUND
                           ? in_endpoints_
                           : out_endpoints_;
    if (set)
      bit_vector.Set(endpoint_number - 1);
    else
      bit_vector.Clear(endpoint_number - 1);
  }
}

void USBDevice::AsyncOpen(ScriptPromiseResolver* resolver,
                          UsbOpenDeviceError error) {
  if (!MarkRequestComplete(resolver))
    return;

  switch (error) {
    case UsbOpenDeviceError::ALREADY_OPEN:
      NOTREACHED();
      FALLTHROUGH;
    case UsbOpenDeviceError::OK:
      OnDeviceOpenedOrClosed(true /* opened */);
      resolver->Resolve();
      return;
    case UsbOpenDeviceError::ACCESS_DENIED:
      OnDeviceOpenedOrClosed(false /* not opened */);
      resolver->Reject(DOMException::Create(DOMExceptionCode::kSecurityError,
                                            "Access denied."));
      return;
  }
}

void USBDevice::AsyncClose(ScriptPromiseResolver* resolver) {
  if (!MarkRequestComplete(resolver))
    return;

  OnDeviceOpenedOrClosed(false /* closed */);
  resolver->Resolve();
}

void USBDevice::OnDeviceOpenedOrClosed(bool opened) {
  opened_ = opened;
  if (!opened_) {
    claimed_interfaces_.ClearAll();
    selected_alternates_.Fill(0);
    in_endpoints_.ClearAll();
    out_endpoints_.ClearAll();
  }
  device_state_change_in_progress_ = false;
}

void USBDevice::AsyncSelectConfiguration(wtf_size_t configuration_index,
                                         ScriptPromiseResolver* resolver,
                                         bool success) {
  if (!MarkRequestComplete(resolver))
    return;

  OnConfigurationSelected(success, configuration_index);
  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kNetworkError,
                             "Unable to set device configuration."));
  }
}

void USBDevice::OnConfigurationSelected(bool success,
                                        wtf_size_t configuration_index) {
  if (success) {
    configuration_index_ = configuration_index;
    wtf_size_t num_interfaces =
        Info().configurations[configuration_index_]->interfaces.size();
    claimed_interfaces_.ClearAll();
    claimed_interfaces_.Resize(num_interfaces);
    interface_state_change_in_progress_.ClearAll();
    interface_state_change_in_progress_.Resize(num_interfaces);
    selected_alternates_.resize(num_interfaces);
    selected_alternates_.Fill(0);
    in_endpoints_.ClearAll();
    out_endpoints_.ClearAll();
  }
  device_state_change_in_progress_ = false;
}

void USBDevice::AsyncClaimInterface(wtf_size_t interface_index,
                                    ScriptPromiseResolver* resolver,
                                    bool success) {
  if (!MarkRequestComplete(resolver))
    return;

  OnInterfaceClaimedOrUnclaimed(success, interface_index);
  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNetworkError,
                                          "Unable to claim interface."));
  }
}

void USBDevice::AsyncReleaseInterface(wtf_size_t interface_index,
                                      ScriptPromiseResolver* resolver,
                                      bool success) {
  if (!MarkRequestComplete(resolver))
    return;

  OnInterfaceClaimedOrUnclaimed(!success, interface_index);
  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNetworkError,
                                          "Unable to release interface."));
  }
}

void USBDevice::OnInterfaceClaimedOrUnclaimed(bool claimed,
                                              wtf_size_t interface_index) {
  if (claimed) {
    claimed_interfaces_.Set(interface_index);
  } else {
    claimed_interfaces_.Clear(interface_index);
    selected_alternates_[interface_index] = 0;
  }
  SetEndpointsForInterface(interface_index, claimed);
  interface_state_change_in_progress_.Clear(interface_index);
}

void USBDevice::AsyncSelectAlternateInterface(wtf_size_t interface_index,
                                              wtf_size_t alternate_index,
                                              ScriptPromiseResolver* resolver,
                                              bool success) {
  if (!MarkRequestComplete(resolver))
    return;

  if (success)
    selected_alternates_[interface_index] = alternate_index;
  SetEndpointsForInterface(interface_index, success);
  interface_state_change_in_progress_.Clear(interface_index);

  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNetworkError,
                                          "Unable to set device interface."));
  }
}

void USBDevice::AsyncControlTransferIn(ScriptPromiseResolver* resolver,
                                       UsbTransferStatus status,
                                       const Vector<uint8_t>& data) {
  if (!MarkRequestComplete(resolver))
    return;

  DOMException* error = ConvertFatalTransferStatus(status);
  if (error) {
    resolver->Reject(error);
  } else {
    resolver->Resolve(
        USBInTransferResult::Create(ConvertTransferStatus(status), data));
  }
}

void USBDevice::AsyncControlTransferOut(unsigned transfer_length,
                                        ScriptPromiseResolver* resolver,
                                        UsbTransferStatus status) {
  if (!MarkRequestComplete(resolver))
    return;

  DOMException* error = ConvertFatalTransferStatus(status);
  if (error) {
    resolver->Reject(error);
  } else {
    resolver->Resolve(USBOutTransferResult::Create(
        ConvertTransferStatus(status), transfer_length));
  }
}

void USBDevice::AsyncClearHalt(ScriptPromiseResolver* resolver, bool success) {
  if (!MarkRequestComplete(resolver))
    return;

  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNetworkError,
                                          "Unable to clear endpoint."));
  }
}

void USBDevice::AsyncTransferIn(ScriptPromiseResolver* resolver,
                                UsbTransferStatus status,
                                const Vector<uint8_t>& data) {
  if (!MarkRequestComplete(resolver))
    return;

  DOMException* error = ConvertFatalTransferStatus(status);
  if (error) {
    resolver->Reject(error);
  } else {
    resolver->Resolve(
        USBInTransferResult::Create(ConvertTransferStatus(status), data));
  }
}

void USBDevice::AsyncTransferOut(unsigned transfer_length,
                                 ScriptPromiseResolver* resolver,
                                 UsbTransferStatus status) {
  if (!MarkRequestComplete(resolver))
    return;

  DOMException* error = ConvertFatalTransferStatus(status);
  if (error) {
    resolver->Reject(error);
  } else {
    resolver->Resolve(USBOutTransferResult::Create(
        ConvertTransferStatus(status), transfer_length));
  }
}

void USBDevice::AsyncIsochronousTransferIn(
    ScriptPromiseResolver* resolver,
    const Vector<uint8_t>& data,
    Vector<UsbIsochronousPacketPtr> mojo_packets) {
  if (!MarkRequestComplete(resolver))
    return;

  DOMArrayBuffer* buffer = DOMArrayBuffer::Create(data.data(), data.size());
  HeapVector<Member<USBIsochronousInTransferPacket>> packets;
  packets.ReserveCapacity(mojo_packets.size());
  size_t byte_offset = 0;
  for (const auto& packet : mojo_packets) {
    DOMException* error = ConvertFatalTransferStatus(packet->status);
    if (error) {
      resolver->Reject(error);
      return;
    }
    DOMDataView* data_view = nullptr;
    if (buffer) {
      data_view =
          DOMDataView::Create(buffer, byte_offset, packet->transferred_length);
    }
    packets.push_back(USBIsochronousInTransferPacket::Create(
        ConvertTransferStatus(packet->status), data_view));
    byte_offset += packet->length;
  }
  resolver->Resolve(USBIsochronousInTransferResult::Create(buffer, packets));
}

void USBDevice::AsyncIsochronousTransferOut(
    ScriptPromiseResolver* resolver,
    Vector<UsbIsochronousPacketPtr> mojo_packets) {
  if (!MarkRequestComplete(resolver))
    return;

  HeapVector<Member<USBIsochronousOutTransferPacket>> packets;
  packets.ReserveCapacity(mojo_packets.size());
  for (const auto& packet : mojo_packets) {
    DOMException* error = ConvertFatalTransferStatus(packet->status);
    if (error) {
      resolver->Reject(error);
      return;
    }
    packets.push_back(USBIsochronousOutTransferPacket::Create(
        ConvertTransferStatus(packet->status), packet->transferred_length));
  }
  resolver->Resolve(USBIsochronousOutTransferResult::Create(packets));
}

void USBDevice::AsyncReset(ScriptPromiseResolver* resolver, bool success) {
  if (!MarkRequestComplete(resolver))
    return;

  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNetworkError,
                                          "Unable to reset the device."));
  }
}

void USBDevice::OnConnectionError() {
  device_.reset();
  opened_ = false;
  for (ScriptPromiseResolver* resolver : device_requests_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kDeviceDisconnected));
  }
  device_requests_.clear();
}

bool USBDevice::MarkRequestComplete(ScriptPromiseResolver* resolver) {
  auto request_entry = device_requests_.find(resolver);
  if (request_entry == device_requests_.end())
    return false;
  device_requests_.erase(request_entry);
  return true;
}

}  // namespace blink
