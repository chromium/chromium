// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_device.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/hid/hid.h"
#include "third_party/blink/renderer/modules/hid/hid_collection_info.h"
#include "third_party/blink/renderer/modules/hid/hid_input_report_event.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

const char kDeviceStateChangeInProgress[] =
    "An operation that changes the device state is in progress.";
const char kOpenRequired[] = "The device must be opened first.";
const char kOpenFailed[] = "Failed to open the device.";
const char kSendReportFailed[] = "Failed to write the report.";
const char kSendFeatureReportFailed[] = "Failed to write the feature report.";
const char kReceiveFeatureReportFailed[] =
    "Failed to receive the feature report.";
const char kUnexpectedClose[] = "The device was closed unexpectedly.";

Vector<uint8_t> ConvertBufferSource(
    const ArrayBufferOrArrayBufferView& buffer) {
  DCHECK(!buffer.IsNull());
  Vector<uint8_t> vector;
  if (buffer.IsArrayBuffer()) {
    vector.Append(static_cast<uint8_t*>(buffer.GetAsArrayBuffer()->Data()),
                  buffer.GetAsArrayBuffer()->DeprecatedByteLengthAsUnsigned());
  } else {
    vector.Append(static_cast<uint8_t*>(
                      buffer.GetAsArrayBufferView().View()->BaseAddress()),
                  buffer.GetAsArrayBufferView().View()->byteLength());
  }
  return vector;
}

bool IsProtected(
    const device::mojom::blink::HidUsageAndPage& hid_usage_and_page) {
  const uint16_t usage = hid_usage_and_page.usage;
  const uint16_t usage_page = hid_usage_and_page.usage_page;

  if (usage_page == device::mojom::blink::kPageFido)
    return true;

  if (usage_page == device::mojom::blink::kPageKeyboard)
    return true;

  if (usage_page != device::mojom::blink::kPageGenericDesktop)
    return false;

  if (usage == device::mojom::blink::kGenericDesktopPointer ||
      usage == device::mojom::blink::kGenericDesktopMouse ||
      usage == device::mojom::blink::kGenericDesktopKeyboard ||
      usage == device::mojom::blink::kGenericDesktopKeypad) {
    return true;
  }

  if (usage >= device::mojom::blink::kGenericDesktopSystemControl &&
      usage <= device::mojom::blink::kGenericDesktopSystemWarmRestart) {
    return true;
  }

  if (usage >= device::mojom::blink::kGenericDesktopSystemDock &&
      usage <= device::mojom::blink::kGenericDesktopSystemDisplaySwap) {
    return true;
  }

  return false;
}

}  // namespace

HIDDevice::HIDDevice(HID* parent,
                     device::mojom::blink::HidDeviceInfoPtr info,
                     ExecutionContext* context)
    : ContextLifecycleObserver(context),
      parent_(parent),
      device_info_(std::move(info)) {
  DCHECK(device_info_);
  for (const auto& collection : device_info_->collections) {
    // Omit information about top-level collections with protected usages.
    if (!IsProtected(*collection->usage)) {
      collections_.push_back(
          MakeGarbageCollected<HIDCollectionInfo>(*collection));
    }
  }
}

HIDDevice::~HIDDevice() {
  DCHECK(device_requests_.IsEmpty());
}

ExecutionContext* HIDDevice::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& HIDDevice::InterfaceName() const {
  return event_target_names::kHIDDevice;
}

void HIDDevice::OnInputReport(uint8_t report_id,
                              const Vector<uint8_t>& buffer) {
  DispatchEvent(*MakeGarbageCollected<HIDInputReportEvent>(
      event_type_names::kInputreport, this, report_id, buffer));
}

bool HIDDevice::opened() const {
  return connection_.is_bound();
}

uint16_t HIDDevice::vendorId() const {
  return device_info_->vendor_id;
}

uint16_t HIDDevice::productId() const {
  return device_info_->product_id;
}

String HIDDevice::productName() const {
  return device_info_->product_name;
}

const HeapVector<Member<HIDCollectionInfo>>& HIDDevice::collections() const {
  return collections_;
}

ScriptPromise HIDDevice::open(ScriptState* script_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver))
    return promise;

  if (opened()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "The device is already open."));
    return promise;
  }

  mojo::PendingRemote<device::mojom::blink::HidConnectionClient> client;
  receiver_.Bind(client.InitWithNewPipeAndPassReceiver());

  device_state_change_in_progress_ = true;
  device_requests_.insert(resolver);
  parent_->Connect(device_info_->guid, std::move(client),
                   WTF::Bind(&HIDDevice::FinishOpen, WrapPersistent(this),
                             WrapPersistent(resolver)));
  return promise;
}

