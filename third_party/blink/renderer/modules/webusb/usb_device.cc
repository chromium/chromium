// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_device.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include "build/chromeos_buildflags.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_control_transfer_parameters.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/webusb/usb_configuration.h"
#include "third_party/blink/renderer/modules/webusb/usb_in_transfer_result.h"
#include "third_party/blink/renderer/modules/webusb/usb_isochronous_in_transfer_result.h"
#include "third_party/blink/renderer/modules/webusb/usb_isochronous_out_transfer_result.h"
#include "third_party/blink/renderer/modules/webusb/usb_out_transfer_result.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using device::mojom::blink::UsbControlTransferParamsPtr;
using device::mojom::blink::UsbControlTransferRecipient;
using device::mojom::blink::UsbControlTransferType;
using device::mojom::blink::UsbDevice;
using device::mojom::blink::UsbDeviceInfoPtr;
using device::mojom::blink::UsbIsochronousPacketPtr;
using device::mojom::blink::UsbOpenDeviceError;
using device::mojom::blink::UsbTransferDirection;
using device::mojom::blink::UsbTransferStatus;

namespace blink {

namespace {

const char kBufferTooBig[] = "The data buffer exceeded its maximum size.";
const char kDetachedBuffer[] = "The data buffer has been detached.";
const char kDeviceStateChangeInProgress[] =
    "An operation that changes the device state is in progress.";
const char kDeviceDisconnected[] = "The device was disconnected.";
const char kInterfaceNotFound[] =
    "The interface number provided is not supported by the device in its "
    "current configuration.";
const char kInterfaceStateChangeInProgress[] =
    "An operation that changes interface state is in progress.";
const char kOpenRequired[] = "The device must be opened first.";

#if BUILDFLAG(IS_ASH)
const char kExtensionProtocol[] = "chrome-extension";

// These Imprivata extensions can claim the protected HID interface class (used
// as badge readers), see crbug.com/1065112 and crbug.com/995294.
// This list needs to be alphabetically sorted for quick access via binary
// search.
const char* kImprivataExtensionIds[] = {
    "baobpecgllpajfeojepgedjdlnlfffde", "bnfoibgpjolimhppjmligmcgklpboloj",
    "cdgickkdpbekbnalbmpgochbninibkko", "cjakdianfealdjlapagfagpdpemoppba",
    "cokoeepjbmmnhgdhlkpahohdaiedfjgn", "dahgfgiifpnaoajmloofonkndaaafacp",
    "dbknmmkopacopifbkgookcdbhfnggjjh", "ddcjglpbfbibgepfffpklmpihphbcdco",
    "dhodapiemamlmhlhblgcibabhdkohlen", "dlahpllbhpbkfnoiedkgombmegnnjopi",
    "egfpnfjeaopimgpiioeedbpmojdapaip", "fnbibocngjnefolmcodjkkghijpdlnfm",
    "jcnflhjcfjkplgkcinikhbgbhfldkadl", "jkfjfbelolphkjckiolfcakgalloegek",
    "kmhpgpnbglclbaccjjgoioogjlnfgbne", "lpimkpkllnkdlcigdbgmabfplniahkgm",
    "odehonhhkcjnbeaomlodfkjaecbmhklm", "olnmflhcfkifkgbiegcoabineoknmbjc",
    "omificdfgpipkkpdhbjmefgfgbppehke", "phjobickjiififdadeoepbdaciefacfj",
    "pkeacbojooejnjolgjdecbpnloibpafm", "pllbepacblmgialkkpcceohmjakafnbb",
    "plpogimmgnkkiflhpidbibfmgpkaofec", "pmhiabnkkchjeaehcodceadhdpfejmmd",
};
const char** kExtensionNameMappingsEnd = std::end(kImprivataExtensionIds);

bool IsCStrBefore(const char* first, const char* second) {
  return strcmp(first, second) < 0;
}

bool IsClassAllowedForExtension(uint8_t class_code, const KURL& url) {
  if (url.Protocol() != kExtensionProtocol)
    return false;

  switch (class_code) {
    case 0x03:  // HID
      DCHECK(std::is_sorted(kImprivataExtensionIds, kExtensionNameMappingsEnd,
                            IsCStrBefore));
      return std::binary_search(kImprivataExtensionIds,
                                kExtensionNameMappingsEnd,
                                url.Host().Utf8().c_str(), IsCStrBefore);
    default:
      return false;
  }
}
#endif  // BUILDFLAG(IS_ASH)

DOMException* ConvertFatalTransferStatus(const UsbTransferStatus& status) {
  switch (status) {
    case UsbTransferStatus::TRANSFER_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError, "A transfer error has occurred.");
    case UsbTransferStatus::PERMISSION_DENIED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "The transfer was not allowed.");
    case UsbTransferStatus::TIMEOUT:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kTimeoutError,
                                                "The transfer timed out.");
    case UsbTransferStatus::CANCELLED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                "The transfer was cancelled.");
    case UsbTransferStatus::DISCONNECT:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, kDeviceDisconnected);
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

