// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_device.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_collection_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_report_info.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/hid/hid_input_report_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

const char kDeviceStateChangeInProgress[] =
    "An operation that changes the device state is in progress.";
const char kDeviceIsForgotten[] = "The device is forgotten.";
const char kOpenRequired[] = "The device must be opened first.";
const char kOpenFailed[] = "Failed to open the device.";
const char kSendReportFailed[] = "Failed to write the report.";
const char kSendFeatureReportFailed[] = "Failed to write the feature report.";
const char kReceiveFeatureReportFailed[] =
    "Failed to receive the feature report.";
const char kUnexpectedClose[] = "The device was closed unexpectedly.";
const char kArrayBufferTooBig[] =
    "The provided ArrayBuffer exceeds the maximum allowed size.";
const char kContextGone[] = "Script context has shut down.";

enum ReportType {
  kInput,
  kOutput,
  kFeature,
};

bool IsProtectedReportType(
    const device::mojom::blink::HidUsageAndPage& hid_usage_and_page,
    ReportType report_type) {
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
    return report_type != ReportType::kFeature;
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

// The HID specification defines four canonical unit systems. Each unit system
// corresponds to a set of units for length, mass, time, temperature, current,
// and luminous intensity. The vendor-defined unit system can be used for
// devices which produce measurements that cannot be adequately described by
// these unit systems.
//
// See the Units table in section 6.2.2.7 of the Device Class Definition for
// HID v1.11.
// https://www.usb.org/document-library/device-class-definition-hid-111
enum HidUnitSystem {
  // none: No unit system
  kUnitSystemNone = 0x00,
  // si-linear: Centimeter, Gram, Seconds, Kelvin, Ampere, Candela
  kUnitSystemSILinear = 0x01,
  // si-rotation: Radians, Gram, Seconds, Kelvin, Ampere, Candela
  kUnitSystemSIRotation = 0x02,
  // english-linear: Inch, Slug, Seconds, Fahrenheit, Ampere, Candela
  kUnitSystemEnglishLinear = 0x03,
  // english-linear: Degrees, Slug, Seconds, Fahrenheit, Ampere, Candela
  kUnitSystemEnglishRotation = 0x04,
  // vendor-defined unit system
  kUnitSystemVendorDefined = 0x0f,
};

uint32_t ConvertHidUsageAndPageToUint32(
    const device::mojom::blink::HidUsageAndPage& usage) {
  return (usage.usage_page) << 16 | usage.usage;
}

String UnitSystemToString(uint8_t unit) {
  DCHECK_LE(unit, 0x0f);
  switch (unit) {
    case kUnitSystemNone:
      return "none";
    case kUnitSystemSILinear:
      return "si-linear";
    case kUnitSystemSIRotation:
      return "si-rotation";
    case kUnitSystemEnglishLinear:
      return "english-linear";
    case kUnitSystemEnglishRotation:
      return "english-rotation";
    case kUnitSystemVendorDefined:
      return "vendor-defined";
    default:
      break;
  }
  // Values other than those defined in HidUnitSystem are reserved by the spec.
  return "reserved";
}

// Convert |unit_factor_exponent| from its coded representation to a signed
// integer type.
int8_t UnitFactorExponentToInt(uint8_t unit_factor_exponent) {
  DCHECK_LE(unit_factor_exponent, 0x0f);
  // Values from 0x08 to 0x0f encode negative exponents.
  if (unit_factor_exponent > 0x08)
    return static_cast<int8_t>(unit_factor_exponent) - 16;
  return unit_factor_exponent;
}

// Unpack the 32-bit unit definition value |unit| into each of its components.
// The unit definition value includes the unit system as well as unit factor
// exponents for each of the 6 units defined by the unit system.
void UnpackUnitValues(uint32_t unit,
                      String& unit_system,
                      int8_t& length_exponent,
                      int8_t& mass_exponent,
                      int8_t& time_exponent,
                      int8_t& temperature_exponent,
                      int8_t& current_exponent,
                      int8_t& luminous_intensity_exponent) {
  unit_system = UnitSystemToString(unit & 0x0f);
  length_exponent = UnitFactorExponentToInt((unit >> 4) & 0x0f);
  mass_exponent = UnitFactorExponentToInt((unit >> 8) & 0x0f);
  time_exponent = UnitFactorExponentToInt((unit >> 12) & 0x0f);
  temperature_exponent = UnitFactorExponentToInt((unit >> 16) & 0x0f);
  current_exponent = UnitFactorExponentToInt((unit >> 20) & 0x0f);
  luminous_intensity_exponent = UnitFactorExponentToInt((unit >> 24) & 0x0f);
}

HIDReportInfo* ToHIDReportInfo(
    const device::mojom::blink::HidReportDescription& report_info) {
  HIDReportInfo* result = HIDReportInfo::Create();
  result->setReportId(report_info.report_id);

  HeapVector<Member<HIDReportItem>> items;
  for (const auto& item : report_info.items)
    items.push_back(HIDDevice::ToHIDReportItem(*item));
  result->setItems(items);

  return result;
}

HIDCollectionInfo* ToHIDCollectionInfo(
    const device::mojom::blink::HidCollectionInfo& collection) {
  HIDCollectionInfo* result = HIDCollectionInfo::Create();
  result->setUsage(collection.usage->usage);
  result->setUsagePage(collection.usage->usage_page);
  result->setType(collection.collection_type);

  HeapVector<Member<HIDReportInfo>> input_reports;
  for (const auto& report : collection.input_reports)
    input_reports.push_back(ToHIDReportInfo(*report));
  result->setInputReports(input_reports);

  HeapVector<Member<HIDReportInfo>> output_reports;
  for (const auto& report : collection.output_reports)
    output_reports.push_back(ToHIDReportInfo(*report));
  result->setOutputReports(output_reports);

  HeapVector<Member<HIDReportInfo>> feature_reports;
  for (const auto& report : collection.feature_reports)
    feature_reports.push_back(ToHIDReportInfo(*report));
  result->setFeatureReports(feature_reports);

  HeapVector<Member<HIDCollectionInfo>> children;
  for (const auto& child : collection.children)
    children.push_back(ToHIDCollectionInfo(*child));
  result->setChildren(children);

  return result;
}

}  // namespace

