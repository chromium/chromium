// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/bluetooth_task_manager_win.h"

#include <winsock2.h>

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "device/bluetooth/bluetooth_classic_win.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_init_win.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "net/base/winsock_init.h"

namespace {

const int kMaxNumDeviceAddressChar = 127;
const int kServiceDiscoveryResultBufferSize = 5000;

// See http://goo.gl/iNTRQe: cTimeoutMultiplier: A value that indicates the time
// out for the inquiry, expressed in increments of 1.28 seconds. For example, an
// inquiry of 12.8 seconds has a cTimeoutMultiplier value of 10. The maximum
// value for this member is 48. When a value greater than 48 is used, the
// calling function immediately fails and returns
const int kMaxDeviceDiscoveryTimeoutMultiplier = 48;

typedef device::BluetoothTaskManagerWin::ServiceRecordState ServiceRecordState;

// Note: The string returned here must have the same format as
// CanonicalizeBluetoothAddress.
std::string BluetoothAddressToCanonicalString(const BLUETOOTH_ADDRESS& btha) {
  std::string result = base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
                                          btha.rgBytes[5],
                                          btha.rgBytes[4],
                                          btha.rgBytes[3],
                                          btha.rgBytes[2],
                                          btha.rgBytes[1],
                                          btha.rgBytes[0]);
  DCHECK_EQ(result, device::CanonicalizeBluetoothAddress(result));
  return result;
}

// Populates bluetooth adapter state from the currently open adapter.
void GetAdapterState(device::win::BluetoothClassicWrapper* classic_wrapper,
                     device::BluetoothTaskManagerWin::AdapterState* state) {
  std::string name;
  std::string address;
  bool powered = false;
  BLUETOOTH_RADIO_INFO adapter_info = {sizeof(BLUETOOTH_RADIO_INFO)};
  if (classic_wrapper->HasHandle() &&
      ERROR_SUCCESS == classic_wrapper->GetRadioInfo(&adapter_info)) {
    name = base::SysWideToUTF8(adapter_info.szName);
    address = BluetoothAddressToCanonicalString(adapter_info.address);
    powered = !!classic_wrapper->IsConnectable();
  }
  state->name = name;
  state->address = address;
  state->powered = powered;
}

void GetDeviceState(const BLUETOOTH_DEVICE_INFO& device_info,
                    device::BluetoothTaskManagerWin::DeviceState* state) {
  state->name = base::SysWideToUTF8(device_info.szName);
  state->address = BluetoothAddressToCanonicalString(device_info.Address);
  state->bluetooth_class = device_info.ulClassofDevice;
  state->visible = true;
  state->connected = !!device_info.fConnected;
  state->authenticated = !!device_info.fAuthenticated;
}

}  // namespace

namespace device {

// static
const int BluetoothTaskManagerWin::kPollIntervalMs = 500;

BluetoothTaskManagerWin::AdapterState::AdapterState() : powered(false) {
}

BluetoothTaskManagerWin::AdapterState::~AdapterState() {
}

BluetoothTaskManagerWin::ServiceRecordState::ServiceRecordState() {
}

BluetoothTaskManagerWin::ServiceRecordState::~ServiceRecordState() {
}

BluetoothTaskManagerWin::DeviceState::DeviceState()
    : visible(false),
      connected(false),
      authenticated(false),
      bluetooth_class(0) {
}

BluetoothTaskManagerWin::DeviceState::~DeviceState() {
}

BluetoothTaskManagerWin::BluetoothTaskManagerWin(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner)),
      classic_wrapper_(std::make_unique<win::BluetoothClassicWrapper>()) {}

BluetoothTaskManagerWin::BluetoothTaskManagerWin(
    std::unique_ptr<win::BluetoothClassicWrapper> classic_wrapper,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner)),
      classic_wrapper_(std::move(classic_wrapper)) {}

BluetoothTaskManagerWin::~BluetoothTaskManagerWin() = default;