bool ConvertBufferSource(const ArrayBufferOrArrayBufferView& buffer_source,
                         Vector<uint8_t>* vector,
                         ScriptPromiseResolver* resolver) {
  DCHECK(!buffer_source.IsNull());
  if (buffer_source.IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer = buffer_source.GetAsArrayBuffer();
    if (array_buffer->IsDetached()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, kDetachedBuffer));
      return false;
    }
    if (array_buffer->ByteLength() > std::numeric_limits<wtf_size_t>::max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kDataError, kBufferTooBig));
      return false;
    }

    vector->Append(static_cast<uint8_t*>(array_buffer->Data()),
                   static_cast<wtf_size_t>(array_buffer->ByteLength()));
  } else {
    DOMArrayBufferView* view = buffer_source.GetAsArrayBufferView().View();
    if (!view->buffer() || view->buffer()->IsDetached()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, kDetachedBuffer));
      return false;
    }
    if (view->byteLength() > std::numeric_limits<wtf_size_t>::max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kDataError, kBufferTooBig));
      return false;
    }

    vector->Append(static_cast<uint8_t*>(view->BaseAddress()),
                   static_cast<wtf_size_t>(view->byteLength()));
  }
  return true;
}

}  // namespace

USBDevice::USBDevice(UsbDeviceInfoPtr device_info,
                     mojo::PendingRemote<UsbDevice> device,
                     ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      device_info_(std::move(device_info)),
      device_(context),
      opened_(false),
      device_state_change_in_progress_(false),
      configuration_index_(kNotFound) {
  device_.Bind(std::move(device),
               context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  if (device_.is_bound()) {
    device_.set_disconnect_handler(
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
         claimed_interfaces_[interface_index];
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
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
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
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
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
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureNoDeviceOrInterfaceChangeInProgress(resolver)) {
    if (!opened_) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, kOpenRequired));
    } else {
      wtf_size_t configuration_index =
          FindConfigurationIndex(configuration_value);
      if (configuration_index == kNotFound) {
        resolver->Reject(
            MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotFoundError,
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
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureDeviceConfigured(resolver)) {
    wtf_size_t interface_index = FindInterfaceIndex(interface_number);
    if (interface_index == kNotFound) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, kInterfaceNotFound));
    } else if (interface_state_change_in_progress_[interface_index]) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          kInterfaceStateChangeInProgress));
    } else if (claimed_interfaces_[interface_index]) {
      resolver->Resolve();
    } else if (IsProtectedInterfaceClass(interface_index)) {
      GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::ConsoleMessageSource::kJavaScript,
              mojom::ConsoleMessageLevel::kWarning,
              "An attempt to claim a USB device interface "
              "has been blocked because it "
              "implements a protected interface class."));
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The requested interface implements a protected class."));
    } else {
      interface_state_change_in_progress_[interface_index] = true;
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
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureDeviceConfigured(resolver)) {
    wtf_size_t interface_index = FindInterfaceIndex(interface_number);
    if (interface_index == kNotFound) {
      resolver->Reject(
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotFoundError,
                                             "The interface number provided is "
                                             "not supported by the device in "
                                             "its current configuration."));
    } else if (interface_state_change_in_progress_[interface_index]) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          kInterfaceStateChangeInProgress));
    } else if (!claimed_interfaces_[interface_index]) {
      resolver->Resolve();
    } else {
      // Mark this interface's endpoints unavailable while its state is
      // changing.
      SetEndpointsForInterface(interface_index, false);
      interface_state_change_in_progress_[interface_index] = true;
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
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureInterfaceClaimed(interface_number, resolver)) {
    // TODO(reillyg): This is duplicated work.
    wtf_size_t interface_index = FindInterfaceIndex(interface_number);
    DCHECK_NE(interface_index, kNotFound);
    wtf_size_t alternate_index =
        FindAlternateIndex(interface_index, alternate_setting);
    if (alternate_index == kNotFound) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError,
          "The alternate setting provided is "
          "not supported by the device in "
          "its current configuration."));
    } else {
      // Mark this old alternate interface's endpoints unavailable while
      // the change is in progress.
      SetEndpointsForInterface(interface_index, false);
      interface_state_change_in_progress_[interface_index] = true;
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
    const USBControlTransferParameters* setup,
    unsigned length) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureNoDeviceOrInterfaceChangeInProgress(resolver))
    return promise;

  if (!opened_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kOpenRequired));
    return promise;
  }

  auto parameters = ConvertControlTransferParameters(setup, resolver);
  if (!parameters)
    return promise;

  device_requests_.insert(resolver);
  device_->ControlTransferIn(
      std::move(parameters), length, 0,
      WTF::Bind(&USBDevice::AsyncControlTransferIn, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

ScriptPromise USBDevice::controlTransferOut(
    ScriptState* script_state,
    const USBControlTransferParameters* setup) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureNoDeviceOrInterfaceChangeInProgress(resolver))
    return promise;

  if (!opened_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kOpenRequired));
    return promise;
  }

  auto parameters = ConvertControlTransferParameters(setup, resolver);
  if (!parameters)
    return promise;

  device_requests_.insert(resolver);
  device_->ControlTransferOut(
      std::move(parameters), Vector<uint8_t>(), 0,
      WTF::Bind(&USBDevice::AsyncControlTransferOut, WrapPersistent(this), 0,
                WrapPersistent(resolver)));
  return promise;
}