HIDDevice::HIDDevice(ServiceInterface* parent,
                     device::mojom::blink::HidDeviceInfoPtr info,
                     ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      ActiveScriptWrappable<HIDDevice>({}),
      parent_(parent),
      connection_(context),
      receiver_(this, context) {
  UpdateDeviceInfo(std::move(info));
}

HIDDevice::~HIDDevice() {
  DCHECK(device_requests_.empty());
}

ExecutionContext* HIDDevice::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
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

ScriptPromise<IDLUndefined> HIDDevice::open(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kContextGone);
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver) ||
      !EnsureDeviceIsNotForgotten(resolver)) {
    return promise;
  }

  if (opened()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "The device is already open.");
    return promise;
  }

  mojo::PendingRemote<device::mojom::blink::HidConnectionClient> client;
  receiver_.Bind(client.InitWithNewPipeAndPassReceiver(),
                 ExecutionContext::From(script_state)
                     ->GetTaskRunner(TaskType::kMiscPlatformAPI));

  device_state_change_in_progress_ = true;
  device_requests_.insert(resolver);
  parent_->Connect(device_info_->guid, std::move(client),
                   WTF::BindOnce(&HIDDevice::FinishOpen, WrapPersistent(this),
                                 WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLUndefined> HIDDevice::close(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver) ||
      !EnsureDeviceIsNotForgotten(resolver)) {
    return promise;
  }

  connection_.reset();
  receiver_.reset();
  resolver->Resolve();
  return promise;
}

