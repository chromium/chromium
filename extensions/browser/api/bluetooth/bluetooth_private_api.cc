// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_private_api.h"

#include <stdint.h>

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "extensions/browser/api/bluetooth/bluetooth_api.h"
#include "extensions/browser/api/bluetooth/bluetooth_api_pairing_delegate.h"
#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"
#include "extensions/common/api/bluetooth.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace bt = extensions::api::bluetooth;
namespace bt_private = extensions::api::bluetooth_private;
namespace SetDiscoveryFilter = bt_private::SetDiscoveryFilter;

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

namespace {

#if BUILDFLAG(IS_CHROMEOS)
device::BluetoothTransport GetBluetoothTransport(bt::Transport transport) {
  switch (transport) {
    case bt::Transport::kClassic:
      return device::BLUETOOTH_TRANSPORT_CLASSIC;
    case bt::Transport::kLe:
      return device::BLUETOOTH_TRANSPORT_LE;
    case bt::Transport::kDual:
      return device::BLUETOOTH_TRANSPORT_DUAL;
    default:
      return device::BLUETOOTH_TRANSPORT_INVALID;
  }
}

bool IsActualConnectionFailure(bt_private::ConnectResultType result) {
  DCHECK(result != bt_private::ConnectResultType::kSuccess);

  switch (result) {
    case bt_private::ConnectResultType::kInProgress:
    case bt_private::ConnectResultType::kAuthCanceled:
    case bt_private::ConnectResultType::kAuthRejected:
      // The connection is not a failure if it's still in progress, the user
      // canceled auth, or the user entered incorrect auth details.
      return false;
    default:
      return true;
  }
}

std::optional<device::ConnectionFailureReason> GetConnectionFailureReason(
    bt_private::ConnectResultType result) {
  DCHECK(IsActualConnectionFailure(result));

  switch (result) {
    case bt_private::ConnectResultType::kNone:
      return device::ConnectionFailureReason::kSystemError;
    case bt_private::ConnectResultType::kAuthFailed:
      return device::ConnectionFailureReason::kAuthFailed;
    case bt_private::ConnectResultType::kAuthTimeout:
      return device::ConnectionFailureReason::kAuthTimeout;
    case bt_private::ConnectResultType::kFailed:
      return device::ConnectionFailureReason::kFailed;
    case bt_private::ConnectResultType::kUnknownError:
      return device::ConnectionFailureReason::kUnknownConnectionError;
    case bt_private::ConnectResultType::kUnsupportedDevice:
      return device::ConnectionFailureReason::kUnsupportedDevice;
    case bt_private::ConnectResultType::kNotReady:
      return device::ConnectionFailureReason::kDeviceNotReady;
    case bt_private::ConnectResultType::kAlreadyExists:
      return device::ConnectionFailureReason::kDeviceAlreadyExists;
    case bt_private::ConnectResultType::kNotConnected:
      return device::ConnectionFailureReason::kNotConnectable;
    case bt_private::ConnectResultType::kDoesNotExist:
      return device::ConnectionFailureReason::kNotFound;
    case bt_private::ConnectResultType::kInvalidArgs:
      return device::ConnectionFailureReason::kInvalidArgs;
    case bt_private::ConnectResultType::kNonAuthTimeout:
      return device::ConnectionFailureReason::kNonAuthTimeout;
    case bt_private::ConnectResultType::kNoMemory:
      return device::ConnectionFailureReason::kNoMemory;
    case bt_private::ConnectResultType::kJniEnvironment:
      return device::ConnectionFailureReason::kJniEnvironment;
    case bt_private::ConnectResultType::kJniThreadAttach:
      return device::ConnectionFailureReason::kJniThreadAttach;
    case bt_private::ConnectResultType::kWakelock:
      return device::ConnectionFailureReason::kWakelock;
    case bt_private::ConnectResultType::kAlreadyConnected:
      return device::ConnectionFailureReason::kAlreadyConnected;
    case bt_private::ConnectResultType::kUnexpectedState:
      return device::ConnectionFailureReason::kUnexpectedState;
    case bt_private::ConnectResultType::kSocketError:
      return device::ConnectionFailureReason::kSocketError;
    case bt_private::ConnectResultType::kInProgress:
      [[fallthrough]];
    case bt_private::ConnectResultType::kAuthRejected:
      [[fallthrough]];
    case bt_private::ConnectResultType::kAuthCanceled:
      [[fallthrough]];
    case bt_private::ConnectResultType::kSuccess:
      NOTREACHED();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::string GetListenerId(const EventListenerInfo& details) {
  return !details.extension_id.empty() ? details.extension_id
                                       : details.listener_url.host();
}

bt_private::ConnectResultType DeviceConnectErrorToConnectResult(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ERROR_AUTH_CANCELED:
      return bt_private::ConnectResultType::kAuthCanceled;
    case device::BluetoothDevice::ERROR_AUTH_FAILED:
      return bt_private::ConnectResultType::kAuthFailed;
    case device::BluetoothDevice::ERROR_AUTH_REJECTED:
      return bt_private::ConnectResultType::kAuthRejected;
    case device::BluetoothDevice::ERROR_AUTH_TIMEOUT:
      return bt_private::ConnectResultType::kAuthTimeout;
    case device::BluetoothDevice::ERROR_FAILED:
      return bt_private::ConnectResultType::kFailed;
    case device::BluetoothDevice::ERROR_INPROGRESS:
      return bt_private::ConnectResultType::kInProgress;
    case device::BluetoothDevice::ERROR_UNKNOWN:
      return bt_private::ConnectResultType::kUnknownError;
    case device::BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      return bt_private::ConnectResultType::kUnsupportedDevice;
    case device::BluetoothDevice::ERROR_DEVICE_NOT_READY:
      return bt_private::ConnectResultType::kNotReady;
    case device::BluetoothDevice::ERROR_ALREADY_CONNECTED:
      return bt_private::ConnectResultType::kAlreadyConnected;
    case device::BluetoothDevice::ERROR_DEVICE_ALREADY_EXISTS:
      return bt_private::ConnectResultType::kAlreadyExists;
    case device::BluetoothDevice::ERROR_DEVICE_UNCONNECTED:
      return bt_private::ConnectResultType::kNotConnected;
    case device::BluetoothDevice::ERROR_DOES_NOT_EXIST:
      return bt_private::ConnectResultType::kDoesNotExist;
    case device::BluetoothDevice::ERROR_INVALID_ARGS:
      return bt_private::ConnectResultType::kInvalidArgs;
    case device::BluetoothDevice::ERROR_NON_AUTH_TIMEOUT:
      return bt_private::ConnectResultType::kNonAuthTimeout;
    case device::BluetoothDevice::ERROR_NO_MEMORY:
      return bt_private::ConnectResultType::kNoMemory;
    case device::BluetoothDevice::ERROR_JNI_ENVIRONMENT:
      return bt_private::ConnectResultType::kJniEnvironment;
    case device::BluetoothDevice::ERROR_JNI_THREAD_ATTACH:
      return bt_private::ConnectResultType::kJniThreadAttach;
    case device::BluetoothDevice::ERROR_WAKELOCK:
      return bt_private::ConnectResultType::kWakelock;
    case device::BluetoothDevice::ERROR_UNEXPECTED_STATE:
      return bt_private::ConnectResultType::kUnexpectedState;
    case device::BluetoothDevice::ERROR_SOCKET:
      return bt_private::ConnectResultType::kSocketError;
    case device::BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return bt_private::ConnectResultType::kNone;
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

BluetoothPrivateAPI::~BluetoothPrivateAPI() = default;

void BluetoothPrivateAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void BluetoothPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  // This function can be called multiple times for the same JS listener, for
  // example, once for the addListener call and again if it is a lazy listener.
  if (details.is_lazy)
    return;

  BluetoothAPI::Get(browser_context_)
      ->event_router()
      ->AddPairingDelegate(GetListenerId(details));
}

void BluetoothPrivateAPI::OnListenerRemoved(const EventListenerInfo& details) {
  // This function can be called multiple times for the same JS listener, for
  // example, once for the addListener call and again if it is a lazy listener.
  if (details.is_lazy)
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
  bool response = options.response != bt_private::PairingResponse::kNone;
  bool pincode = options.pincode.has_value();
  bool passkey = options.passkey.has_value();

  if (!response && !pincode && !passkey)
    return false;
  if (pincode && passkey)
    return false;
  if (options.response != bt_private::PairingResponse::kConfirm &&
      (pincode || passkey)) {
    return false;
  }

  if (options.response == bt_private::PairingResponse::kCancel) {
    return true;
  }

  // Check the BluetoothDevice is in expecting the correct response.
  if (!device->ExpectingConfirmation() && !device->ExpectingPinCode() &&
      !device->ExpectingPasskey())
    return false;
  if (pincode && !device->ExpectingPinCode())
    return false;
  if (passkey && !device->ExpectingPasskey())
    return false;
  if (options.response == bt_private::PairingResponse::kConfirm && !pincode &&
      !passkey && !device->ExpectingConfirmation()) {
    return false;
  }

  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateSetAdapterStateFunction::
    BluetoothPrivateSetAdapterStateFunction() {}

BluetoothPrivateSetAdapterStateFunction::
    ~BluetoothPrivateSetAdapterStateFunction() {}

bool BluetoothPrivateSetAdapterStateFunction::CreateParams() {
  params_ = bt_private::SetAdapterState::Params::Create(args());
  return params_.has_value();
}

void BluetoothPrivateSetAdapterStateFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter->IsPresent()) {
    Respond(Error(kAdapterNotPresent));
    return;
  }

  const bt_private::NewAdapterState& new_state = params_->adapter_state;

  // These properties are not owned.
  const auto& name = new_state.name;
  const auto& powered = new_state.powered;
  const auto& discoverable = new_state.discoverable;

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

base::OnceClosure
BluetoothPrivateSetAdapterStateFunction::CreatePropertySetCallback(
    const std::string& property_name) {
  BLUETOOTH_LOG(DEBUG) << "Set property succeeded: " << property_name;
  return base::BindOnce(
      &BluetoothPrivateSetAdapterStateFunction::OnAdapterPropertySet, this,
      property_name);
}

base::OnceClosure
BluetoothPrivateSetAdapterStateFunction::CreatePropertyErrorCallback(
    const std::string& property_name) {
  BLUETOOTH_LOG(DEBUG) << "Set property failed: " << property_name;
  return base::BindOnce(
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

  std::vector<std::string_view> failed_vector(failed_properties_.begin(),
                                              failed_properties_.end());

  std::vector<std::string> replacements(1);
  replacements[0] = base::JoinString(failed_vector, ", ");
  std::string error = base::ReplaceStringPlaceholders(kSetAdapterPropertyError,
                                                      replacements, nullptr);
  Respond(Error(std::move(error)));
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateSetPairingResponseFunction::
    BluetoothPrivateSetPairingResponseFunction() {}

BluetoothPrivateSetPairingResponseFunction::
    ~BluetoothPrivateSetPairingResponseFunction() {}

bool BluetoothPrivateSetPairingResponseFunction::CreateParams() {
  params_ = bt_private::SetPairingResponse::Params::Create(args());
  return params_.has_value();
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

  if (options.pincode) {
    device->SetPinCode(*options.pincode);
  } else if (options.passkey) {
    device->SetPasskey(*options.passkey);
  } else {
    switch (options.response) {
      case bt_private::PairingResponse::kConfirm:
        device->ConfirmPairing();
        break;
      case bt_private::PairingResponse::kReject:
        device->RejectPairing();
        break;
      case bt_private::PairingResponse::kCancel:
        device->CancelPairing();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
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
  params_ = bt_private::DisconnectAll::Params::Create(args());
  return params_.has_value();
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
      base::BindOnce(&BluetoothPrivateDisconnectAllFunction::OnSuccessCallback,
                     this),
      base::BindOnce(&BluetoothPrivateDisconnectAllFunction::OnErrorCallback,
                     this, adapter, params_->device_address));
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

BluetoothPrivateForgetDeviceFunction::BluetoothPrivateForgetDeviceFunction() =
    default;

BluetoothPrivateForgetDeviceFunction::~BluetoothPrivateForgetDeviceFunction() =
    default;

bool BluetoothPrivateForgetDeviceFunction::CreateParams() {
  params_ = bt_private::ForgetDevice::Params::Create(args());
  return params_.has_value();
}

void BluetoothPrivateForgetDeviceFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  device::BluetoothDevice* device = adapter->GetDevice(params_->device_address);
  if (!device) {
    Respond(Error(kDeviceNotFoundError));
    return;
  }

  device->Forget(
      base::BindOnce(&BluetoothPrivateForgetDeviceFunction::OnSuccessCallback,
                     this),
      base::BindOnce(&BluetoothPrivateForgetDeviceFunction::OnErrorCallback,
                     this, adapter, params_->device_address));
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
  params_ = SetDiscoveryFilter::Params::Create(args());
  return params_.has_value();
}

void BluetoothPrivateSetDiscoveryFilterFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  auto& df_param = params_->discovery_filter;

  std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter;

  // If all filter fields are empty, we are clearing filter. If any field is
  // set, then create proper filter.
  if (df_param.uuids || df_param.rssi || df_param.pathloss ||
      df_param.transport != bt_private::TransportType::kNone) {
    device::BluetoothTransport transport;

    switch (df_param.transport) {
      case bt_private::TransportType::kLe:
        transport = device::BLUETOOTH_TRANSPORT_LE;
        break;
      case bt_private::TransportType::kBredr:
        transport = device::BLUETOOTH_TRANSPORT_CLASSIC;
        break;
      default:  // TRANSPORT_TYPE_NONE is included here
        transport = device::BLUETOOTH_TRANSPORT_DUAL;
        break;
    }

    discovery_filter =
        std::make_unique<device::BluetoothDiscoveryFilter>(transport);

    if (df_param.uuids) {
      if (df_param.uuids->as_string) {
        device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
        device_filter.uuids.insert(
            device::BluetoothUUID(*df_param.uuids->as_string));
        discovery_filter->AddDeviceFilter(std::move(device_filter));
      } else if (df_param.uuids->as_strings) {
        for (const auto& iter : *df_param.uuids->as_strings) {
          device::BluetoothDiscoveryFilter::DeviceInfoFilter device_filter;
          device_filter.uuids.insert(device::BluetoothUUID(iter));
          discovery_filter->AddDeviceFilter(std::move(device_filter));
        }
      }
    }

    if (df_param.rssi)
      discovery_filter->SetRSSI(*df_param.rssi);

    if (df_param.pathloss)
      discovery_filter->SetPathloss(*df_param.pathloss);
  }

  BluetoothAPI::Get(browser_context())
      ->event_router()
      ->SetDiscoveryFilter(
          std::move(discovery_filter), adapter.get(), GetExtensionId(),
          base::BindOnce(
              &BluetoothPrivateSetDiscoveryFilterFunction::OnSuccessCallback,
              this),
          base::BindOnce(
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

BluetoothPrivateConnectFunction::BluetoothPrivateConnectFunction() = default;

BluetoothPrivateConnectFunction::~BluetoothPrivateConnectFunction() = default;

bool BluetoothPrivateConnectFunction::CreateParams() {
  params_ = bt_private::Connect::Params::Create(args());
  return params_.has_value();
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
        bt_private::ConnectResultType::kAlreadyConnected)));
    return;
  }

  // pairing_delegate may be null for connect.
  device::BluetoothDevice::PairingDelegate* pairing_delegate =
      BluetoothAPI::Get(browser_context())
          ->event_router()
          ->GetPairingDelegate(GetExtensionId());
  device->Connect(
      pairing_delegate,
      base::BindOnce(&BluetoothPrivateConnectFunction::OnConnect, this));
}

void BluetoothPrivateConnectFunction::OnConnect(
    std::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code.has_value()) {
    // Set the result type and respond with true (success).
    Respond(ArgumentList(bt_private::Connect::Results::Create(
        DeviceConnectErrorToConnectResult(error_code.value()))));
    return;
  }
  Respond(ArgumentList(bt_private::Connect::Results::Create(
      bt_private::ConnectResultType::kSuccess)));
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivatePairFunction::BluetoothPrivatePairFunction() = default;

BluetoothPrivatePairFunction::~BluetoothPrivatePairFunction() = default;
bool BluetoothPrivatePairFunction::CreateParams() {
  params_ = bt_private::Pair::Params::Create(args());
  return params_.has_value();
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

  device->Pair(pairing_delegate,
               base::BindOnce(&BluetoothPrivatePairFunction::OnPair, this));
}

void BluetoothPrivatePairFunction::OnPair(
    std::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code.has_value()) {
    Respond(Error(kPairingFailed));
    return;
  }
  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateRecordPairingFunction::BluetoothPrivateRecordPairingFunction() =
    default;

BluetoothPrivateRecordPairingFunction::
    ~BluetoothPrivateRecordPairingFunction() = default;

bool BluetoothPrivateRecordPairingFunction::CreateParams() {
  params_ = bt_private::RecordPairing::Params::Create(args());
  return params_.has_value();
}

void BluetoothPrivateRecordPairingFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
#if BUILDFLAG(IS_CHROMEOS)
  bt_private::ConnectResultType result = params_->result;
  bool success = (result == bt_private::ConnectResultType::kSuccess);

  // Only emit metrics if this is a success or a true connection failure.
  if (success || IsActualConnectionFailure(result)) {
    device::RecordPairingResult(
        success ? std::nullopt : GetConnectionFailureReason(result),
        GetBluetoothTransport(params_->transport),
        base::Milliseconds(params_->pairing_duration_ms));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateRecordReconnectionFunction::
    BluetoothPrivateRecordReconnectionFunction() = default;

BluetoothPrivateRecordReconnectionFunction::
    ~BluetoothPrivateRecordReconnectionFunction() = default;

bool BluetoothPrivateRecordReconnectionFunction::CreateParams() {
  params_ = bt_private::RecordReconnection::Params::Create(args());
  return params_.has_value();
}

void BluetoothPrivateRecordReconnectionFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
#if BUILDFLAG(IS_CHROMEOS)
  bt_private::ConnectResultType result = params_->result;
  bool success = (result == bt_private::ConnectResultType::kSuccess);

  // Only emit metrics if this is a success or a true connection failure.
  if (success || IsActualConnectionFailure(result)) {
    device::RecordUserInitiatedReconnectionAttemptResult(
        success ? std::nullopt : GetConnectionFailureReason(result),
        device::UserInitiatedReconnectionUISurfaces::kSettings);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////

BluetoothPrivateRecordDeviceSelectionFunction::
    BluetoothPrivateRecordDeviceSelectionFunction() = default;

BluetoothPrivateRecordDeviceSelectionFunction::
    ~BluetoothPrivateRecordDeviceSelectionFunction() = default;

bool BluetoothPrivateRecordDeviceSelectionFunction::CreateParams() {
  params_ = bt_private::RecordDeviceSelection::Params::Create(args());
  return params_.has_value();
}

void BluetoothPrivateRecordDeviceSelectionFunction::DoWork(
    scoped_refptr<device::BluetoothAdapter> adapter) {
#if BUILDFLAG(IS_CHROMEOS)
  device::RecordDeviceSelectionDuration(
      base::Milliseconds(params_->selection_duration_ms),
      device::DeviceSelectionUISurfaces::kSettings, params_->was_paired,
      GetBluetoothTransport(params_->transport));
#endif  // BUILDFLAG(IS_CHROMEOS)

  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////

}  // namespace api

}  // namespace extensions