ScriptPromise USBDevice::controlTransferOut(
    ScriptState* script_state,
    const USBControlTransferParameters* setup,
    const ArrayBufferOrArrayBufferView& data) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureNoDeviceOrInterfaceChangeInProgress(resolver))
    return promise;

  if (!opened_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kOpenRequired));
    return promise;
  }

  auto parameters = ConvertControlTransferParameters(setup, resolver);
  if (!parameters)
    return promise;

  Vector<uint8_t> buffer;
  if (!ConvertBufferSource(data, &buffer, resolver))
    return promise;

  unsigned transfer_length = buffer.size();
  device_requests_.insert(resolver);
  device_->ControlTransferOut(
      std::move(parameters), buffer, 0,
      WTF::Bind(&USBDevice::AsyncControlTransferOut, WrapPersistent(this),
                transfer_length, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise USBDevice::clearHalt(ScriptState* script_state,
                                   String direction,
                                   uint8_t endpoint_number) {
  UsbTransferDirection mojo_direction = direction == "in"
                                            ? UsbTransferDirection::INBOUND
                                            : UsbTransferDirection::OUTBOUND;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureEndpointAvailable(direction == "in", endpoint_number, resolver)) {
    device_requests_.insert(resolver);
    device_->ClearHalt(
        mojo_direction, endpoint_number,
        WTF::Bind(&USBDevice::AsyncClearHalt, WrapPersistent(this),
                  WrapPersistent(resolver)));
  }
  return promise;
}

ScriptPromise USBDevice::transferIn(ScriptState* script_state,
                                    uint8_t endpoint_number,
                                    unsigned length) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
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
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureEndpointAvailable(false /* out */, endpoint_number, resolver))
    return promise;

  Vector<uint8_t> buffer;
  if (!ConvertBufferSource(data, &buffer, resolver))
    return promise;

  unsigned transfer_length = buffer.size();
  device_requests_.insert(resolver);
  device_->GenericTransferOut(
      endpoint_number, buffer, 0,
      WTF::Bind(&USBDevice::AsyncTransferOut, WrapPersistent(this),
                transfer_length, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise USBDevice::isochronousTransferIn(
    ScriptState* script_state,
    uint8_t endpoint_number,
    Vector<unsigned> packet_lengths) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
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
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureEndpointAvailable(false /* out */, endpoint_number, resolver))
    return promise;

  Vector<uint8_t> buffer;
  if (!ConvertBufferSource(data, &buffer, resolver))
    return promise;

  device_requests_.insert(resolver);
  device_->IsochronousTransferOut(
      endpoint_number, buffer, packet_lengths, 0,
      WTF::Bind(&USBDevice::AsyncIsochronousTransferOut, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

ScriptPromise USBDevice::reset(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (EnsureNoDeviceOrInterfaceChangeInProgress(resolver)) {
    if (!opened_) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, kOpenRequired));
    } else {
      device_requests_.insert(resolver);
      device_->Reset(WTF::Bind(&USBDevice::AsyncReset, WrapPersistent(this),
                               WrapPersistent(resolver)));
    }
  }
  return promise;
}

