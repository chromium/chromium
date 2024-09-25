// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_utils.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/constants/chromeos_features.h"
#include "device/base/features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_client_uuids.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/ble_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace device {

namespace {

// https://www.bluetooth.com/specifications/gatt/services.
const char kHIDServiceUUID[] = "1812";

// https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-sdos.
const char kSecurityKeyServiceUUID[] = "FFFD";

constexpr base::TimeDelta kMaxDeviceSelectionDuration = base::Seconds(30);
constexpr base::TimeDelta kConnectionTimeIntervalThreshold = base::Minutes(15);
#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr base::TimeDelta kToastShownCountTimeIntervalThreshold =
    base::Hours(24);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

constexpr uint8_t kLimitedDiscoveryFlag = 0x01;
constexpr uint8_t kGeneralDiscoveryFlag = 0x02;

// Get limited number of devices from |devices| and
// prioritize paired/connecting devices over other devices.
BluetoothAdapter::DeviceList GetLimitedNumDevices(
    size_t max_device_num,
    const BluetoothAdapter::DeviceList& devices) {
  // If |max_device_num| is 0, it means there's no limit.
  if (max_device_num == 0)
    return devices;

  BluetoothAdapter::DeviceList result;
  for (BluetoothDevice* device : devices) {
    if (result.size() == max_device_num)
      break;

    if (device->IsPaired() || device->IsConnecting())
      result.push_back(device);
  }

  for (BluetoothDevice* device : devices) {
    if (result.size() == max_device_num)
      break;

    if (!device->IsPaired() && !device->IsConnecting())
      result.push_back(device);
  }

  return result;
}

// Filter out unknown devices from the list.
BluetoothAdapter::DeviceList FilterUnknownDevices(
    const BluetoothAdapter::DeviceList& devices) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::switches::IsUnfilteredBluetoothDevicesEnabled())
    return devices;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (chromeos::BrowserParamsProxy::Get()
          ->IsUnfilteredBluetoothDeviceEnabled()) {
    return devices;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  BluetoothAdapter::DeviceList result;
  for (BluetoothDevice* device : devices) {
    if (device::IsUnsupportedDevice(device))
      continue;

    result.push_back(device);
  }
  return result;
}

void RecordPairingDuration(const std::string& histogram_name,
                           base::TimeDelta pairing_duration) {
  base::UmaHistogramCustomTimes(histogram_name, pairing_duration,
                                base::Milliseconds(1) /* min */,
                                base::Seconds(30) /* max */, 50 /* buckets */);
}

void RecordPairingTransport(BluetoothTransport transport) {
  BluetoothTransportType type;
  switch (transport) {
    case BLUETOOTH_TRANSPORT_CLASSIC:
      type = BluetoothTransportType::kClassic;
      break;
    case BLUETOOTH_TRANSPORT_LE:
      type = BluetoothTransportType::kLE;
      break;
    case BLUETOOTH_TRANSPORT_DUAL:
      type = BluetoothTransportType::kDual;
      break;
    case BLUETOOTH_TRANSPORT_INVALID:
      type = BluetoothTransportType::kInvalid;
      break;
    default:
      type = BluetoothTransportType::kUnknown;
      break;
  }

  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.Pairing.TransportType",
                                type);
}

void RecordDeviceSelectionDuration(const std::string& histogram_name,
                                   base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(
      histogram_name, duration, base::Milliseconds(1) /* min */,
      kMaxDeviceSelectionDuration /* max */, 50 /* buckets */);
}

std::string GetTransportName(BluetoothTransport transport) {
  switch (transport) {
    case BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC:
      return "Classic";
    case BluetoothTransport::BLUETOOTH_TRANSPORT_LE:
      return "BLE";
    case BluetoothTransport::BLUETOOTH_TRANSPORT_DUAL:
      return "Dual";
    case BLUETOOTH_TRANSPORT_INVALID:
      return "Invalid";
    default:
      // A transport type of other is unexpected, and no success
      // metric for it exists.
      return "";
  }
}

