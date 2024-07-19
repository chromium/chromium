// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/adapter.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "device/bluetooth/advertisement.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/device.h"
#include "device/bluetooth/discovery_session.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/public/mojom/connect_result_type_converter.h"
#include "device/bluetooth/server_socket.h"
#include "device/bluetooth/socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "device/bluetooth/bluez/metrics_recorder.h"
#endif

namespace bluetooth {
namespace {

const char kMojoReceivingPipeError[] = "Failed to create receiving DataPipe.";
const char kMojoSendingPipeError[] = "Failed to create sending DataPipe.";
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
const char kCannotConnectToDeviceError[] = "Cannot connect to device.";
#endif

}  // namespace

Adapter::ConnectToServiceRequestDetails::ConnectToServiceRequestDetails(
    const std::string& address,
    const device::BluetoothUUID& service_uuid,
    const base::Time& time_requested,
    const bool should_unbond_on_error,
    ConnectToServiceInsecurelyCallback callback)
    : address(address),
      service_uuid(service_uuid),
      time_requested(time_requested),
      should_unbond_on_error(should_unbond_on_error),
      callback(std::move(callback)) {}

Adapter::ConnectToServiceRequestDetails::~ConnectToServiceRequestDetails() =
    default;

Adapter::Adapter(scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(std::move(adapter)) {
  adapter_->AddObserver(this);
}

Adapter::~Adapter() {
  for (auto& entry : connect_to_service_request_map_) {
    base::UmaHistogramMediumTimes(
        "Bluetooth.Mojo.PendingConnectAtShutdown."
        "DurationWaiting",
        base::Time::Now() - entry.second->time_requested);
  }
  base::UmaHistogramCounts100(
      "Bluetooth.Mojo.PendingConnectAtShutdown."
      "NumberOfServiceDiscoveriesInProgress",
      connect_to_service_requests_pending_discovery_.size());

  adapter_->RemoveObserver(this);
  adapter_ = nullptr;
}

void Adapter::ConnectToDevice(const std::string& address,
                              ConnectToDeviceCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(address);

  if (!device) {
    std::move(callback).Run(mojom::ConnectResult::DEVICE_NO_LONGER_IN_RANGE,
                            /*device=*/mojo::NullRemote());
    return;
  }

  device->CreateGattConnection(base::BindOnce(&Adapter::OnGattConnect,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              std::move(callback)));
}

void Adapter::GetDevices(GetDevicesCallback callback) {
  std::vector<mojom::DeviceInfoPtr> devices;

  for (const device::BluetoothDevice* device : adapter_->GetDevices()) {
    mojom::DeviceInfoPtr device_info =
        Device::ConstructDeviceInfoStruct(device);
    devices.push_back(std::move(device_info));
  }

  std::move(callback).Run(std::move(devices));
}

void Adapter::GetInfo(GetInfoCallback callback) {
  mojom::AdapterInfoPtr adapter_info = mojom::AdapterInfo::New();
  adapter_info->address = adapter_->GetAddress();
  adapter_info->name = adapter_->GetName();
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  adapter_info->system_name = adapter_->GetSystemName();
#endif
#if BUILDFLAG(IS_CHROMEOS)
  adapter_info->floss = floss::features::IsFlossEnabled();
  adapter_info->extended_advertisement_support =
      adapter_->IsExtendedAdvertisementsAvailable();
#endif
  adapter_info->initialized = adapter_->IsInitialized();
  adapter_info->present = adapter_->IsPresent();
  adapter_info->powered = adapter_->IsPowered();
  adapter_info->discoverable = adapter_->IsDiscoverable();
  adapter_info->discovering = adapter_->IsDiscovering();
  std::move(callback).Run(std::move(adapter_info));
}

void Adapter::AddObserver(mojo::PendingRemote<mojom::AdapterObserver> observer,
                          AddObserverCallback callback) {
  observers_.Add(std::move(observer));
  std::move(callback).Run();
}

void Adapter::RegisterAdvertisement(const device::BluetoothUUID& service_uuid,
                                    const std::vector<uint8_t>& service_data,
                                    bool use_scan_response,
                                    bool connectable,
                                    RegisterAdvertisementCallback callback) {
  auto advertisement_data =
      std::make_unique<device::BluetoothAdvertisement::Data>(
          connectable
              ? device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_PERIPHERAL
              : device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

  device::BluetoothAdvertisement::UUIDList uuid_list;
  uuid_list.push_back(service_uuid.value());
  advertisement_data->set_service_uuids(std::move(uuid_list));

  if (!use_scan_response) {
    device::BluetoothAdvertisement::ServiceData service_data_map;
    service_data_map.emplace(service_uuid.value(), service_data);
    advertisement_data->set_service_data(std::move(service_data_map));
  } else {
    // Require the service uuid to be in 128-bit format.
    DCHECK_EQ(service_uuid.format(),
              device::BluetoothUUID::Format::kFormat128Bit);
    device::BluetoothAdvertisement::ScanResponseData scan_response_data_map;
    // Start with the original scan response data.
    std::vector<uint8_t> scan_response_data(service_data.begin(),
                                            service_data.end());
    // Now insert in front of the service data the identifying 2-bytes of the
    // service id assuming this is a valid 16-bit uuid. For example, the uuid:
    // 0000fef3-0000-1000-8000-00805f9b34fb can be uniquely defined by two bytes
    // ****fef3-****-****-****-************ the rest is the same for all 16-bit
    // uuids as defined by the Bluetooth spec. We insert them in little endian
    // ordering 0xf3 first, then 0xfe in for this example.
    auto service_id_bytes = service_uuid.GetBytes();
    // Take bytes 2 and 3.
    auto id_bytes = base::make_span(service_id_bytes).subspan(2, 2);
    // Add them in reverse order (little endian).
    scan_response_data.insert(scan_response_data.begin(), id_bytes.rbegin(),
                              id_bytes.rend());
    // The platform API only supports AD Type 0x16 "Service Data" which assumes
    // as 16-bit service id.
    scan_response_data_map.emplace(0x16, scan_response_data);
    advertisement_data->set_scan_response_data(
        std::move(scan_response_data_map));
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::BindOnce(&Adapter::OnRegisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&Adapter::OnRegisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void Adapter::SetDiscoverable(bool discoverable,
                              SetDiscoverableCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  adapter_->SetDiscoverable(discoverable,
                            base::BindOnce(&Adapter::OnSetDiscoverable,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(split_callback.first)),
                            base::BindOnce(&Adapter::OnSetDiscoverableError,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(split_callback.second)));
}

void Adapter::SetName(const std::string& name, SetNameCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  adapter_->SetName(
      name,
      base::BindOnce(&Adapter::OnSetName, weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&Adapter::OnSetNameError, weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void Adapter::StartDiscoverySession(const std::string& client_name,
                                    StartDiscoverySessionCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  adapter_->StartDiscoverySession(
      client_name,
      base::BindOnce(&Adapter::OnStartDiscoverySession,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&Adapter::OnDiscoverySessionError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void Adapter::ConnectToServiceInsecurely(
    const std::string& address,
    const device::BluetoothUUID& service_uuid,
    bool should_unbond_on_error,
    ConnectToServiceInsecurelyCallback callback) {
  if (!base::Contains(allowed_uuids_, service_uuid)) {
    std::move(callback).Run(/*result=*/nullptr);
    return;
  }

  auto* device = adapter_->GetDevice(address);
  int request_id = next_request_id_++;
  connect_to_service_request_map_.emplace(
      request_id, std::make_unique<ConnectToServiceRequestDetails>(
                      address, service_uuid, base::Time::Now(),
                      should_unbond_on_error, std::move(callback)));

  if (device) {
    ProcessDeviceForInsecureServiceConnection(request_id, device,
                                              /*disconnected=*/false);
    return;
  }

  // This device has neither been discovered, nor has it been paired/connected
  // to previously. Use the ConnectDevice() API, if available, to connect to it.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  adapter_->ConnectDevice(
      address, /*address_type=*/std::nullopt,
      base::BindOnce(&Adapter::OnDeviceFetchedForInsecureServiceConnection,
                     weak_ptr_factory_.GetWeakPtr(), request_id),
      base::BindOnce(&Adapter::OnConnectToServiceError,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
#else
  OnConnectToServiceError(request_id, "Device does not exist.");
#endif
}

void Adapter::CreateRfcommServiceInsecurely(
    const std::string& service_name,
    const device::BluetoothUUID& service_uuid,
    CreateRfcommServiceInsecurelyCallback callback) {
  if (!base::Contains(allowed_uuids_, service_uuid)) {
    std::move(callback).Run(/*server_socket=*/mojo::NullRemote());
    return;
  }

  device::BluetoothAdapter::ServiceOptions service_options;
  service_options.name = service_name;
  service_options.require_authentication = false;

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  adapter_->CreateRfcommService(
      service_uuid, service_options,
      base::BindOnce(&Adapter::OnCreateRfcommServiceInsecurely,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&Adapter::OnCreateRfcommServiceInsecurelyError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void Adapter::CreateLocalGattService(
    const device::BluetoothUUID& service_id,
    mojo::PendingRemote<mojom::GattServiceObserver> observer,
    CreateLocalGattServiceCallback callback) {
  // It is expected that callers of `CreateLocalGattService()` only call this
  // method when creating a new GATT service that corresponds to |service_id|.
  // See more details in //device/bluetooth/public/mojom/adapter.mojom method
  // documentation.
  CHECK(!base::Contains(uuid_to_local_gatt_service_map_, service_id));

  mojo::PendingReceiver<mojom::GattService> pending_gatt_service_receiver;
  mojo::PendingRemote<mojom::GattService> pending_gatt_service_remote =
      pending_gatt_service_receiver.InitWithNewPipeAndPassRemote();

  auto gatt_service = std::make_unique<GattService>(
      std::move(pending_gatt_service_receiver), std::move(observer), service_id,
      adapter_,
      base::BindOnce(&Adapter::OnGattServiceInvalidated,
                     base::Unretained(this)));
  uuid_to_local_gatt_service_map_.try_emplace(service_id,
                                              std::move(gatt_service));
  std::move(callback).Run(
      /*gatt_service=*/std::move(pending_gatt_service_remote));
}

void Adapter::IsLeScatternetDualRoleSupported(
    IsLeScatternetDualRoleSupportedCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  std::move(callback).Run(base::Contains(
      adapter_->GetSupportedRoles(),
      device::BluetoothAdapter::BluetoothRole::kCentralPeripheral));
#else
  std::move(callback).Run(/*is_supported=*/false);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void Adapter::AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                    bool present) {
  for (auto& observer : observers_)
    observer->PresentChanged(present);
}

void Adapter::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                    bool powered) {
  for (auto& observer : observers_)
    observer->PoweredChanged(powered);
}

void Adapter::AdapterDiscoverableChanged(device::BluetoothAdapter* adapter,
                                         bool discoverable) {
  for (auto& observer : observers_)
    observer->DiscoverableChanged(discoverable);
}

void Adapter::AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                        bool discovering) {
  for (auto& observer : observers_)
    observer->DiscoveringChanged(discovering);
}

void Adapter::DeviceAdded(device::BluetoothAdapter* adapter,
                          device::BluetoothDevice* device) {
  for (auto& observer : observers_)
    observer->DeviceAdded(Device::ConstructDeviceInfoStruct(device));
}

void Adapter::DeviceChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device) {
  // Because paired Bluetooth devices never fire device-removed events, we also
  // consider a null RSSI indicative of a device no longer being discoverable.
  // In this scenario, we fail any pending connection requests.
  if (!device->GetInquiryRSSI()) {
    ProcessPendingInsecureServiceConnectionRequest(device,
                                                   /*disconnected=*/true);
  }

  for (auto& observer : observers_)
    observer->DeviceChanged(Device::ConstructDeviceInfoStruct(device));
}

void Adapter::DeviceRemoved(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device) {
  // First abort the requests that are pending service discovery.
  ProcessPendingInsecureServiceConnectionRequest(device,
                                                 /*disconnected=*/true);

  // Then abort all other requests refer to this device.
  const std::string& address = device->GetAddress();
  std::vector<int> request_ids;
  for (const auto& [req_id, details] : connect_to_service_request_map_) {
    if (address == details->address) {
      request_ids.push_back(req_id);
    }
  }
  for (int req_id : request_ids) {
    ProcessDeviceForInsecureServiceConnection(req_id, device,
                                              /*disconnected=*/true);
  }

  // Finally emit DeviceRemoved to the observers.
  for (auto& observer : observers_)
    observer->DeviceRemoved(Device::ConstructDeviceInfoStruct(device));
}

void Adapter::GattServicesDiscovered(device::BluetoothAdapter* adapter,
                                     device::BluetoothDevice* device) {
  // GattServicesDiscovered() and IsGattServicesDiscoveryComplete() actually
  // indicate that all services on the remote device, including SDP, are
  // resolved. Once service probing for a device within a cached request (in
  // |pending_connect_to_service_args_|) concludes, attempt socket creation
  // again via ProcessDeviceForInsecureServiceConnection().
  if (device->IsGattServicesDiscoveryComplete()) {
    ProcessPendingInsecureServiceConnectionRequest(device,
                                                   /*disconnected=*/false);
  }
}

void Adapter::AllowConnectionsForUuid(
    const device::BluetoothUUID& service_uuid) {
  allowed_uuids_.emplace(service_uuid);
}

void Adapter::OnGattServiceInvalidated(device::BluetoothUUID service_id) {
  uuid_to_local_gatt_service_map_.erase(service_id);
}

void Adapter::OnDeviceFetchedForInsecureServiceConnection(
    int request_id,
    device::BluetoothDevice* device) {
  CHECK(device);
  ProcessDeviceForInsecureServiceConnection(request_id, device,
                                            /*disconnected=*/false);
}

void Adapter::ProcessDeviceForInsecureServiceConnection(
    int request_id,
    device::BluetoothDevice* device,
    bool disconnected) {
  CHECK(connect_to_service_request_map_.contains(request_id));
  CHECK(device);

  if (disconnected) {
    ExecuteConnectToServiceCallback(request_id, /*result=*/nullptr);
    return;
  }

  if (!device->IsPaired() && device->IsConnected() &&
      !device->IsGattServicesDiscoveryComplete()) {
    // This provided device is most likely a result of calling ConnectDevice():
    // it's connected, but the remote device's services are still being probed
    // (IsGattServicesDiscoveryComplete() refers to all services, not just GATT
    // services). That means attempting ConnectToServiceInsecurely() right now
    // would fail with an "InProgress" error. Wait for GattServicesDiscovered()
    // to be called to signal that ConnectToServiceInsecurely() can be called.
    connect_to_service_requests_pending_discovery_.push_back(request_id);
    return;
  }

  device->ConnectToServiceInsecurely(
      connect_to_service_request_map_[request_id]->service_uuid,
      base::BindOnce(&Adapter::OnConnectToService,
                     weak_ptr_factory_.GetWeakPtr(), request_id),
      base::BindOnce(&Adapter::OnConnectToServiceInsecurelyError,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void Adapter::ProcessPendingInsecureServiceConnectionRequest(
    device::BluetoothDevice* device,
    bool disconnected) {
  CHECK(device);
  const std::string& address = device->GetAddress();
  auto it = connect_to_service_requests_pending_discovery_.begin();
  while (it != connect_to_service_requests_pending_discovery_.end()) {
    auto request_it = connect_to_service_request_map_.find(*it);
    CHECK(request_it != connect_to_service_request_map_.end(),
          base::NotFatalUntil::M130);
    if (address == request_it->second->address) {
      ProcessDeviceForInsecureServiceConnection(*it, device, disconnected);
      it = connect_to_service_requests_pending_discovery_.erase(it);
    } else {
      ++it;
    }
  }
}

void Adapter::OnGattConnect(
    ConnectToDeviceCallback callback,
    std::unique_ptr<device::BluetoothGattConnection> connection,
    std::optional<device::BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code.has_value()) {
    std::move(callback).Run(
        mojo::ConvertTo<mojom::ConnectResult>(error_code.value()),
        /*device=*/mojo::NullRemote());
    return;
  }
  mojo::PendingRemote<mojom::Device> device;
  Device::Create(adapter_, std::move(connection),
                 device.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(mojom::ConnectResult::SUCCESS, std::move(device));
}

void Adapter::OnRegisterAdvertisement(
    RegisterAdvertisementCallback callback,
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  mojo::PendingRemote<mojom::Advertisement> pending_advertisement;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<Advertisement>(std::move(advertisement)),
      pending_advertisement.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(pending_advertisement));
}

void Adapter::OnRegisterAdvertisementError(
    RegisterAdvertisementCallback callback,
    device::BluetoothAdvertisement::ErrorCode error_code) {
  DLOG(ERROR) << "Failed to register advertisement, error code: " << error_code;
  std::move(callback).Run(/*advertisement=*/mojo::NullRemote());
}

void Adapter::OnSetDiscoverable(SetDiscoverableCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void Adapter::OnSetDiscoverableError(SetDiscoverableCallback callback) {
  std::move(callback).Run(/*success=*/false);
}

void Adapter::OnSetName(SetNameCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void Adapter::OnSetNameError(SetNameCallback callback) {
  std::move(callback).Run(/*success=*/false);
}

void Adapter::OnStartDiscoverySession(
    StartDiscoverySessionCallback callback,
    std::unique_ptr<device::BluetoothDiscoverySession> session) {
  mojo::PendingRemote<mojom::DiscoverySession> pending_session;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DiscoverySession>(std::move(session)),
      pending_session.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(pending_session));
}

void Adapter::OnDiscoverySessionError(StartDiscoverySessionCallback callback) {
  std::move(callback).Run(/*session=*/mojo::NullRemote());
}

void Adapter::OnConnectToService(
    int request_id,
    scoped_refptr<device::BluetoothSocket> socket) {
  mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
  MojoResult result =
      mojo::CreateDataPipe(/*options=*/nullptr, receive_pipe_producer_handle,
                           receive_pipe_consumer_handle);
  if (result != MOJO_RESULT_OK) {
    socket->Disconnect(base::BindOnce(&Adapter::OnConnectToServiceError,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      request_id, kMojoReceivingPipeError));
    return;
  }

  mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
  result = mojo::CreateDataPipe(/*options=*/nullptr, send_pipe_producer_handle,
                                send_pipe_consumer_handle);
  if (result != MOJO_RESULT_OK) {
    socket->Disconnect(base::BindOnce(&Adapter::OnConnectToServiceError,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      request_id, kMojoSendingPipeError));
    return;
  }

  mojo::PendingRemote<mojom::Socket> pending_socket;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<Socket>(std::move(socket),
                               std::move(receive_pipe_producer_handle),
                               std::move(send_pipe_consumer_handle)),
      pending_socket.InitWithNewPipeAndPassReceiver());

  mojom::ConnectToServiceResultPtr connect_to_service_result =
      mojom::ConnectToServiceResult::New();
  connect_to_service_result->socket = std::move(pending_socket);
  connect_to_service_result->receive_stream =
      std::move(receive_pipe_consumer_handle);
  connect_to_service_result->send_stream = std::move(send_pipe_producer_handle);
  ExecuteConnectToServiceCallback(request_id,
                                  std::move(connect_to_service_result));

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  RecordConnectToServiceInsecurelyResult(
      ConnectToServiceInsecurelyResult::kSuccess);
#endif
}

void Adapter::OnConnectToServiceError(int request_id,
                                      const std::string& message) {
  DLOG(ERROR) << "Failed to connect to service: '" << message << "'";
  ExecuteConnectToServiceCallback(request_id, /*result=*/nullptr);

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  std::optional<ConnectToServiceInsecurelyResult> result =
      ExtractResultFromErrorString(message);
  if (result) {
    RecordConnectToServiceInsecurelyResult(*result);
  } else if (message == kMojoSendingPipeError) {
    RecordConnectToServiceInsecurelyResult(
        ConnectToServiceInsecurelyResult::kMojoSendingPipeError);
  } else if (message == kMojoReceivingPipeError) {
    RecordConnectToServiceInsecurelyResult(
        ConnectToServiceInsecurelyResult::kMojoReceivingPipeError);
  } else if (message == kCannotConnectToDeviceError) {
    RecordConnectToServiceInsecurelyResult(
        ConnectToServiceInsecurelyResult::kCouldNotConnectError);
  } else {
    RecordConnectToServiceInsecurelyResult(
        ConnectToServiceInsecurelyResult::kUnknownError);
  }
#endif
}

void Adapter::OnConnectToServiceInsecurelyError(
    int request_id,
    const std::string& error_message) {
  DLOG(ERROR) << error_message;
  auto it = connect_to_service_request_map_.find(request_id);
  if (it == connect_to_service_request_map_.end()) {
    DLOG(WARNING) << "Request ID not found, possibly already removed?";
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    RecordConnectToServiceInsecurelyResult(
        ConnectToServiceInsecurelyResult::kDoesNotExistError);
#endif
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  device::BluetoothDevice* device = adapter_->GetDevice(it->second->address);
  DCHECK(device);

  ConnectToServiceFailureReason failure_reason =
      ExtractFailureReasonFromErrorString(error_message);
  // When the local device thinks it's paired with the remote device (IsBonded)
  // and we receive one of these errors when trying to connect, then we're most
  // likely in a state where the remote device doesn't recognize the pairing
  // (half-paired).
  bool is_half_paired_failure =
      device->IsBonded() &&
      (failure_reason == ConnectToServiceFailureReason::kReasonCanceled ||
       failure_reason == ConnectToServiceFailureReason::kReasonRefused ||
       failure_reason == ConnectToServiceFailureReason::kReasonUnknown);

  if (is_half_paired_failure && it->second->should_unbond_on_error) {
    // To recover from the half-paired state, just forget the remote device.
    // This strategy works because the local device will continue attempting to
    // connect. On the next attempt, it will no longer be in the half-paired
    // state.
    DLOG(ERROR) << "Half-paired state detected. Forgetting the device.";
    device->Forget(base::BindOnce(&Adapter::OnConnectToServiceError,
                                  weak_ptr_factory_.GetWeakPtr(), request_id,
                                  error_message),
                   base::BindOnce(&Adapter::OnConnectToServiceError,
                                  weak_ptr_factory_.GetWeakPtr(), request_id,
                                  error_message));
    return;
  }
#endif

  OnConnectToServiceError(request_id, error_message);
}

void Adapter::OnCreateRfcommServiceInsecurely(
    CreateRfcommServiceInsecurelyCallback callback,
    scoped_refptr<device::BluetoothSocket> socket) {
  mojo::PendingRemote<mojom::ServerSocket> pending_server_socket;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ServerSocket>(std::move(socket)),
      pending_server_socket.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(pending_server_socket));
}

void Adapter::OnCreateRfcommServiceInsecurelyError(
    CreateRfcommServiceInsecurelyCallback callback,
    const std::string& message) {
  LOG(ERROR) << "Failed to create service: '" << message << "'";
  std::move(callback).Run(/*server_socket=*/mojo::NullRemote());
}

void Adapter::ExecuteConnectToServiceCallback(
    int request_id,
    mojom::ConnectToServiceResultPtr result) {
  // The request may have already been cancelled if the device was removed.
  auto it = connect_to_service_request_map_.find(request_id);
  if (it != connect_to_service_request_map_.end()) {
    std::move(it->second->callback).Run(std::move(result));
    connect_to_service_request_map_.erase(it);
  }
}

}  // namespace bluetooth