void USBDevice::ContextDestroyed() {
  device_requests_.clear();
}

void USBDevice::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(device_requests_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
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
  // https://www.usb.org/defined-class-codes
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
#if BUILDFLAG(IS_ASH)
      return !IsClassAllowedForExtension(alternate->class_code,
                                         GetExecutionContext()->Url());
#else
      return true;
#endif
    }
  }

  return false;
}

bool USBDevice::EnsureNoDeviceChangeInProgress(
    ScriptPromiseResolver* resolver) const {
  if (!device_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kDeviceDisconnected));
    return false;
  }

  if (device_state_change_in_progress_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kDeviceStateChangeInProgress));
    return false;
  }

  return true;
}

bool USBDevice::EnsureNoDeviceOrInterfaceChangeInProgress(
    ScriptPromiseResolver* resolver) const {
  if (!EnsureNoDeviceChangeInProgress(resolver))
    return false;

  if (AnyInterfaceChangeInProgress()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInterfaceStateChangeInProgress));
    return false;
  }

  return true;
}

bool USBDevice::EnsureDeviceConfigured(ScriptPromiseResolver* resolver) const {
  if (!EnsureNoDeviceChangeInProgress(resolver))
    return false;

  if (!opened_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kOpenRequired));
    return false;
  }

  if (configuration_index_ == kNotFound) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The device must have a configuration selected."));
    return false;
  }

  return true;
}

bool USBDevice::EnsureInterfaceClaimed(uint8_t interface_number,
                                       ScriptPromiseResolver* resolver) const {
  if (!EnsureDeviceConfigured(resolver))
    return false;

  wtf_size_t interface_index = FindInterfaceIndex(interface_number);
  if (interface_index == kNotFound) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kInterfaceNotFound));
    return false;
  }

  if (interface_state_change_in_progress_[interface_index]) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInterfaceStateChangeInProgress));
    return false;
  }

  if (!claimed_interfaces_[interface_index]) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The specified interface has not been claimed."));
    return false;
  }

  return true;
}

bool USBDevice::EnsureEndpointAvailable(bool in_transfer,
                                        uint8_t endpoint_number,
                                        ScriptPromiseResolver* resolver) const {
  if (!EnsureDeviceConfigured(resolver))
    return false;

  if (endpoint_number == 0 || endpoint_number >= kEndpointsBitsNumber) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kIndexSizeError,
        "The specified endpoint number is out of range."));
    return false;
  }

  auto& bit_vector = in_transfer ? in_endpoints_ : out_endpoints_;
  if (!bit_vector[endpoint_number - 1]) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError,
        "The specified endpoint is not part "
        "of a claimed and selected alternate "
        "interface."));
    return false;
  }

  return true;
}

bool USBDevice::AnyInterfaceChangeInProgress() const {
  for (wtf_size_t i = 0; i < interface_state_change_in_progress_.size(); ++i) {
    if (interface_state_change_in_progress_[i])
      return true;
  }
  return false;
}