bool IsUserError(std::optional<ConnectionFailureReason> failure_reason) {
  if (!failure_reason.has_value()) {
    return false;
  }

  switch (failure_reason.value()) {
    case ConnectionFailureReason::kInprogress:
      [[fallthrough]];
    case ConnectionFailureReason::kNotFound:
      [[fallthrough]];
    case ConnectionFailureReason::kAuthCanceled:
      return true;
    case ConnectionFailureReason::kAuthRejected:
      [[fallthrough]];
    case ConnectionFailureReason::kUnknownError:
      [[fallthrough]];
    case ConnectionFailureReason::kAuthFailed:
      [[fallthrough]];
    case ConnectionFailureReason::kAuthTimeout:
      [[fallthrough]];
    case ConnectionFailureReason::kUnknownConnectionError:
      [[fallthrough]];
    case ConnectionFailureReason::kUnsupportedDevice:
      [[fallthrough]];
    case ConnectionFailureReason::kNotConnectable:
      [[fallthrough]];
    case ConnectionFailureReason::kSystemError:
      [[fallthrough]];
    case ConnectionFailureReason::kFailed:
      [[fallthrough]];
    case ConnectionFailureReason::kBluetoothDisabled:
      [[fallthrough]];
    case ConnectionFailureReason::kDeviceNotReady:
      [[fallthrough]];
    case ConnectionFailureReason::kAlreadyConnected:
      [[fallthrough]];
    case ConnectionFailureReason::kDeviceAlreadyExists:
      [[fallthrough]];
    case ConnectionFailureReason::kInvalidArgs:
      [[fallthrough]];
    case ConnectionFailureReason::kNonAuthTimeout:
      [[fallthrough]];
    case ConnectionFailureReason::kNoMemory:
      [[fallthrough]];
    case ConnectionFailureReason::kJniEnvironment:
      [[fallthrough]];
    case ConnectionFailureReason::kJniThreadAttach:
      [[fallthrough]];
    case ConnectionFailureReason::kWakelock:
      [[fallthrough]];
    case ConnectionFailureReason::kUnexpectedState:
      [[fallthrough]];
    case ConnectionFailureReason::kSocketError:
      return false;
  }
  NOTREACHED();
}

