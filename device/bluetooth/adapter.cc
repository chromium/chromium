// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/adapter.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "device/bluetooth/advertisement.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/device.h"
#include "device/bluetooth/discovery_session.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/public/mojom/connect_result_type_converter.h"
#include "device/bluetooth/server_socket.h"
#include "device/bluetooth/socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "device/bluetooth/bluez/metrics_recorder.h"
#endif

namespace bluetooth {
namespace {

const char kMojoReceivingPipeError[] = "Failed to create receiving DataPipe.";
const char kMojoSendingPipeError[] = "Failed to create sending DataPipe.";
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
const char kCannotConnectToDeviceError[] = "Cannot connect to device.";
#endif

}  // namespace

Adapter::Adapter(scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(std::move(adapter)) {
  adapter_->AddObserver(this);
}

Adapter::~Adapter() {
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

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  device->CreateGattConnection(
      base::BindOnce(&Adapter::OnGattConnected, weak_ptr_factory_.GetWeakPtr(),
                     copyable_callback),
      base::BindOnce(&Adapter::OnConnectError, weak_ptr_factory_.GetWeakPtr(),
                     copyable_callback));
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
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  adapter_info->system_name = adapter_->GetSystemName();
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
                                    RegisterAdvertisementCallback callback) {
  auto advertisement_data =
      std::make_unique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

  auto uuid_list = std::make_unique<device::BluetoothAdvertisement::UUIDList>();
  uuid_list->push_back(service_uuid.value());
  advertisement_data->set_service_uuids(std::move(uuid_list));

  if (!use_scan_response) {
    auto service_data_map =
        std::make_unique<device::BluetoothAdvertisement::ServiceData>();
    service_data_map->emplace(service_uuid.value(), service_data);
    advertisement_data->set_service_data(std::move(service_data_map));
  } else {
    // Require the service uuid to be in 128-bit format.
    DCHECK_EQ(service_uuid.format(),
              device::BluetoothUUID::Format::kFormat128Bit);
    auto scan_response_data_map =
        std::make_unique<device::BluetoothAdvertisement::ScanResponseData>();
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
    scan_response_data_map->emplace(0x16, scan_response_data);
    advertisement_data->set_scan_response_data(
        std::move(scan_response_data_map));
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::BindOnce(&Adapter::OnRegisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Adapter::OnRegisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void Adapter::SetDiscoverable(bool discoverable,
                              SetDiscoverableCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  adapter_->SetDiscoverable(
      discoverable,
      base::BindOnce(&Adapter::OnSetDiscoverable,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Adapter::OnSetDiscoverableError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void Adapter::SetName(const std::string& name, SetNameCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  adapter_->SetName(
      name,
      base::BindOnce(&Adapter::OnSetName, weak_ptr_factory_.GetWeakPtr(),
                     copyable_callback),
      base::BindOnce(&Adapter::OnSetNameError, weak_ptr_factory_.GetWeakPtr(),
                     copyable_callback));
}

void Adapter::StartDiscoverySession(StartDiscoverySessionCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  adapter_->StartDiscoverySession(
      base::BindOnce(&Adapter::OnStartDiscoverySession,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Adapter::OnDiscoverySessionError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void Adapter::ConnectToServiceInsecurely(
    const std::string& address,
    const device::BluetoothUUID& service_uuid,
    ConnectToServiceInsecurelyCallback callback) {
  if (!base::Contains(allowed_uuids_, service_uuid)) {
    std::move(callback).Run(/*result=*/nullptr);
    return;
  }

  auto* device = adapter_->GetDevice(address);
  if (device) {
    OnDeviceFetchedForInsecureServiceConnection(service_uuid,
                                                std::move(callback), device);
    return;
  }

  // This device has neither been discovered, nor has it been paired/connected
  // to previously. Use the ConnectDevice() API, if available, to connect to it.
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  adapter_->ConnectDevice(
      address, /*address_type=*/base::nullopt,
      base::BindOnce(&Adapter::OnDeviceFetchedForInsecureServiceConnection,
                     weak_ptr_factory_.GetWeakPtr(), service_uuid,
                     copyable_callback),
      base::BindOnce(&Adapter::OnConnectToServiceError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback,
                     kCannotConnectToDeviceError));
#else
  OnConnectToServiceError(std::move(callback), "Device does not exist.");
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

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  adapter_->CreateRfcommService(
      service_uuid, service_options,
      base::BindOnce(&Adapter::OnCreateRfcommServiceInsecurely,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Adapter::OnCreateRfcommServiceInsecurelyError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
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
  auto device_info = Device::ConstructDeviceInfoStruct(device);
  for (auto& observer : observers_)
    observer->DeviceAdded(device_info->Clone());
}

void Adapter::DeviceChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device) {
  // Because paired Bluetooth devices never fire device-removed events, we also
  // consider a null RSSI indicative of a device no longer being discoverable.
  // In this scenario, we fail any pending connection requests.
  if (!device->GetInquiryRSSI()) {
    ProcessPendingInsecureServiceConnectionRequest(device->GetAddress(),
                                                   /*device=*/nullptr);
  }

  auto device_info = Device::ConstructDeviceInfoStruct(device);
  for (auto& observer : observers_)
    observer->DeviceChanged(device_info->Clone());
}

void Adapter::DeviceRemoved(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device) {
  ProcessPendingInsecureServiceConnectionRequest(device->GetAddress(),
                                                 /*device=*/nullptr);

  auto device_info = Device::ConstructDeviceInfoStruct(device);
  for (auto& observer : observers_)
    observer->DeviceRemoved(device_info->Clone());
}

void Adapter::GattServicesDiscovered(device::BluetoothAdapter* adapter,
                                     device::BluetoothDevice* device) {
  // GattServicesDiscovered() and IsGattServicesDiscoveryComplete() actually
  // indicate that all services on the remote device, including SDP, are
  // resolved. Once service probing for a device within a cached request (in
  // |pending_connect_to_service_args_|) concludes, attempt socket creation
  // again via OnDeviceFetchedForInsecureServiceConnection().
  if (device->IsGattServicesDiscoveryComplete()) {
    ProcessPendingInsecureServiceConnectionRequest(device->GetAddress(),
                                                   device);
  }
}

void Adapter::AllowConnectionsForUuid(
    const device::BluetoothUUID& service_uuid) {
  allowed_uuids_.emplace(service_uuid);
}

void Adapter::OnDeviceFetchedForInsecureServiceConnection(
    const device::BluetoothUUID& service_uuid,
    ConnectToServiceInsecurelyCallback callback,
    device::BluetoothDevice* device) {
  if (!device) {
    std::move(callback).Run(/*result=*/nullptr);
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
    pending_connect_to_service_args_.emplace_back(
        device->GetAddress(), service_uuid, std::move(callback));
    return;
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  device->ConnectToServiceInsecurely(
      service_uuid,
      base::BindOnce(&Adapter::OnConnectToService,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Adapter::OnConnectToServiceError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void Adapter::ProcessPendingInsecureServiceConnectionRequest(
    const std::string& address,
    device::BluetoothDevice* device) {
  auto it = pending_connect_to_service_args_.begin();
  while (it != pending_connect_to_service_args_.end()) {
    if (address == std::get<0>(*it)) {
      OnDeviceFetchedForInsecureServiceConnection(
          /*service_uuid=*/std::get<1>(*it),
          /*callback=*/std::move(std::get<2>(*it)), device);
      it = pending_connect_to_service_args_.erase(it);
    } else {
      ++it;
    }
  }
}

void Adapter::OnGattConnected(
    ConnectToDeviceCallback callback,
    std::unique_ptr<device::BluetoothGattConnection> connection) {
  mojo::PendingRemote<mojom::Device> device;
  Device::Create(adapter_, std::move(connection),
                 device.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(mojom::ConnectResult::SUCCESS, std::move(device));
}

void Adapter::OnConnectError(
    ConnectToDeviceCallback callback,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  std::move(callback).Run(mojo::ConvertTo<mojom::ConnectResult>(error_code),
                          /*device=*/mojo::NullRemote());
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
    ConnectToServiceInsecurelyCallback callback,
    scoped_refptr<device::BluetoothSocket> socket) {
  mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
  MojoResult result =
      mojo::CreateDataPipe(/*options=*/nullptr, receive_pipe_producer_handle,
                           receive_pipe_consumer_handle);
  if (result != MOJO_RESULT_OK) {
    socket->Disconnect(base::BindOnce(
        &Adapter::OnConnectToServiceError, weak_ptr_factory_.GetWeakPtr(),
        std::move(callback), kMojoReceivingPipeError));
    return;
  }

  mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
  result = mojo::CreateDataPipe(/*options=*/nullptr, send_pipe_producer_handle,
                                send_pipe_consumer_handle);
  if (result != MOJO_RESULT_OK) {
    socket->Disconnect(base::BindOnce(
        &Adapter::OnConnectToServiceError, weak_ptr_factory_.GetWeakPtr(),
        std::move(callback), kMojoSendingPipeError));
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
  std::move(callback).Run(std::move(connect_to_service_result));

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  RecordConnectToServiceInsecurelyResult(
      ConnectToServiceInsecurelyResult::kSuccess);
#endif
}

void Adapter::OnConnectToServiceError(
    ConnectToServiceInsecurelyCallback callback,
    const std::string& message) {
  DLOG(ERROR) << "Failed to connect to service: '" << message << "'";
  std::move(callback).Run(/*result=*/nullptr);

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  base::Optional<ConnectToServiceInsecurelyResult> result =
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

}  // namespace bluetooth