// static
scoped_refptr<BluetoothTaskManagerWin>
BluetoothTaskManagerWin::CreateForTesting(
    std::unique_ptr<win::BluetoothClassicWrapper> classic_wrapper,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  return new BluetoothTaskManagerWin(std::move(classic_wrapper),
                                     std::move(ui_task_runner));
}

void BluetoothTaskManagerWin::AddObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  observers_.AddObserver(observer);
}

void BluetoothTaskManagerWin::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  observers_.RemoveObserver(observer);
}

void BluetoothTaskManagerWin::Initialize() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  InitializeWithBluetoothTaskRunner(base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
}

void BluetoothTaskManagerWin::InitializeWithBluetoothTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  bluetooth_task_runner_ = bluetooth_task_runner;
  bluetooth_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothTaskManagerWin::StartPolling, this));
}

void BluetoothTaskManagerWin::StartPolling() {
  DCHECK(bluetooth_task_runner_->RunsTasksInCurrentSequence());

  if (device::bluetooth_init_win::HasBluetoothStack()) {
    PollAdapter();
  } else {
    // IF the bluetooth stack is not available, we still send an empty state
    // to BluetoothAdapter so that it is marked initialized, but the adapter
    // will not be present.
    AdapterState* state = new AdapterState();
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&BluetoothTaskManagerWin::OnAdapterStateChanged, this,
                       base::Owned(state)));
  }
}

void BluetoothTaskManagerWin::PostSetPoweredBluetoothTask(
    bool powered,
    base::OnceClosure callback,
    BluetoothAdapter::ErrorCallback error_callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  bluetooth_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothTaskManagerWin::SetPowered, this, powered,
                     std::move(callback), std::move(error_callback)));
}

void BluetoothTaskManagerWin::PostStartDiscoveryTask() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  bluetooth_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothTaskManagerWin::StartDiscovery, this));
}

void BluetoothTaskManagerWin::PostStopDiscoveryTask() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  bluetooth_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothTaskManagerWin::StopDiscovery, this));
}

void BluetoothTaskManagerWin::LogPollingError(const char* message,
                                              int win32_error) {
  const int kLogPeriodInMilliseconds = 60 * 1000;
  const int kMaxMessagesPerLogPeriod = 10;

  // Check if we need to discard this message
  if (!current_logging_batch_ticks_.is_null()) {
    if (base::TimeTicks::Now() - current_logging_batch_ticks_ <=
        base::Milliseconds(kLogPeriodInMilliseconds)) {
      if (current_logging_batch_count_ >= kMaxMessagesPerLogPeriod)
        return;
    } else {
      // The batch expired, reset it to "null".
      current_logging_batch_ticks_ = base::TimeTicks();
    }
  }

  // Keep track of this batch of messages
  if (current_logging_batch_ticks_.is_null()) {
    current_logging_batch_ticks_ = base::TimeTicks::Now();
    current_logging_batch_count_ = 0;
  }
  ++current_logging_batch_count_;

  // Log the message
  if (win32_error == 0)
    LOG(WARNING) << message;
  else
    LOG(WARNING) << message << ": "
                 << logging::SystemErrorCodeToString(win32_error);
}

void BluetoothTaskManagerWin::OnAdapterStateChanged(const AdapterState* state) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observers_)
    observer.AdapterStateChanged(*state);
}

void BluetoothTaskManagerWin::OnDiscoveryStarted(bool success) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observers_)
    observer.DiscoveryStarted(success);
}

void BluetoothTaskManagerWin::OnDiscoveryStopped() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observers_)
    observer.DiscoveryStopped();
}

void BluetoothTaskManagerWin::OnDevicesPolled(
    std::vector<std::unique_ptr<DeviceState>> devices) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  for (auto& observer : observers_)
    observer.DevicesPolled(devices);
}