ScriptPromise HIDDevice::close(ScriptState* script_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver))
    return promise;

  connection_.reset();
  resolver->Resolve();
  return promise;
}

ScriptPromise HIDDevice::sendReport(ScriptState* script_state,
                                    uint8_t report_id,
                                    const ArrayBufferOrArrayBufferView& data) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver))
    return promise;

  if (!opened()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kOpenRequired));
    return promise;
  }

  device_requests_.insert(resolver);
  connection_->Write(report_id, ConvertBufferSource(data),
                     WTF::Bind(&HIDDevice::FinishSendReport,
                               WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise HIDDevice::sendFeatureReport(
    ScriptState* script_state,
    uint8_t report_id,
    const ArrayBufferOrArrayBufferView& data) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver))
    return promise;

  if (!opened()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kOpenRequired));
    return promise;
  }

  device_requests_.insert(resolver);
  connection_->SendFeatureReport(
      report_id, ConvertBufferSource(data),
      WTF::Bind(&HIDDevice::FinishSendFeatureReport, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

ScriptPromise HIDDevice::receiveFeatureReport(ScriptState* script_state,
                                              uint8_t report_id) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver))
    return promise;

  if (!opened()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kOpenRequired));
    return promise;
  }

  device_requests_.insert(resolver);
  connection_->GetFeatureReport(
      report_id, WTF::Bind(&HIDDevice::FinishReceiveFeatureReport,
                           WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void HIDDevice::ContextDestroyed(ExecutionContext*) {
  connection_.reset();
  device_requests_.clear();
  receiver_.reset();
}

void HIDDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_);
  visitor->Trace(device_requests_);
  visitor->Trace(collections_);
  EventTargetWithInlineData::Trace(visitor);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void HIDDevice::Dispose() {
  // The connection client binding holds a raw pointer to this object which must
  // be released when it becomes garbage.
  receiver_.reset();
}

bool HIDDevice::EnsureNoDeviceChangeInProgress(
    ScriptPromiseResolver* resolver) const {
  if (device_state_change_in_progress_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kDeviceStateChangeInProgress));
    return false;
  }
  return true;
}

void HIDDevice::FinishOpen(
    ScriptPromiseResolver* resolver,
    mojo::PendingRemote<device::mojom::blink::HidConnection> connection) {
  MarkRequestComplete(resolver);
  device_state_change_in_progress_ = false;

  if (connection) {
    connection_.Bind(std::move(connection));
    connection_.set_disconnect_handler(WTF::Bind(
        &HIDDevice::OnServiceConnectionError, WrapWeakPersistent(this)));
    resolver->Resolve();
  } else {
    // If the connection is null, the open failed.
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kOpenFailed));
  }
}

void HIDDevice::OnServiceConnectionError() {
  for (auto& resolver : device_requests_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kUnexpectedClose));
  }
  device_requests_.clear();
}

void HIDDevice::FinishClose(ScriptPromiseResolver* resolver) {
  MarkRequestComplete(resolver);
  connection_.reset();
  resolver->Resolve();
}

void HIDDevice::FinishSendReport(ScriptPromiseResolver* resolver,
                                 bool success) {
  MarkRequestComplete(resolver);
  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kSendReportFailed));
  }
}

void HIDDevice::FinishSendFeatureReport(ScriptPromiseResolver* resolver,
                                        bool success) {
  MarkRequestComplete(resolver);
  if (success) {
    resolver->Resolve();
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kSendFeatureReportFailed));
  }
}

void HIDDevice::FinishReceiveFeatureReport(
    ScriptPromiseResolver* resolver,
    bool success,
    const base::Optional<Vector<uint8_t>>& data) {
  MarkRequestComplete(resolver);
  if (success && data) {
    DOMArrayBuffer* dom_buffer =
        DOMArrayBuffer::Create(data->data(), data->size());
    DOMDataView* data_view = DOMDataView::Create(dom_buffer, 0, data->size());
    resolver->Resolve(data_view);
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kReceiveFeatureReportFailed));
  }
}

void HIDDevice::MarkRequestComplete(ScriptPromiseResolver* resolver) {
  auto find_result = device_requests_.find(resolver);
  DCHECK_NE(device_requests_.end(), find_result);
  device_requests_.erase(find_result);
}

}  // namespace blink