void EmitFilteredFailureReason(ConnectionFailureReason failure_reason,
                               const std::string& transport_name) {
  switch (failure_reason) {
    case ConnectionFailureReason::kAuthCanceled:
      [[fallthrough]];
    case ConnectionFailureReason::kAuthRejected:
      return;
    case ConnectionFailureReason::kUnknownError:
      [[fallthrough]];
    case ConnectionFailureReason::kAuthFailed:
      [[fallthrough]];
    case ConnectionFailureReason::kAuthTimeout:
      [[fallthrough]];
    case ConnectionFailureReason::kUnknownConnectionError:
      [[fallthrough]];
    case ConnectionFailureReason::kUnsupportedDevice:
      [[fallthrough]];
    case ConnectionFailureReason::kNotConnectable:
      [[fallthrough]];
    case ConnectionFailureReason::kSystemError:
      [[fallthrough]];
    case ConnectionFailureReason::kFailed:
      [[fallthrough]];
    case ConnectionFailureReason::kInprogress:
      [[fallthrough]];
    case ConnectionFailureReason::kNotFound:
      [[fallthrough]];
    case ConnectionFailureReason::kBluetoothDisabled:
      [[fallthrough]];
    case ConnectionFailureReason::kDeviceNotReady:
      [[fallthrough]];
    case ConnectionFailureReason::kAlreadyConnected:
      [[fallthrough]];
    case ConnectionFailureReason::kDeviceAlreadyExists:
      [[fallthrough]];
    case ConnectionFailureReason::kInvalidArgs:
      [[fallthrough]];
    case ConnectionFailureReason::kNonAuthTimeout:
      [[fallthrough]];
    case ConnectionFailureReason::kNoMemory:
      [[fallthrough]];
    case ConnectionFailureReason::kJniEnvironment:
      [[fallthrough]];
    case ConnectionFailureReason::kJniThreadAttach:
      [[fallthrough]];
    case ConnectionFailureReason::kWakelock:
      [[fallthrough]];
    case ConnectionFailureReason::kUnexpectedState:
      [[fallthrough]];
    case ConnectionFailureReason::kSocketError:
      const std::string result_histogram_name_prefix =
          "Bluetooth.ChromeOS.Pairing.Result";
      base::UmaHistogramEnumeration(
          result_histogram_name_prefix + ".FilteredFailureReason",
          failure_reason);
      base::UmaHistogramEnumeration(result_histogram_name_prefix +
                                        ".FilteredFailureReason." +
                                        transport_name,
                                    failure_reason);
      return;
  }
  NOTREACHED();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsNonPhonePolyDevice(const device::BluetoothDevice* device) {
  // OUI portions of Bluetooth addresses for devices manufactured by Poly. See
  // https://standards-oui.ieee.org/. This also includes acquisitions by Poly,
  // namely ViaVideo and PictureTel.
  // Actually there are still others belonging in this category, but they are
  // truly phones, e.g. Voyant and Spectralink, so we omit those here.
  constexpr auto kNonPhonePolyOuis = base::MakeFixedFlatSet<std::string_view>(
      {"64:16:7F", "48:25:67", "00:04:F2", "00:E0:DB", "00:10:1A"});

  return base::Contains(kNonPhonePolyOuis,
                        device->GetOuiPortionOfBluetoothAddress());
}
#endif

// Provide heuristics for which transport to use for a dual device
BluetoothTransport InferDeviceTransport(const device::BluetoothDevice* device) {
  if (device->GetType() != BLUETOOTH_TRANSPORT_DUAL) {
    return device->GetType();
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Random address type indicates LE device.
  if (device->GetAddressType() ==
      BluetoothDevice::AddressType::ADDR_TYPE_RANDOM) {
    return BLUETOOTH_TRANSPORT_LE;
  }
#endif

  // Devices without type/appearance most likely signals that it is truly only
  // a LE advertisement for a peripheral which is active, but not pairable. Many
  // popular headphones behave in this exact way. Mark as invalid until they
  // provide a type/appearance; this means they've become pairable. See
  // https://crrev.com/c/1656971 for more.
  if (device->GetDeviceType() == BluetoothDeviceType::UNKNOWN) {
    return BLUETOOTH_TRANSPORT_INVALID;
  }

  return BLUETOOTH_TRANSPORT_CLASSIC;
}

}  // namespace

ConnectionFailureReason GetConnectionFailureReason(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED:
      return device::ConnectionFailureReason::kAuthCanceled;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED:
      return device::ConnectionFailureReason::kAuthFailed;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED:
      return device::ConnectionFailureReason::kAuthRejected;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_TIMEOUT:
      return device::ConnectionFailureReason::kAuthTimeout;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED:
      return device::ConnectionFailureReason::kFailed;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS:
      return device::ConnectionFailureReason::kInprogress;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN:
      return device::ConnectionFailureReason::kUnknownError;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_UNSUPPORTED_DEVICE:
      return device::ConnectionFailureReason::kUnsupportedDevice;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_DEVICE_NOT_READY:
      return device::ConnectionFailureReason::kDeviceNotReady;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_ALREADY_CONNECTED:
      return device::ConnectionFailureReason::kAlreadyConnected;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_DEVICE_ALREADY_EXISTS:
      return device::ConnectionFailureReason::kDeviceAlreadyExists;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_DEVICE_UNCONNECTED:
      return device::ConnectionFailureReason::kNotConnectable;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_DOES_NOT_EXIST:
      return device::ConnectionFailureReason::kNotFound;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_INVALID_ARGS:
      return device::ConnectionFailureReason::kInvalidArgs;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_NON_AUTH_TIMEOUT:
      return device::ConnectionFailureReason::kNonAuthTimeout;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_NO_MEMORY:
      return device::ConnectionFailureReason::kNoMemory;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_JNI_ENVIRONMENT:
      return device::ConnectionFailureReason::kJniEnvironment;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_JNI_THREAD_ATTACH:
      return device::ConnectionFailureReason::kJniThreadAttach;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_WAKELOCK:
      return device::ConnectionFailureReason::kWakelock;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_UNEXPECTED_STATE:
      return device::ConnectionFailureReason::kUnexpectedState;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_SOCKET:
      return device::ConnectionFailureReason::kSocketError;
    case device::BluetoothDevice::ConnectErrorCode::NUM_CONNECT_ERROR_CODES:
      NOTREACHED();
  }
}