void BluetoothTaskManagerWin::PollAdapter() {
  DCHECK(bluetooth_task_runner_->RunsTasksInCurrentSequence());

  // Skips updating the adapter info if the adapter is in discovery mode.
  if (!discovering_) {
    const BLUETOOTH_FIND_RADIO_PARAMS adapter_param =
        { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
    HBLUETOOTH_RADIO_FIND handle =
        classic_wrapper_->FindFirstRadio(&adapter_param);

    if (handle) {
      GetKnownDevices();
      classic_wrapper_->FindRadioClose(handle);
    } else {
      // If `handle` is null, reset `classic_wrapper_` to avoid stale data
      // coming from the opened radio handle in the `classic_wrapper_`.
      classic_wrapper_ = std::make_unique<win::BluetoothClassicWrapper>();
    }

    PostAdapterStateToUi();
  }

  // Re-poll.
  bluetooth_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&BluetoothTaskManagerWin::PollAdapter, this),
      base::Milliseconds(kPollIntervalMs));
}

void BluetoothTaskManagerWin::PostAdapterStateToUi() {
  DCHECK(bluetooth_task_runner_->RunsTasksInCurrentSequence());
  AdapterState* state = new AdapterState();
  GetAdapterState(classic_wrapper_.get(), state);
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothTaskManagerWin::OnAdapterStateChanged,
                                this, base::Owned(state)));
}

void BluetoothTaskManagerWin::SetPowered(
    bool powered,
    base::OnceClosure callback,
    BluetoothAdapter::ErrorCallback error_callback) {
  DCHECK(bluetooth_task_runner_->RunsTasksInCurrentSequence());
  bool success = false;
  if (classic_wrapper_->HasHandle()) {
    if (!powered)
      classic_wrapper_->EnableDiscovery(false);

    success = !!classic_wrapper_->EnableIncomingConnections(powered);
  }

  if (success) {
    PostAdapterStateToUi();
    ui_task_runner_->PostTask(FROM_HERE, std::move(callback));
  } else {
    ui_task_runner_->PostTask(FROM_HERE, std::move(error_callback));
  }
}

void BluetoothTaskManagerWin::StartDiscovery() {
  DCHECK(bluetooth_task_runner_->RunsTasksInCurrentSequence());
  bool adapter_opened = classic_wrapper_->HasHandle();
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothTaskManagerWin::OnDiscoveryStarted,
                                this, adapter_opened));
  if (!adapter_opened)
    return;
  discovering_ = true;

  DiscoverDevices(1);
}

void BluetoothTaskManagerWin::StopDiscovery() {
  DCHECK(bluetooth_task_runner_->RunsTasksInCurrentSequence());
  discovering_ = false;
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothTaskManagerWin::OnDiscoveryStopped, this));
}

void BluetoothTaskManagerWin::DiscoverDevices(int timeout_multiplier) {
  DCHECK(bluetooth_task_runner_->RunsTasksInCurrentSequence());
  if (!discovering_ || !classic_wrapper_->HasHandle())
    return;

  std::vector<std::unique_ptr<DeviceState>> device_list;
  if (SearchDevices(timeout_multiplier, false, &device_list)) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothTaskManagerWin::OnDevicesPolled,
                                  this, std::move(device_list)));
  }

  if (timeout_multiplier < kMaxDeviceDiscoveryTimeoutMultiplier)
    ++timeout_multiplier;
  bluetooth_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothTaskManagerWin::DiscoverDevices, this,
                                timeout_multiplier));
}

void BluetoothTaskManagerWin::GetKnownDevices() {
  std::vector<std::unique_ptr<DeviceState>> device_list;
  if (SearchDevices(1, true, &device_list)) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothTaskManagerWin::OnDevicesPolled,
                                  this, std::move(device_list)));
  }
}

bool BluetoothTaskManagerWin::SearchDevices(
    int timeout_multiplier,
    bool search_cached_devices_only,
    std::vector<std::unique_ptr<DeviceState>>* device_list) {
  return SearchClassicDevices(timeout_multiplier, search_cached_devices_only,
                              device_list) &&
         DiscoverServices(device_list, search_cached_devices_only);
}

