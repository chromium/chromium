// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_HID_FAKE_HID_IMPL_FOR_TESTING_H_
#define DEVICE_FIDO_HID_FAKE_HID_IMPL_FOR_TESTING_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "device/fido/fido_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockFidoHidConnection : public device::mojom::HidConnection {
 public:
  MockFidoHidConnection(
      device::mojom::HidDeviceInfoPtr device,
      mojo::PendingReceiver<device::mojom::HidConnection> receiver,
      std::array<uint8_t, 4> connection_channel_id);
  MockFidoHidConnection(MockFidoHidConnection&) = delete;
  MockFidoHidConnection& operator=(MockFidoHidConnection&) = delete;
  ~MockFidoHidConnection() override;

  MOCK_METHOD1(ReadPtr, void(ReadCallback* callback));
  MOCK_METHOD3(WritePtr,
               void(uint8_t report_id,
                    const std::vector<uint8_t>& buffer,
                    WriteCallback* callback));

  void Read(ReadCallback callback) override;

  void Write(uint8_t report_id,
             const std::vector<uint8_t>& buffer,
             WriteCallback callback) override;

  void GetFeatureReport(uint8_t report_id,
                        GetFeatureReportCallback callback) override;
  void SendFeatureReport(uint8_t report_id,
                         const std::vector<uint8_t>& buffer,
                         SendFeatureReportCallback callback) override;
  void SetNonce(base::span<uint8_t const> nonce);

  void ExpectWriteHidInit();
  void ExpectWriteHidInit(const testing::Sequence& sequence);
  void ExpectHidWriteWithCommand(FidoHidDeviceCommand cmd);
  void ExpectHidWriteWithCommand(const testing::Sequence& sequence,
                                 FidoHidDeviceCommand cmd);
  void ExpectReadAndReplyWith(const testing::Sequence& sequence,
                              std::vector<uint8_t> response);

  const std::array<uint8_t, 4>& connection_channel_id() const {
    return connection_channel_id_;
  }
  const std::vector<uint8_t>& nonce() const { return nonce_; }

 private:
  mojo::Receiver<device::mojom::HidConnection> receiver_;
  device::mojom::HidDeviceInfoPtr device_;
  std::vector<uint8_t> nonce_;
  std::array<uint8_t, 4> connection_channel_id_;
};

class FakeFidoHidConnection : public device::mojom::HidConnection {
 public:
  explicit FakeFidoHidConnection(device::mojom::HidDeviceInfoPtr device);
  FakeFidoHidConnection(FakeFidoHidConnection&) = delete;
  FakeFidoHidConnection& operator=(FakeFidoHidConnection&) = delete;
  ~FakeFidoHidConnection() override;

  // device::mojom::HidConnection implemenation:
  void Read(ReadCallback callback) override;
  void Write(uint8_t report_id,
             const std::vector<uint8_t>& buffer,
             WriteCallback callback) override;
  void GetFeatureReport(uint8_t report_id,
                        GetFeatureReportCallback callback) override;
  void SendFeatureReport(uint8_t report_id,
                         const std::vector<uint8_t>& buffer,
                         SendFeatureReportCallback callback) override;

  static bool mock_connection_error_;

 private:
  device::mojom::HidDeviceInfoPtr device_;
};

class FakeFidoHidManager : public device::mojom::HidManager {
 public:
  FakeFidoHidManager();
  FakeFidoHidManager(FakeFidoHidManager&) = delete;
  FakeFidoHidManager& operator=(FakeFidoHidManager&) = delete;
  ~FakeFidoHidManager() override;

  // Invoke AddDevice with a device info struct that mirrors a FIDO USB device.
  void AddFidoHidDevice(std::string guid);

  // device::mojom::HidManager implementation:
  void GetDevicesAndSetClient(
      mojo::PendingAssociatedRemote<device::mojom::HidManagerClient> client,
      GetDevicesCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void Connect(
      const std::string& device_guid,
      mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
      mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
      bool allow_protected_reports,
      bool allow_fido_reports,
      ConnectCallback callback) override;
  void AddReceiver(
      mojo::PendingReceiver<device::mojom::HidManager> receiver) override;
  void AddDevice(device::mojom::HidDeviceInfoPtr device);
  void AddDeviceAndSetConnection(
      device::mojom::HidDeviceInfoPtr device,
      mojo::PendingRemote<device::mojom::HidConnection> connection);
  void RemoveDevice(const std::string device_guid);
  void ChangeDevice(device::mojom::HidDeviceInfoPtr device);

 private:
  std::map<std::string, device::mojom::HidDeviceInfoPtr> devices_;
  std::map<std::string, mojo::PendingRemote<device::mojom::HidConnection>>
      connections_;
  mojo::AssociatedRemoteSet<device::mojom::HidManagerClient> clients_;
  mojo::ReceiverSet<device::mojom::HidManager> receivers_;
};

// ScopedFakeFidoHidManager automatically binds itself to the device service for
// the duration of its lifetime.
class ScopedFakeFidoHidManager : public FakeFidoHidManager {
 public:
  ScopedFakeFidoHidManager();
  ScopedFakeFidoHidManager(ScopedFakeFidoHidManager&) = delete;
  ScopedFakeFidoHidManager& operator=(ScopedFakeFidoHidManager&) = delete;
  ~ScopedFakeFidoHidManager() override;
};

}  // namespace device

#endif  // DEVICE_FIDO_HID_FAKE_HID_IMPL_FOR_TESTING_H_