device::BluetoothAdapter::DeviceList FilterBluetoothDeviceList(
    const BluetoothAdapter::DeviceList& devices,
    BluetoothFilterType filter_type,
    int max_devices) {
  BluetoothAdapter::DeviceList filtered_devices =
      filter_type == BluetoothFilterType::KNOWN ? FilterUnknownDevices(devices)
                                                : devices;
  return GetLimitedNumDevices(max_devices, filtered_devices);
}

bool IsUnsupportedDevice(const device::BluetoothDevice* device) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::switches::IsUnfilteredBluetoothDevicesEnabled()) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (chromeos::BrowserParamsProxy::Get()
          ->IsUnfilteredBluetoothDeviceEnabled()) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Never filter out Poly devices; this requires a special case since these
  // devices often identify themselves as phones, which are disallowed below.
  // See b/228118615.
  if (IsNonPhonePolyDevice(device)) {
    return false;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Always allow bonded devices to appear in the UI.
  if (device->IsBonded()) {
    return false;
  }
#endif

  // Always filter out laptops, etc. There is no intended use case or
  // Bluetooth profile in this context.
  if (device->GetDeviceType() == BluetoothDeviceType::COMPUTER) {
    return true;
  }

  // Always filter out phones. There is no intended use case or Bluetooth
  // profile in this context.
  if (base::FeatureList::IsEnabled(chromeos::features::kBluetoothPhoneFilter) &&
      device->GetDeviceType() == BluetoothDeviceType::PHONE) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const BluetoothDevice::UUIDSet& uuids = device->GetUUIDs();

  // These UUIDs are specific to Nearby Share and Phone Hub and are used to
  // identify devices that should be filtered from the UI that otherwise would
  // not have been correctly identified. These devices should always be filtered
  // from the UI. For more information see b/219627324.
  for (const auto& uuid : ash::nearby::GetNearbyClientUuids()) {
    if (uuids.contains(uuid)) {
      return true;
    }
  }
  if (uuids.contains(BluetoothUUID(ash::secure_channel::kGattServerUuid))) {
    return true;
  }
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  // Allow paired devices which are not filtered above to appear in the UI.
  if (device->IsPaired()) {
    return false;
  }
#endif

  switch (InferDeviceTransport(device)) {
    // For LE devices, check the discoverable flag and UUIDs.
    case BLUETOOTH_TRANSPORT_LE:
      // Hide the LE device that mark itself as non-discoverble.
      if (device->GetAdvertisingDataFlags().has_value()) {
        if (!((kLimitedDiscoveryFlag | kGeneralDiscoveryFlag) &
              device->GetAdvertisingDataFlags().value())) {
          return true;
        }
      }
      // Check the service UUID to determine if it supports HID or second factor
      // authenticator (security key).
      if (base::Contains(device->GetUUIDs(),
                         device::BluetoothUUID(kHIDServiceUUID)) ||
          base::Contains(device->GetUUIDs(),
                         device::BluetoothUUID(kSecurityKeyServiceUUID))) {
        return false;
      }
      break;
    // For classic mode devices, only filter out if the name is empty because
    // the device could have an unknown or even known type and still also
    // provide audio/HID functionality.
    case BLUETOOTH_TRANSPORT_CLASSIC:
      if (device->GetName()) {
        return false;
      }
      break;
    // Otherwise, they are invalid, so filter them out.
    default:
      break;
  }

  return true;
}

