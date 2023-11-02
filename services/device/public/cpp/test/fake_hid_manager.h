// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_HID_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_HID_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

class FakeHidConnection : public mojom::HidConnection {
 public:
  FakeHidConnection(
      mojom::HidDeviceInfoPtr device,
      mojo::PendingReceiver<mojom::HidConnection> receiver,
      mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
      mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
      bool allow_fido_reports);
  FakeHidConnection(FakeHidConnection&) = delete;
  FakeHidConnection& operator=(FakeHidConnection&) = delete;
  ~FakeHidConnection() override;

  // mojom::HidConnection implementation:
  void Read(ReadCallback callback) override;
  void Write(uint8_t report_id,
             const std::vector<uint8_t>& buffer,
             WriteCallback callback) override;
  void GetFeatureReport(uint8_t report_id,
                        GetFeatureReportCallback callback) override;

  void SendFeatureReport(uint8_t report_id,
                         const std::vector<uint8_t>& buffer,
                         SendFeatureReportCallback callback) override;

 private:
  mojo::Receiver<mojom::HidConnection> receiver_;
  mojom::HidDeviceInfoPtr device_;
  mojo::Remote<mojom::HidConnectionClient> client_;
  mojo::Remote<mojom::HidConnectionWatcher> watcher_;
  bool allow_fido_reports_;
};

class FakeHidManager : public mojom::HidManager {
 public:
  FakeHidManager();
  FakeHidManager(FakeHidManager&) = delete;
  FakeHidManager& operator=(FakeHidManager&) = delete;
  ~FakeHidManager() override;

  void Bind(mojo::PendingReceiver<mojom::HidManager> receiver);

  // mojom::HidManager implementation:
  void AddReceiver(mojo::PendingReceiver<mojom::HidManager> receiver) override;
  void GetDevicesAndSetClient(
      mojo::PendingAssociatedRemote<mojom::HidManagerClient> client,
      GetDevicesCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void Connect(
      const std::string& device_guid,
      mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
      mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
      bool allow_protected_reports,
      bool allow_fido_reports,
      ConnectCallback callback) override;

  mojom::HidDeviceInfoPtr CreateAndAddDevice(
      const std::string& physical_device_id,
      uint16_t vendor_id,
      uint16_t product_id,
      const std::string& product_name,
      const std::string& serial_number,
      mojom::HidBusType bus_type);
  mojom::HidDeviceInfoPtr CreateAndAddDeviceWithTopLevelUsage(
      const std::string& physical_device_id,
      uint16_t vendor_id,
      uint16_t product_id,
      const std::string& product_name,
      const std::string& serial_number,
      mojom::HidBusType bus_type,
      uint16_t usage_page,
      uint16_t usage);
  void AddDevice(mojom::HidDeviceInfoPtr device);
  void RemoveDevice(const std::string& guid);
  void ChangeDevice(mojom::HidDeviceInfoPtr device);
  void SimulateConnectionError();

 private:
  std::map<std::string, mojom::HidDeviceInfoPtr> devices_;
  mojo::AssociatedRemoteSet<mojom::HidManagerClient> clients_;
  mojo::ReceiverSet<mojom::HidManager> receivers_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_HID_MANAGER_H_