bool BluetoothTaskManagerWin::SearchClassicDevices(
    int timeout_multiplier,
    bool search_cached_devices_only,
    std::vector<std::unique_ptr<DeviceState>>* device_list) {
  // Issues a device inquiry and waits for |timeout_multiplier| * 1.28 seconds.
  BLUETOOTH_DEVICE_SEARCH_PARAMS device_search_params;
  ZeroMemory(&device_search_params, sizeof(device_search_params));
  device_search_params.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
  device_search_params.fReturnAuthenticated = 1;
  device_search_params.fReturnRemembered = 1;
  device_search_params.fReturnUnknown = (search_cached_devices_only ? 0 : 1);
  device_search_params.fReturnConnected = 1;
  device_search_params.fIssueInquiry = (search_cached_devices_only ? 0 : 1);
  device_search_params.cTimeoutMultiplier = timeout_multiplier;

  BLUETOOTH_DEVICE_INFO device_info;
  ZeroMemory(&device_info, sizeof(device_info));
  device_info.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
  HBLUETOOTH_DEVICE_FIND handle =
      classic_wrapper_->FindFirstDevice(&device_search_params, &device_info);
  if (!handle) {
    int last_error = classic_wrapper_->LastError();
    if (last_error == ERROR_NO_MORE_ITEMS) {
      return true;  // No devices is not an error.
    }
    LogPollingError("Error calling BluetoothFindFirstDevice", last_error);
    return false;
  }

  while (true) {
    auto device_state = std::make_unique<DeviceState>();
    GetDeviceState(device_info, device_state.get());
    device_list->push_back(std::move(device_state));

    // Reset device info before next call (as a safety precaution).
    ZeroMemory(&device_info, sizeof(device_info));
    device_info.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    if (!classic_wrapper_->FindNextDevice(handle, &device_info)) {
      int last_error = classic_wrapper_->LastError();
      if (last_error == ERROR_NO_MORE_ITEMS) {
        break;  // No more items is expected error when done enumerating.
      }
      LogPollingError("Error calling BluetoothFindNextDevice", last_error);
      classic_wrapper_->FindDeviceClose(handle);
      return false;
    }
  }

  if (!classic_wrapper_->FindDeviceClose(handle)) {
    LogPollingError("Error calling BluetoothFindDeviceClose",
                    classic_wrapper_->LastError());
    return false;
  }
  return true;
}

bool BluetoothTaskManagerWin::DiscoverServices(
    std::vector<std::unique_ptr<DeviceState>>* device_list,
    bool search_cached_services_only) {
  DCHECK(bluetooth_task_runner_->RunsTasksInCurrentSequence());
  net::EnsureWinsockInit();
  for (const auto& device : *device_list) {
    std::vector<std::unique_ptr<ServiceRecordState>>* service_record_states =
        &device->service_record_states;

    if (!DiscoverClassicDeviceServices(device->address, L2CAP_PROTOCOL_UUID,
                                       search_cached_services_only,
                                       service_record_states)) {
      return false;
    }
  }
  return true;
}

bool BluetoothTaskManagerWin::DiscoverClassicDeviceServices(
    const std::string& device_address,
    const GUID& protocol_uuid,
    bool search_cached_services_only,
    std::vector<std::unique_ptr<ServiceRecordState>>* service_record_states) {
  int error_code =
      DiscoverClassicDeviceServicesWorker(device_address,
                                          protocol_uuid,
                                          search_cached_services_only,
                                          service_record_states);
  // If the device is "offline", no services are returned when specifying
  // "LUP_FLUSHCACHE". Try again without flushing the cache so that the list
  // of previously known services is returned.
  if (!search_cached_services_only &&
      (error_code == WSASERVICE_NOT_FOUND || error_code == WSANO_DATA)) {
    error_code = DiscoverClassicDeviceServicesWorker(
        device_address, protocol_uuid, true, service_record_states);
  }

  return (error_code == ERROR_SUCCESS);
}