void RecordPairingResult(std::optional<ConnectionFailureReason> failure_reason,
                         BluetoothTransport transport,
                         base::TimeDelta duration) {
  RecordPairingTransport(transport);

  std::string transport_name = GetTransportName(transport);
  if (transport_name.empty()) {
    return;
  }

  bool success = !failure_reason.has_value();
  std::string result_histogram_name_prefix =
      "Bluetooth.ChromeOS.Pairing.Result";
  std::string result_histogram_user_errors_filtered_name =
      result_histogram_name_prefix + "." + "UserErrorsFiltered2";

  base::UmaHistogramBoolean(result_histogram_name_prefix, success);
  base::UmaHistogramBoolean(result_histogram_name_prefix + "." + transport_name,
                            success);
  if (!IsUserError(failure_reason)) {
    base::UmaHistogramBoolean(result_histogram_user_errors_filtered_name,
                              success);
  }

  std::string duration_histogram_name_prefix =
      "Bluetooth.ChromeOS.Pairing.Duration";
  std::string success_histogram_name = success ? "Success" : "Failure";

  std::string base_histogram_name =
      duration_histogram_name_prefix + "." + success_histogram_name;
  RecordPairingDuration(base_histogram_name, duration);
  RecordPairingDuration(base_histogram_name + "." + transport_name, duration);

  if (!success) {
    base::UmaHistogramEnumeration(
        result_histogram_name_prefix + ".FailureReason", *failure_reason);
    base::UmaHistogramEnumeration(
        result_histogram_name_prefix + ".FailureReason." + transport_name,
        *failure_reason);
    EmitFilteredFailureReason(*failure_reason, transport_name);
  }
}

void RecordUserInitiatedReconnectionAttemptResult(
    std::optional<ConnectionFailureReason> failure_reason,
    UserInitiatedReconnectionUISurfaces surface) {
  bool success = !failure_reason.has_value();
  std::string base_histogram_name =
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result";
  std::string result_histogram_user_errors_filtered_name =
      base_histogram_name + ".UserErrorsFiltered";

  if (!IsUserError(failure_reason)) {
    base::UmaHistogramBoolean(result_histogram_user_errors_filtered_name,
                              success);
  }

  base::UmaHistogramBoolean(base_histogram_name, success);

  std::string surface_name =
      (surface == UserInitiatedReconnectionUISurfaces::kSettings
           ? "Settings"
           : "SystemTray");
  base::UmaHistogramBoolean(base_histogram_name + "." + surface_name, success);

  if (!success) {
    base::UmaHistogramEnumeration(base_histogram_name + ".FailureReason",
                                  *failure_reason);
    base::UmaHistogramEnumeration(
        base_histogram_name + ".FailureReason." + surface_name,
        *failure_reason);
  }
}

void RecordDeviceSelectionDuration(base::TimeDelta duration,
                                   DeviceSelectionUISurfaces surface,
                                   bool was_paired,
                                   BluetoothTransport transport) {
  // Throw out longtail results of the user taking longer than
  // |kMaxDeviceSelectionDuration|. Assume that these thrown out results reflect
  // the user not being actively engaged with device connection: leaving the
  // page open for a long time, walking away from computer, etc.
  if (duration > kMaxDeviceSelectionDuration)
    return;

  std::string base_histogram_name =
      "Bluetooth.ChromeOS.DeviceSelectionDuration";
  RecordDeviceSelectionDuration(base_histogram_name, duration);

  std::string surface_name =
      (surface == DeviceSelectionUISurfaces::kSettings ? "Settings"
                                                       : "SystemTray");
  std::string surface_histogram_name = base_histogram_name + "." + surface_name;
  RecordDeviceSelectionDuration(surface_histogram_name, duration);

  std::string paired_name = (was_paired ? "Paired" : "NotPaired");
  std::string paired_histogram_name =
      surface_histogram_name + "." + paired_name;
  RecordDeviceSelectionDuration(paired_histogram_name, duration);

  if (!was_paired) {
    std::string transport_name = GetTransportName(transport);
    if (transport_name.empty()) {
      return;
    }
    std::string transport_histogram_name =
        paired_histogram_name + "." + transport_name;
    RecordDeviceSelectionDuration(transport_histogram_name, duration);
  }
}

void RecordPoweredStateOperationResult(PoweredStateOperation operation,
                                       bool success) {
  std::string operation_name =
      operation == PoweredStateOperation::kEnable ? "Enable" : "Disable";

  base::UmaHistogramBoolean(base::StrCat({"Bluetooth.ChromeOS.PoweredState.",
                                          operation_name, ".Result"}),
                            success);
}

void RecordPoweredState(bool is_powered) {
  base::UmaHistogramBoolean("Bluetooth.ChromeOS.PoweredState", is_powered);
}