UsbControlTransferParamsPtr USBDevice::ConvertControlTransferParameters(
    const USBControlTransferParameters* parameters,
    ScriptPromiseResolver* resolver) const {
  auto mojo_parameters = device::mojom::blink::UsbControlTransferParams::New();

  if (parameters->requestType() == "standard") {
    mojo_parameters->type = UsbControlTransferType::STANDARD;
  } else if (parameters->requestType() == "class") {
    mojo_parameters->type = UsbControlTransferType::CLASS;
  } else if (parameters->requestType() == "vendor") {
    mojo_parameters->type = UsbControlTransferType::VENDOR;
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kTypeMismatchError,
        "The control transfer requestType parameter is invalid."));
    return nullptr;
  }

  if (parameters->recipient() == "device") {
    mojo_parameters->recipient = UsbControlTransferRecipient::DEVICE;
  } else if (parameters->recipient() == "interface") {
    uint8_t interface_number = parameters->index() & 0xff;
    if (!EnsureInterfaceClaimed(interface_number, resolver))
      return nullptr;
    mojo_parameters->recipient = UsbControlTransferRecipient::INTERFACE;
  } else if (parameters->recipient() == "endpoint") {
    bool in_transfer = parameters->index() & 0x80;
    uint8_t endpoint_number = parameters->index() & 0x0f;
    if (!EnsureEndpointAvailable(in_transfer, endpoint_number, resolver))
      return nullptr;
    mojo_parameters->recipient = UsbControlTransferRecipient::ENDPOINT;
  } else if (parameters->recipient() == "other") {
    mojo_parameters->recipient = UsbControlTransferRecipient::OTHER;
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kTypeMismatchError,
        "The control transfer recipient parameter is invalid."));
    return nullptr;
  }

  mojo_parameters->request = parameters->request();
  mojo_parameters->value = parameters->value();
  mojo_parameters->index = parameters->index();
  return mojo_parameters;
}

void USBDevice::SetEndpointsForInterface(wtf_size_t interface_index, bool set) {
  const auto& configuration = *Info().configurations[configuration_index_];
  const auto& interface = *configuration.interfaces[interface_index];
  const auto& alternate =
      *interface.alternates[selected_alternates_[interface_index]];
  for (const auto& endpoint : alternate.endpoints) {
    uint8_t endpoint_number = endpoint->endpoint_number;
    if (endpoint_number == 0 || endpoint_number >= kEndpointsBitsNumber)
      continue;  // Ignore endpoints with invalid indices.
    auto& bit_vector = endpoint->direction == UsbTransferDirection::INBOUND
                           ? in_endpoints_
                           : out_endpoints_;
    if (set)
      bit_vector.set(endpoint_number - 1);
    else
      bit_vector.reset(endpoint_number - 1);
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
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "Access denied."));
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
    claimed_interfaces_.Fill(false);
    selected_alternates_.Fill(0);
    in_endpoints_.reset();
    out_endpoints_.reset();
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
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError,
        "Unable to set device configuration."));
  }
}

void USBDevice::OnConfigurationSelected(bool success,
                                        wtf_size_t configuration_index) {
  if (success) {
    configuration_index_ = configuration_index;
    wtf_size_t num_interfaces =
        Info().configurations[configuration_index_]->interfaces.size();
    claimed_interfaces_.resize(num_interfaces);
    claimed_interfaces_.Fill(false);
    interface_state_change_in_progress_.resize(num_interfaces);
    interface_state_change_in_progress_.Fill(false);
    selected_alternates_.resize(num_interfaces);
    selected_alternates_.Fill(0);
    in_endpoints_.reset();
    out_endpoints_.reset();
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
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Unable to claim interface."));
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
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Unable to release interface."));
  }
}

void USBDevice::OnInterfaceClaimedOrUnclaimed(bool claimed,
                                              wtf_size_t interface_index) {
  if (claimed) {
    claimed_interfaces_[interface_index] = true;
  } else {
    claimed_interfaces_[interface_index] = false;
    selected_alternates_[interface_index] = 0;
  }
  SetEndpointsForInterface(interface_index, claimed);
  interface_state_change_in_progress_[interface_index] = false;
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
  interface_state_change_in_progress_[interface_index] = false;

  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Unable to set device interface."));
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
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Unable to clear endpoint."));
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
  uint32_t byte_offset = 0;
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
        ConvertTransferStatus(packet->status),
        NotShared<DOMDataView>(data_view)));
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
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNetworkError, "Unable to reset the device."));
  }
}

void USBDevice::OnConnectionError() {
  device_.reset();
  opened_ = false;

  // Move the set to a local variable to prevent script execution in Reject()
  // from invalidating the iterator used by the loop.
  HeapHashSet<Member<ScriptPromiseResolver>> device_requests;
  device_requests.swap(device_requests_);
  for (auto& resolver : device_requests) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kDeviceDisconnected));
  }
}

bool USBDevice::MarkRequestComplete(ScriptPromiseResolver* resolver) {
  auto request_entry = device_requests_.find(resolver);
  if (request_entry == device_requests_.end())
    return false;
  device_requests_.erase(request_entry);
  return true;
}

}  // namespace blink
