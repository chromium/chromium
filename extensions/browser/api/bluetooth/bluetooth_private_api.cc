// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_private_api.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "extensions/browser/api/bluetooth/bluetooth_api.h"
#include "extensions/browser/api/bluetooth/bluetooth_api_pairing_delegate.h"
#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"
#include "extensions/common/api/bluetooth.h"
#include "extensions/common/api/bluetooth_private.h"

#if defined(OS_CHROMEOS)
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#endif

namespace bt = extensions::api::bluetooth;
namespace bt_private = extensions::api::bluetooth_private;
namespace SetDiscoveryFilter = bt_private::SetDiscoveryFilter;

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

namespace {

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum BluetoothTransportType {
  kUnknown = 0,
  kClassic = 1,
  kLE = 2,
  kDual = 3,
  kInvalid = 4,
  kMaxValue
};

std::string GetListenerId(const EventListenerInfo& details) {
  return !details.extension_id.empty() ? details.extension_id
                                       : details.listener_url.host();
}

void RecordPairingDuration(const std::string& histogram_name,
                           base::TimeDelta pairing_duration) {
  base::UmaHistogramCustomTimes(histogram_name, pairing_duration,
                                base::TimeDelta::FromMilliseconds(1) /* min */,
                                base::TimeDelta::FromSeconds(30) /* max */,
                                50 /* buckets */);
}

void RecordPairingResult(bool success,
                         bt::Transport transport,
                         int pairing_duration_ms) {
  std::string transport_histogram_name;
  switch (transport) {
    case bt::Transport::TRANSPORT_CLASSIC:
      transport_histogram_name = "Classic";
      break;
    case bt::Transport::TRANSPORT_LE:
      transport_histogram_name = "BLE";
      break;
    case bt::Transport::TRANSPORT_DUAL:
      transport_histogram_name = "Dual";
      break;
    default:
      // A transport type of INVALID or other is unexpected, and no success
      // metric for it exists.
      return;
  }

  base::UmaHistogramBoolean("Bluetooth.ChromeOS.Pairing.Result", success);
  base::UmaHistogramBoolean(
      "Bluetooth.ChromeOS.Pairing.Result." + transport_histogram_name, success);

  std::string duration_histogram_name_prefix =
      "Bluetooth.ChromeOS.Pairing.Duration";
  std::string success_histogram_name = success ? "Success" : "Failure";

  std::string base_histogram_name =
      duration_histogram_name_prefix + "." + success_histogram_name;
  RecordPairingDuration(base_histogram_name,
                        base::TimeDelta::FromMilliseconds(pairing_duration_ms));
  RecordPairingDuration(base_histogram_name + "." + transport_histogram_name,
                        base::TimeDelta::FromMilliseconds(pairing_duration_ms));
}

void RecordPairingTransport(bt::Transport transport) {
  BluetoothTransportType type;
  switch (transport) {
    case bt::Transport::TRANSPORT_CLASSIC:
      type = BluetoothTransportType::kClassic;
      break;
    case bt::Transport::TRANSPORT_LE:
      type = BluetoothTransportType::kLE;
      break;
    case bt::Transport::TRANSPORT_DUAL:
      type = BluetoothTransportType::kDual;
      break;
    case bt::Transport::TRANSPORT_INVALID:
      type = BluetoothTransportType::kInvalid;
      break;
    default:
      type = BluetoothTransportType::kUnknown;
      break;
  }

  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.Pairing.TransportType",
                                type);
}

}  // namespace

// static
BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>*
BluetoothPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

BluetoothPrivateAPI::BluetoothPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter::Get(browser_context_)
      ->RegisterObserver(this, bt_private::OnPairing::kEventName);
}

BluetoothPrivateAPI::~BluetoothPrivateAPI() {}

void BluetoothPrivateAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void BluetoothPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  // This function can be called multiple times for the same JS listener, for
  // example, once for the addListener call and again if it is a lazy listener.
  if (!details.browser_context)
    return;

  BluetoothAPI::Get(browser_context_)
      ->event_router()
      ->AddPairingDelegate(GetListenerId(details));
}