int BluetoothTaskManagerWin::DiscoverClassicDeviceServicesWorker(
    const std::string& device_address,
    const GUID& protocol_uuid,
    bool search_cached_services_only,
    std::vector<std::unique_ptr<ServiceRecordState>>* service_record_states) {
  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  // Bluetooth and WSAQUERYSET for Service Inquiry. See http://goo.gl/2v9pyt.
  WSAQUERYSET sdp_query;
  ZeroMemory(&sdp_query, sizeof(sdp_query));
  sdp_query.dwSize = sizeof(sdp_query);
  GUID protocol = protocol_uuid;
  sdp_query.lpServiceClassId = &protocol;
  sdp_query.dwNameSpace = NS_BTH;
  wchar_t device_address_context[kMaxNumDeviceAddressChar];
  std::size_t length = base::SysUTF8ToWide("(" + device_address + ")").copy(
      device_address_context, kMaxNumDeviceAddressChar);
  device_address_context[length] = NULL;
  sdp_query.lpszContext = device_address_context;
  DWORD control_flags = LUP_RETURN_ALL;
  // See http://goo.gl/t1Hulo: "Applications should generally specify
  // LUP_FLUSHCACHE. This flag instructs the system to ignore any cached
  // information and establish an over-the-air SDP connection to the specified
  // device to perform the SDP search. This non-cached operation may take
  // several seconds (whereas a cached search returns quickly)."
  // In summary, we need to specify LUP_FLUSHCACHE if we want to obtain the list
  // of services for devices which have not been discovered before.
  if (!search_cached_services_only)
    control_flags |= LUP_FLUSHCACHE;
  HANDLE sdp_handle;
  if (ERROR_SUCCESS !=
      WSALookupServiceBegin(&sdp_query, control_flags, &sdp_handle)) {
    int last_error = WSAGetLastError();
    // If the device is "offline", no services are returned when specifying
    // "LUP_FLUSHCACHE". Don't log error in that case.
    if (!search_cached_services_only &&
        (last_error == WSASERVICE_NOT_FOUND || last_error == WSANO_DATA)) {
      return last_error;
    }
    LogPollingError("Error calling WSALookupServiceBegin", last_error);
    return last_error;
  }
  char sdp_buffer[kServiceDiscoveryResultBufferSize];
  LPWSAQUERYSET sdp_result_data = reinterpret_cast<LPWSAQUERYSET>(sdp_buffer);
  while (true) {
    DWORD sdp_buffer_size = sizeof(sdp_buffer);
    if (ERROR_SUCCESS !=
        WSALookupServiceNext(
            sdp_handle, control_flags, &sdp_buffer_size, sdp_result_data)) {
      int last_error = WSAGetLastError();
      if (last_error == WSA_E_NO_MORE || last_error == WSAENOMORE) {
        break;
      }
      LogPollingError("Error calling WSALookupServiceNext", last_error);
      WSALookupServiceEnd(sdp_handle);
      return last_error;
    }
    auto service_record_state = std::make_unique<ServiceRecordState>();
    service_record_state->name =
        base::SysWideToUTF8(sdp_result_data->lpszServiceInstanceName);
    for (uint64_t i = 0; i < sdp_result_data->lpBlob->cbSize; i++) {
      service_record_state->sdp_bytes.push_back(
          sdp_result_data->lpBlob->pBlobData[i]);
    }
    service_record_states->push_back(std::move(service_record_state));
  }
  if (ERROR_SUCCESS != WSALookupServiceEnd(sdp_handle)) {
    int last_error = WSAGetLastError();
    LogPollingError("Error calling WSALookupServiceEnd", last_error);
    return last_error;
  }

  return ERROR_SUCCESS;
}

}  // namespace device