void RecordForgetResult(ForgetResult forget_result) {
  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.Forget.Result",
                                forget_result);
}

void RecordDeviceDisconnect(BluetoothDeviceType device_type) {
  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.DeviceDisconnect",
                                device_type);
}

void RecordUserInitiatedDisconnectResult(DisconnectResult disconnect_result,
                                         BluetoothTransport transport) {
  std::string transport_name = GetTransportName(transport);

  if (transport_name.empty()) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Bluetooth.ChromeOS.UserInitiatedDisconnect.Result", disconnect_result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Bluetooth.ChromeOS.UserInitiatedDisconnect.Result.",
                    transport_name}),
      disconnect_result);
}

void RecordUiSurfaceDisplayed(BluetoothUiSurface ui_surface) {
  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.UiSurfaceDisplayed",
                                ui_surface);
}

void RecordUserInitiatedReconnectionAttemptDuration(
    std::optional<ConnectionFailureReason> failure_reason,
    BluetoothTransport transport,
    base::TimeDelta duration) {
  bool success = !failure_reason.has_value();
  std::string transport_name = GetTransportName(transport);

  if (transport_name.empty()) {
    return;
  }
  std::string success_histogram_name = success ? "Success" : "Failure";

  std::string base_histogram_name = base::StrCat(
      {"Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Duration.",
       success_histogram_name});
  base::UmaHistogramTimes(base_histogram_name, duration);
  base::UmaHistogramTimes(
      base::StrCat({base_histogram_name, ".", transport_name}), duration);
}

void RecordSetDeviceNickName(SetNicknameResult set_nickname_result) {
  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.SetNickname.Result",
                                set_nickname_result);
}

void RecordTimeIntervalBetweenConnections(
    base::TimeDelta time_interval_since_last_connection) {
  if (time_interval_since_last_connection >= kConnectionTimeIntervalThreshold) {
    return;
  }
  base::UmaHistogramCustomTimes(
      "Bluetooth.ChromeOS.TimeIntervalBetweenConnections",
      time_interval_since_last_connection,
      /*min=*/base::Milliseconds(0),
      /*max=*/kConnectionTimeIntervalThreshold, 100);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void MaybeRecordConnectionToastShownCount(PrefService* local_state_pref,
                                          bool triggered_by_connect) {
  bool is_within_24_hrs =
      base::Time::Now() -
          local_state_pref->GetTime(ash::prefs::kBluetoothToastCountStartTime) <
      kToastShownCountTimeIntervalThreshold;
  int toast_shown_count = local_state_pref->GetInteger(
      ash::prefs::kBluetoothConnectionToastShownCount);

  if (is_within_24_hrs && triggered_by_connect) {
    // Increment the count if within 24 hours and it is triggered by connect.
    local_state_pref->SetInteger(
        ash::prefs::kBluetoothConnectionToastShownCount, toast_shown_count + 1);
    return;
  }

  if (is_within_24_hrs) {
    // Do nothing since we haven't exceeded the time interval.
    return;
  }

  // Emit metric and reset count and timestamp if 24 hours have passed.
  base::UmaHistogramCounts100(
      "Bluetooth.ChromeOS.ConnectionToastShownIn24Hours.Count",
      toast_shown_count);
  // Reset the count, and update the start time.
  local_state_pref->SetInteger(ash::prefs::kBluetoothConnectionToastShownCount,
                               triggered_by_connect ? 1 : 0);
  local_state_pref->SetTime(ash::prefs::kBluetoothToastCountStartTime,
                            base::Time::Now().LocalMidnight());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void RecordFlossManagerClientInit(bool success, base::TimeDelta duration) {
  static constexpr char kSuccessHistogramSuffix[] = "Success";
  static constexpr char kFailureHistogramSuffix[] = "Failure";
  std::string success_histogram_name =
      success ? kSuccessHistogramSuffix : kFailureHistogramSuffix;

  base::UmaHistogramTimes(
      base::StrCat({"Bluetooth.ChromeOS.FlossManagerClientInit.Duration.",
                    success_histogram_name}),
      duration);

  base::UmaHistogramBoolean("Bluetooth.ChromeOS.FlossManagerClientInit.Result",
                            success);
}

}  // namespace device