void BluetoothPrivateAPI::OnListenerRemoved(const EventListenerInfo& details) {
  // This function can be called multiple times for the same JS listener, for
  // example, once for the addListener call and again if it is a lazy listener.
  if (!details.browser_context)
    return;

  BluetoothAPI::Get(browser_context_)
      ->event_router()
      ->RemovePairingDelegate(GetListenerId(details));
}

namespace api {

namespace {

const char kNameProperty[] = "name";
const char kPoweredProperty[] = "powered";
const char kDiscoverableProperty[] = "discoverable";
const char kSetAdapterPropertyError[] = "Error setting adapter properties: $1";
const char kDeviceNotFoundError[] = "Invalid Bluetooth device";
const char kDeviceNotConnectedError[] = "Device not connected";
const char kPairingNotEnabled[] = "Pairing not enabled";
const char kInvalidPairingResponseOptions[] =
    "Invalid pairing response options";
const char kAdapterNotPresent[] = "Failed to find a Bluetooth adapter";
const char kDisconnectError[] = "Failed to disconnect device";
const char kForgetDeviceError[] = "Failed to forget device";
const char kSetDiscoveryFilterFailed[] = "Failed to set discovery filter";
const char kPairingFailed[] = "Pairing failed";

// Returns true if the pairing response options passed into the
// setPairingResponse function are valid.
bool ValidatePairingResponseOptions(
    const device::BluetoothDevice* device,
    const bt_private::SetPairingResponseOptions& options) {
  bool response = options.response != bt_private::PAIRING_RESPONSE_NONE;
  bool pincode = options.pincode != nullptr;
  bool passkey = options.passkey != nullptr;

  if (!response && !pincode && !passkey)
    return false;
  if (pincode && passkey)
    return false;
  if (options.response != bt_private::PAIRING_RESPONSE_CONFIRM &&
      (pincode || passkey))
    return false;

  if (options.response == bt_private::PAIRING_RESPONSE_CANCEL)
    return true;

  // Check the BluetoothDevice is in expecting the correct response.
  if (!device->ExpectingConfirmation() && !device->ExpectingPinCode() &&
      !device->ExpectingPasskey())
    return false;
  if (pincode && !device->ExpectingPinCode())
    return false;
  if (passkey && !device->ExpectingPasskey())
    return false;
  if (options.response == bt_private::PAIRING_RESPONSE_CONFIRM && !pincode &&
      !passkey && !device->ExpectingConfirmation())
    return false;

  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateSetAdapterStateFunction::
    BluetoothPrivateSetAdapterStateFunction() {}

BluetoothPrivateSetAdapterStateFunction::
    ~BluetoothPrivateSetAdapterStateFunction() {}

bool BluetoothPrivateSetAdapterStateFunction::CreateParams() {
  params_ = bt_private::SetAdapterState::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivateSetAdapterStateFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter->IsPresent()) {
    Respond(Error(kAdapterNotPresent));
    return;
  }

  const bt_private::NewAdapterState& new_state = params_->adapter_state;

  // These properties are not owned.
  std::string* name = new_state.name.get();
  bool* powered = new_state.powered.get();
  bool* discoverable = new_state.discoverable.get();

  if (name && adapter->GetName() != *name) {
    BLUETOOTH_LOG(USER) << "SetAdapterState: name=" << *name;
    pending_properties_.insert(kNameProperty);
    adapter->SetName(*name, CreatePropertySetCallback(kNameProperty),
                     CreatePropertyErrorCallback(kNameProperty));
  }

  if (powered && adapter->IsPowered() != *powered) {
    BLUETOOTH_LOG(USER) << "SetAdapterState: powerd=" << *powered;
    pending_properties_.insert(kPoweredProperty);
    adapter->SetPowered(*powered, CreatePropertySetCallback(kPoweredProperty),
                        CreatePropertyErrorCallback(kPoweredProperty));
  }

  if (discoverable && adapter->IsDiscoverable() != *discoverable) {
    BLUETOOTH_LOG(USER) << "SetAdapterState: discoverable=" << *discoverable;
    pending_properties_.insert(kDiscoverableProperty);
    adapter->SetDiscoverable(
        *discoverable, CreatePropertySetCallback(kDiscoverableProperty),
        CreatePropertyErrorCallback(kDiscoverableProperty));
  }

  parsed_ = true;

  if (pending_properties_.empty())
    Respond(NoArguments());
}

base::Closure
BluetoothPrivateSetAdapterStateFunction::CreatePropertySetCallback(
    const std::string& property_name) {
  BLUETOOTH_LOG(DEBUG) << "Set property succeeded: " << property_name;
  return base::Bind(
      &BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertySet, this,
      property_name);
}

base::Closure
BluetoothPrivateSetAdapterStateFunction::CreatePropertyErrorCallback(
    const std::string& property_name) {
  BLUETOOTH_LOG(DEBUG) << "Set property failed: " << property_name;
  return base::Bind(
      &BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertyError, this,
      property_name);
}

void BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertySet(
    const std::string& property) {
  DCHECK(pending_properties_.find(property) != pending_properties_.end());
  DCHECK(failed_properties_.find(property) == failed_properties_.end());

  pending_properties_.erase(property);
  if (pending_properties_.empty() && parsed_) {
    if (failed_properties_.empty())
      Respond(NoArguments());
    else
      SendError();
  }
}

void BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertyError(
    const std::string& property) {
  DCHECK(pending_properties_.find(property) != pending_properties_.end());
  DCHECK(failed_properties_.find(property) == failed_properties_.end());
  pending_properties_.erase(property);
  failed_properties_.insert(property);
  if (pending_properties_.empty() && parsed_)
    SendError();
}

void BluetoothPrivateSetAdapterStateFunction::SendError() {
  DCHECK(pending_properties_.empty());
  DCHECK(!failed_properties_.empty());

  std::vector<base::StringPiece> failed_vector(failed_properties_.begin(),
                                               failed_properties_.end());

  std::vector<std::string> replacements(1);
  replacements[0] = base::JoinString(failed_vector, ", ");
  std::string error = base::ReplaceStringPlaceholders(kSetAdapterPropertyError,
                                                      replacements, nullptr);
  Respond(Error(error));
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateSetPairingResponseFunction::
    BluetoothPrivateSetPairingResponseFunction() {}

BluetoothPrivateSetPairingResponseFunction::
    ~BluetoothPrivateSetPairingResponseFunction() {}

bool BluetoothPrivateSetPairingResponseFunction::CreateParams() {
  params_ = bt_private::SetPairingResponse::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivateSetPairingResponseFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  const bt_private::SetPairingResponseOptions& options = params_->options;

  BluetoothEventRouter* router =
      BluetoothAPI::Get(browser_context())->event_router();
  if (!router->GetPairingDelegate(GetExtensionId())) {
    Respond(Error(kPairingNotEnabled));
    return;
  }

  const std::string& device_address = options.device.address;
  device::BluetoothDevice* device = adapter->GetDevice(device_address);
  if (!device) {
    Respond(Error(kDeviceNotFoundError));
    return;
  }

  if (!ValidatePairingResponseOptions(device, options)) {
    Respond(Error(kInvalidPairingResponseOptions));
    return;
  }

  if (options.pincode.get()) {
    device->SetPinCode(*options.pincode);
  } else if (options.passkey.get()) {
    device->SetPasskey(*options.passkey);
  } else {
    switch (options.response) {
      case bt_private::PAIRING_RESPONSE_CONFIRM:
        device->ConfirmPairing();
        break;
      case bt_private::PAIRING_RESPONSE_REJECT:
        device->RejectPairing();
        break;
      case bt_private::PAIRING_RESPONSE_CANCEL:
        device->CancelPairing();
        break;
      default:
        NOTREACHED();
    }
  }

  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateDisconnectAllFunction::BluetoothPrivateDisconnectAllFunction() {
}

BluetoothPrivateDisconnectAllFunction::
    ~BluetoothPrivateDisconnectAllFunction() {}

bool BluetoothPrivateDisconnectAllFunction::CreateParams() {
  params_ = bt_private::DisconnectAll::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivateDisconnectAllFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  device::BluetoothDevice* device = adapter->GetDevice(params_->device_address);
  if (!device) {
    Respond(Error(kDeviceNotFoundError));
    return;
  }

  if (!device->IsConnected()) {
    Respond(Error(kDeviceNotConnectedError));
    return;
  }

  device->Disconnect(
      base::Bind(&BluetoothPrivateDisconnectAllFunction::OnSuccessCallback,
                 this),
      base::Bind(&BluetoothPrivateDisconnectAllFunction::OnErrorCallback, this,
                 adapter, params_->device_address));
}

void BluetoothPrivateDisconnectAllFunction::OnSuccessCallback() {
  Respond(NoArguments());
}

void BluetoothPrivateDisconnectAllFunction::OnErrorCallback(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address) {
  // The call to Disconnect may report an error if the device was disconnected
  // due to an external reason. In this case, report "Not Connected" as the
  // error.
  device::BluetoothDevice* device = adapter->GetDevice(device_address);
  if (device && !device->IsConnected())
    Respond(Error(kDeviceNotConnectedError));
  else
    Respond(Error(kDisconnectError));
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateForgetDeviceFunction::BluetoothPrivateForgetDeviceFunction() {}

BluetoothPrivateForgetDeviceFunction::~BluetoothPrivateForgetDeviceFunction() {}

bool BluetoothPrivateForgetDeviceFunction::CreateParams() {
  params_ = bt_private::ForgetDevice::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivateForgetDeviceFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  device::BluetoothDevice* device = adapter->GetDevice(params_->device_address);
  if (!device) {
    Respond(Error(kDeviceNotFoundError));
    return;
  }

  device->Forget(
      base::Bind(&BluetoothPrivateForgetDeviceFunction::OnSuccessCallback,
                 this),
      base::Bind(&BluetoothPrivateForgetDeviceFunction::OnErrorCallback, this,
                 adapter, params_->device_address));
}

void BluetoothPrivateForgetDeviceFunction::OnSuccessCallback() {
  Respond(NoArguments());
}

void BluetoothPrivateForgetDeviceFunction::OnErrorCallback(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address) {
  Respond(Error(kForgetDeviceError));
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateSetDiscoveryFilterFunction::
    BluetoothPrivateSetDiscoveryFilterFunction() = default;
BluetoothPrivateSetDiscoveryFilterFunction::
    ~BluetoothPrivateSetDiscoveryFilterFunction() = default;

bool BluetoothPrivateSetDiscoveryFilterFunction::CreateParams() {
  params_ = SetDiscoveryFilter::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivateSetDiscoveryFilterFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  auto& df_param = params_->discovery_filter;

  std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter;

  // If all filter fields are empty, we are clearing filter. If any field is
  // set, then create proper filter.
  if (df_param.uuids.get() || df_param.rssi.get() || df_param.pathloss.get() ||
      df_param.transport != bt_private::TransportType::TRANSPORT_TYPE_NONE) {
    device::BluetoothTransport transport;

    switch (df_param.transport) {
      case bt_private::TransportType::TRANSPORT_TYPE_LE:
        transport = device::BLUETOOTH_TRANSPORT_LE;
        break;
      case bt_private::TransportType::TRANSPORT_TYPE_BREDR:
        transport = device::BLUETOOTH_TRANSPORT_CLASSIC;
        break;
      default:  // TRANSPORT_TYPE_NONE is included here
        transport = device::BLUETOOTH_TRANSPORT_DUAL;
        break;
    }

    discovery_filter.reset(new device::BluetoothDiscoveryFilter(transport));

    if (df_param.uuids.get()) {
      if (df_param.uuids->as_string.get()) {
        device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
        device_filter.uuids.insert(
            device::BluetoothUUID(*df_param.uuids->as_string));
        discovery_filter->AddDeviceFilter(std::move(device_filter));
      } else if (df_param.uuids->as_strings.get()) {
        for (const auto& iter : *df_param.uuids->as_strings) {
          device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
          device_filter.uuids.insert(device::BluetoothUUID(iter));
          discovery_filter->AddDeviceFilter(std::move(device_filter));
        }
      }
    }

    if (df_param.rssi.get())
      discovery_filter->SetRSSI(*df_param.rssi);

    if (df_param.pathloss.get())
      discovery_filter->SetPathloss(*df_param.pathloss);
  }

  BluetoothAPI::Get(browser_context())
      ->event_router()
      ->SetDiscoveryFilter(
          std::move(discovery_filter), adapter.get(), GetExtensionId(),
          base::Bind(
              &BluetoothPrivateSetDiscoveryFilterFunction::OnSuccessCallback,
              this),
          base::Bind(
              &BluetoothPrivateSetDiscoveryFilterFunction::OnErrorCallback,
              this));
}

void BluetoothPrivateSetDiscoveryFilterFunction::OnSuccessCallback() {
  Respond(NoArguments());
}

void BluetoothPrivateSetDiscoveryFilterFunction::OnErrorCallback() {
  Respond(Error(kSetDiscoveryFilterFailed));
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateConnectFunction::BluetoothPrivateConnectFunction() {}

BluetoothPrivateConnectFunction::~BluetoothPrivateConnectFunction() {}

bool BluetoothPrivateConnectFunction::CreateParams() {
  params_ = bt_private::Connect::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivateConnectFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  device::BluetoothDevice* device = adapter->GetDevice(params_->device_address);
  if (!device) {
    Respond(Error(kDeviceNotFoundError));
    return;
  }

  if (device->IsConnected()) {
    Respond(ArgumentList(bt_private::Connect::Results::Create(
        bt_private::CONNECT_RESULT_TYPE_ALREADYCONNECTED)));
    return;
  }

  // pairing_delegate may be null for connect.
  device::BluetoothDevice::PairingDelegate* pairing_delegate =
      BluetoothAPI::Get(browser_context())
          ->event_router()
          ->GetPairingDelegate(GetExtensionId());
  device->Connect(
      pairing_delegate,
      base::Bind(&BluetoothPrivateConnectFunction::OnSuccessCallback, this),
      base::Bind(&BluetoothPrivateConnectFunction::OnErrorCallback, this));
}

void BluetoothPrivateConnectFunction::OnSuccessCallback() {
  Respond(ArgumentList(bt_private::Connect::Results::Create(
      bt_private::CONNECT_RESULT_TYPE_SUCCESS)));
}

void BluetoothPrivateConnectFunction::OnErrorCallback(
    device::BluetoothDevice::ConnectErrorCode error) {
  bt_private::ConnectResultType result = bt_private::CONNECT_RESULT_TYPE_NONE;
  switch (error) {
    case device::BluetoothDevice::ERROR_AUTH_CANCELED:
      result = bt_private::CONNECT_RESULT_TYPE_AUTHCANCELED;
      break;
    case device::BluetoothDevice::ERROR_AUTH_FAILED:
      result = bt_private::CONNECT_RESULT_TYPE_AUTHFAILED;
      break;
    case device::BluetoothDevice::ERROR_AUTH_REJECTED:
      result = bt_private::CONNECT_RESULT_TYPE_AUTHREJECTED;
      break;
    case device::BluetoothDevice::ERROR_AUTH_TIMEOUT:
      result = bt_private::CONNECT_RESULT_TYPE_AUTHTIMEOUT;
      break;
    case device::BluetoothDevice::ERROR_FAILED:
      result = bt_private::CONNECT_RESULT_TYPE_FAILED;
      break;
    case device::BluetoothDevice::ERROR_INPROGRESS:
      result = bt_private::CONNECT_RESULT_TYPE_INPROGRESS;
      break;
    case device::BluetoothDevice::ERROR_UNKNOWN:
      result = bt_private::CONNECT_RESULT_TYPE_UNKNOWNERROR;
      break;
    case device::BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      result = bt_private::CONNECT_RESULT_TYPE_UNSUPPORTEDDEVICE;
      break;
    case device::BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED();
      break;
  }
  // Set the result type and respond with true (success).
  Respond(ArgumentList(bt_private::Connect::Results::Create(result)));
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivatePairFunction::BluetoothPrivatePairFunction() {}

BluetoothPrivatePairFunction::~BluetoothPrivatePairFunction() {}
bool BluetoothPrivatePairFunction::CreateParams() {
  params_ = bt_private::Pair::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivatePairFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  device::BluetoothDevice* device = adapter->GetDevice(params_->device_address);
  if (!device) {
    Respond(Error(kDeviceNotFoundError));
    return;
  }

  device::BluetoothDevice::PairingDelegate* pairing_delegate =
      BluetoothAPI::Get(browser_context())
          ->event_router()
          ->GetPairingDelegate(GetExtensionId());

  // pairing_delegate must be set (by adding an onPairing listener) before
  // any calls to pair().
  if (!pairing_delegate) {
    Respond(Error(kPairingNotEnabled));
    return;
  }

  device->Pair(
      pairing_delegate,
      base::Bind(&BluetoothPrivatePairFunction::OnSuccessCallback, this),
      base::Bind(&BluetoothPrivatePairFunction::OnErrorCallback, this));
}

void BluetoothPrivatePairFunction::OnSuccessCallback() {
  Respond(NoArguments());
}

void BluetoothPrivatePairFunction::OnErrorCallback(
    device::BluetoothDevice::ConnectErrorCode error) {
  Respond(Error(kPairingFailed));
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateRecordPairingFunction::BluetoothPrivateRecordPairingFunction() =
    default;

BluetoothPrivateRecordPairingFunction::
    ~BluetoothPrivateRecordPairingFunction() = default;

bool BluetoothPrivateRecordPairingFunction::CreateParams() {
  params_ = bt_private::RecordPairing::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivateRecordPairingFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  RecordPairingResult(params_->success, params_->transport,
                      params_->pairing_duration_ms);
  RecordPairingTransport(params_->transport);

  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateRecordReconnectionFunction::
    BluetoothPrivateRecordReconnectionFunction() = default;

BluetoothPrivateRecordReconnectionFunction::
    ~BluetoothPrivateRecordReconnectionFunction() = default;

bool BluetoothPrivateRecordReconnectionFunction::CreateParams() {
  params_ = bt_private::RecordReconnection::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivateRecordReconnectionFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  base::UmaHistogramBoolean(
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result",
      params_->success);
  base::UmaHistogramBoolean(
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result.Settings",
      params_->success);

  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateRecordDeviceSelectionFunction::
    BluetoothPrivateRecordDeviceSelectionFunction() = default;

BluetoothPrivateRecordDeviceSelectionFunction::
    ~BluetoothPrivateRecordDeviceSelectionFunction() = default;

bool BluetoothPrivateRecordDeviceSelectionFunction::CreateParams() {
  params_ = bt_private::RecordDeviceSelection::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothPrivateRecordDeviceSelectionFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
#if defined(OS_CHROMEOS)
  device::BluetoothTransport transport;
  switch (params_->transport) {
    case bt::Transport::TRANSPORT_CLASSIC:
      transport = device::BLUETOOTH_TRANSPORT_CLASSIC;
      break;
    case bt::Transport::TRANSPORT_LE:
      transport = device::BLUETOOTH_TRANSPORT_LE;
      break;
    case bt::Transport::TRANSPORT_DUAL:
      transport = device::BLUETOOTH_TRANSPORT_DUAL;
      break;
    default:
      transport = device::BLUETOOTH_TRANSPORT_INVALID;
      break;
  }

  device::RecordDeviceSelectionDuration(
      base::TimeDelta::FromMilliseconds(params_->selection_duration_ms),
      device::BluetoothUiSurface::kSettings, params_->was_paired, transport);
#endif

  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////

}  // namespace api

}  // namespace extensions