ScriptPromise<IDLUndefined> HIDDevice::forget(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kContextGone);
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver))
    return promise;

  device_state_change_in_progress_ = true;
  parent_->Forget(device_info_.Clone(),
                  WTF::BindOnce(&HIDDevice::FinishForget, WrapPersistent(this),
                                WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLUndefined> HIDDevice::sendReport(ScriptState* script_state,
                                                  uint8_t report_id,
                                                  const DOMArrayPiece& data) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver) ||
      !EnsureDeviceIsNotForgotten(resolver)) {
    return promise;
  }

  if (!opened()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kOpenRequired);
    return promise;
  }

  if (!base::CheckedNumeric<wtf_size_t>(data.ByteLength()).IsValid()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                     kArrayBufferTooBig);
    return promise;
  }

  Vector<uint8_t> vector;
  vector.AppendSpan(data.ByteSpan());

  device_requests_.insert(resolver);
  connection_->Write(
      report_id, vector,
      WTF::BindOnce(&HIDDevice::FinishSendReport, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLUndefined> HIDDevice::sendFeatureReport(
    ScriptState* script_state,
    uint8_t report_id,
    const DOMArrayPiece& data) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver) ||
      !EnsureDeviceIsNotForgotten(resolver)) {
    return promise;
  }

  if (!opened()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kOpenRequired);
    return promise;
  }

  if (!base::CheckedNumeric<wtf_size_t>(data.ByteLength()).IsValid()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                     kArrayBufferTooBig);
    return promise;
  }

  Vector<uint8_t> vector;
  vector.AppendSpan(data.ByteSpan());

  device_requests_.insert(resolver);
  connection_->SendFeatureReport(
      report_id, vector,
      WTF::BindOnce(&HIDDevice::FinishSendFeatureReport, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<NotShared<DOMDataView>> HIDDevice::receiveFeatureReport(
    ScriptState* script_state,
    uint8_t report_id) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<NotShared<DOMDataView>>>(
          script_state);
  auto promise = resolver->Promise();
  if (!EnsureNoDeviceChangeInProgress(resolver) ||
      !EnsureDeviceIsNotForgotten(resolver)) {
    return promise;
  }

  if (!opened()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kOpenRequired));
    return promise;
  }

  device_requests_.insert(resolver);
  connection_->GetFeatureReport(
      report_id, WTF::BindOnce(&HIDDevice::FinishReceiveFeatureReport,
                               WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void HIDDevice::ContextDestroyed() {
  device_requests_.clear();
}

bool HIDDevice::HasPendingActivity() const {
  // The object should be considered active if it is connected and has at least
  // one event listener.
  return connection_.is_bound() && HasEventListeners();
}

void HIDDevice::UpdateDeviceInfo(device::mojom::blink::HidDeviceInfoPtr info) {
  device_info_ = std::move(info);
  collections_.clear();
  for (const auto& collection : device_info_->collections) {
    auto* collection_info = ToHIDCollectionInfo(*collection);
    // Omit information about protected reports.
    if (IsProtectedReportType(*collection->usage, ReportType::kInput)) {
      collection_info->setInputReports(HeapVector<Member<HIDReportInfo>>{});
    }
    if (IsProtectedReportType(*collection->usage, ReportType::kOutput)) {
      collection_info->setOutputReports(HeapVector<Member<HIDReportInfo>>{});
    }
    if (IsProtectedReportType(*collection->usage, ReportType::kFeature)) {
      collection_info->setFeatureReports(HeapVector<Member<HIDReportInfo>>{});
    }
    collections_.push_back(collection_info);
  }
}

void HIDDevice::ResetIsForgotten() {
  device_is_forgotten_ = false;
}

void HIDDevice::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(connection_);
  visitor->Trace(receiver_);
  visitor->Trace(device_requests_);
  visitor->Trace(collections_);
  EventTarget::Trace(visitor);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

bool HIDDevice::EnsureNoDeviceChangeInProgress(
    ScriptPromiseResolverBase* resolver) const {
  if (device_state_change_in_progress_) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kDeviceStateChangeInProgress);
    return false;
  }
  return true;
}

bool HIDDevice::EnsureDeviceIsNotForgotten(
    ScriptPromiseResolverBase* resolver) const {
  if (device_is_forgotten_) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kDeviceIsForgotten);
    return false;
  }
  return true;
}

void HIDDevice::FinishOpen(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    mojo::PendingRemote<device::mojom::blink::HidConnection> connection) {
  MarkRequestComplete(resolver);
  device_state_change_in_progress_ = false;

  if (connection && GetExecutionContext()) {
    connection_.Bind(
        std::move(connection),
        GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
    connection_.set_disconnect_handler(WTF::BindOnce(
        &HIDDevice::OnServiceConnectionError, WrapWeakPersistent(this)));
    resolver->Resolve();
  } else {
    // If the connection or the context is null, the open failed.
    receiver_.reset();
    resolver->RejectWithDOMException(DOMExceptionCode::kNotAllowedError,
                                     kOpenFailed);
  }
}

void HIDDevice::FinishForget(ScriptPromiseResolver<IDLUndefined>* resolver) {
  device_state_change_in_progress_ = false;
  device_is_forgotten_ = true;
  connection_.reset();
  receiver_.reset();
  resolver->Resolve();
}

void HIDDevice::OnServiceConnectionError() {
  for (auto& resolver : device_requests_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kUnexpectedClose));
  }
  device_requests_.clear();
}

void HIDDevice::FinishSendReport(ScriptPromiseResolver<IDLUndefined>* resolver,
                                 bool success) {
  MarkRequestComplete(resolver);
  if (success) {
    resolver->Resolve();
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotAllowedError,
                                     kSendReportFailed);
  }
}

void HIDDevice::FinishSendFeatureReport(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    bool success) {
  MarkRequestComplete(resolver);
  if (success) {
    resolver->Resolve();
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotAllowedError,
                                     kSendFeatureReportFailed);
  }
}

void HIDDevice::FinishReceiveFeatureReport(
    ScriptPromiseResolver<NotShared<DOMDataView>>* resolver,
    bool success,
    const std::optional<Vector<uint8_t>>& data) {
  MarkRequestComplete(resolver);
  if (success && data) {
    DOMArrayBuffer* dom_buffer = DOMArrayBuffer::Create(data.value());
    DOMDataView* data_view = DOMDataView::Create(dom_buffer, 0, data->size());
    resolver->Resolve(NotShared(data_view));
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kReceiveFeatureReportFailed));
  }
}

void HIDDevice::MarkRequestComplete(ScriptPromiseResolverBase* resolver) {
  auto find_result = device_requests_.find(resolver);
  CHECK_NE(device_requests_.end(), find_result);
  device_requests_.erase(find_result);
}

// static
HIDReportItem* HIDDevice::ToHIDReportItem(
    const device::mojom::blink::HidReportItem& report_item) {
  HIDReportItem* result = HIDReportItem::Create();
  result->setIsAbsolute(!report_item.is_relative);
  result->setIsArray(!report_item.is_variable);
  result->setIsBufferedBytes(report_item.is_buffered_bytes);
  result->setIsConstant(report_item.is_constant);
  result->setIsLinear(!report_item.is_non_linear);
  result->setIsRange(report_item.is_range);
  result->setIsVolatile(report_item.is_volatile);
  result->setHasNull(report_item.has_null_position);
  result->setHasPreferredState(!report_item.no_preferred_state);
  result->setWrap(report_item.wrap);
  result->setReportSize(report_item.report_size);
  result->setReportCount(report_item.report_count);
  result->setUnitExponent(
      UnitFactorExponentToInt(report_item.unit_exponent & 0x0f));
  result->setLogicalMinimum(report_item.logical_minimum);
  result->setLogicalMaximum(report_item.logical_maximum);
  result->setPhysicalMinimum(report_item.physical_minimum);
  result->setPhysicalMaximum(report_item.physical_maximum);

  if (report_item.is_range) {
    result->setUsageMinimum(
        ConvertHidUsageAndPageToUint32(*report_item.usage_minimum));
    result->setUsageMaximum(
        ConvertHidUsageAndPageToUint32(*report_item.usage_maximum));
  } else {
    Vector<uint32_t> usages;
    for (const auto& usage : report_item.usages)
      usages.push_back(ConvertHidUsageAndPageToUint32(*usage));
    result->setUsages(usages);
  }

  String unit_system;
  int8_t unit_factor_length_exponent;
  int8_t unit_factor_mass_exponent;
  int8_t unit_factor_time_exponent;
  int8_t unit_factor_temperature_exponent;
  int8_t unit_factor_current_exponent;
  int8_t unit_factor_luminous_intensity_exponent;
  UnpackUnitValues(report_item.unit, unit_system, unit_factor_length_exponent,
                   unit_factor_mass_exponent, unit_factor_time_exponent,
                   unit_factor_temperature_exponent,
                   unit_factor_current_exponent,
                   unit_factor_luminous_intensity_exponent);
  result->setUnitSystem(unit_system);
  result->setUnitFactorLengthExponent(unit_factor_length_exponent);
  result->setUnitFactorMassExponent(unit_factor_mass_exponent);
  result->setUnitFactorTimeExponent(unit_factor_time_exponent);
  result->setUnitFactorTemperatureExponent(unit_factor_temperature_exponent);
  result->setUnitFactorCurrentExponent(unit_factor_current_exponent);
  result->setUnitFactorLuminousIntensityExponent(
      unit_factor_luminous_intensity_exponent);

  // TODO(mattreynolds): Set |strings_|.

  return result;
}

}  // namespace blink
