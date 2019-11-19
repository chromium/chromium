// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mojo/device_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/usb/mock_usb_device.h"
#include "services/device/usb/mock_usb_device_handle.h"
#include "services/device/usb/usb_descriptors.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbIsochronousPacketPtr;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;

namespace usb {

namespace {

MATCHER_P(BufferSizeIs, size, "") {
  return arg->size() == size;
}

class ConfigBuilder {
 public:
  explicit ConfigBuilder(uint8_t value)
      : config_(BuildUsbConfigurationInfoPtr(value, false, false, 0)) {}

  ConfigBuilder& AddInterface(uint8_t interface_number,
                              uint8_t alternate_setting,
                              uint8_t class_code,
                              uint8_t subclass_code,
                              uint8_t protocol_code) {
    config_->interfaces.push_back(
        BuildUsbInterfaceInfoPtr(interface_number, alternate_setting,
                                 class_code, subclass_code, protocol_code));
    return *this;
  }

  mojom::UsbConfigurationInfoPtr Build() { return std::move(config_); }

 private:
  mojom::UsbConfigurationInfoPtr config_;

  DISALLOW_COPY_AND_ASSIGN(ConfigBuilder);
};

void ExpectOpenAndThen(mojom::UsbOpenDeviceError expected,
                       const base::Closure& continuation,
                       mojom::UsbOpenDeviceError error) {
  EXPECT_EQ(expected, error);
  continuation.Run();
}

void ExpectResultAndThen(bool expected_result,
                         const base::Closure& continuation,
                         bool actual_result) {
  EXPECT_EQ(expected_result, actual_result);
  continuation.Run();
}

void ExpectTransferInAndThen(mojom::UsbTransferStatus expected_status,
                             const std::vector<uint8_t>& expected_bytes,
                             const base::Closure& continuation,
                             mojom::UsbTransferStatus actual_status,
                             const std::vector<uint8_t>& actual_bytes) {
  EXPECT_EQ(expected_status, actual_status);
  ASSERT_EQ(expected_bytes.size(), actual_bytes.size());
  for (size_t i = 0; i < actual_bytes.size(); ++i) {
    EXPECT_EQ(expected_bytes[i], actual_bytes[i])
        << "Contents differ at index: " << i;
  }
  continuation.Run();
}

void ExpectPacketsOutAndThen(
    const std::vector<uint32_t>& expected_packets,
    const base::Closure& continuation,
    std::vector<UsbIsochronousPacketPtr> actual_packets) {
  ASSERT_EQ(expected_packets.size(), actual_packets.size());
  for (size_t i = 0; i < expected_packets.size(); ++i) {
    EXPECT_EQ(expected_packets[i], actual_packets[i]->transferred_length)
        << "Packet lengths differ at index: " << i;
    EXPECT_EQ(mojom::UsbTransferStatus::COMPLETED, actual_packets[i]->status)
        << "Packet at index " << i << " not completed.";
  }
  continuation.Run();
}

void ExpectPacketsInAndThen(
    const std::vector<uint8_t>& expected_bytes,
    const std::vector<uint32_t>& expected_packets,
    const base::Closure& continuation,
    const std::vector<uint8_t>& actual_bytes,
    std::vector<UsbIsochronousPacketPtr> actual_packets) {
  ASSERT_EQ(expected_packets.size(), actual_packets.size());
  for (size_t i = 0; i < expected_packets.size(); ++i) {
    EXPECT_EQ(expected_packets[i], actual_packets[i]->transferred_length)
        << "Packet lengths differ at index: " << i;
    EXPECT_EQ(mojom::UsbTransferStatus::COMPLETED, actual_packets[i]->status)
        << "Packet at index " << i << " not completed.";
  }
  ASSERT_EQ(expected_bytes.size(), actual_bytes.size());
  for (size_t i = 0; i < actual_bytes.size(); ++i) {
    EXPECT_EQ(expected_bytes[i], actual_bytes[i])
        << "Contents differ at index: " << i;
  }
  continuation.Run();
}

void ExpectTransferStatusAndThen(mojom::UsbTransferStatus expected_status,
                                 const base::Closure& continuation,
                                 mojom::UsbTransferStatus actual_status) {
  EXPECT_EQ(expected_status, actual_status);
  continuation.Run();
}

class MockUsbDeviceClient : public mojom::UsbDeviceClient {
 public:
  MockUsbDeviceClient() = default;
  ~MockUsbDeviceClient() override = default;

  mojo::PendingRemote<mojom::UsbDeviceClient> CreateInterfacePtrAndBind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD0(OnDeviceOpened, void());
  MOCK_METHOD0(OnDeviceClosed, void());

 private:
  mojo::Receiver<mojom::UsbDeviceClient> receiver_{this};
};

class USBDeviceImplTest : public testing::Test {
 public:
  USBDeviceImplTest() : is_device_open_(false), allow_reset_(false) {}

  ~USBDeviceImplTest() override = default;

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  MockUsbDevice& mock_device() { return *mock_device_.get(); }
  bool is_device_open() const { return is_device_open_; }
  MockUsbDeviceHandle& mock_handle() { return *mock_handle_.get(); }

  void set_allow_reset(bool allow_reset) { allow_reset_ = allow_reset; }

  // Creates a mock device and binds a Device proxy to a Device service impl
  // wrapping the mock device.
  mojo::Remote<mojom::UsbDevice> GetMockDeviceProxy(
      uint16_t vendor_id,
      uint16_t product_id,
      const std::string& manufacturer,
      const std::string& product,
      const std::string& serial,
      mojo::PendingRemote<mojom::UsbDeviceClient> client) {
    mock_device_ =
        new MockUsbDevice(vendor_id, product_id, manufacturer, product, serial);
    mock_handle_ = new MockUsbDeviceHandle(mock_device_.get());

    mojo::Remote<mojom::UsbDevice> proxy;
    DeviceImpl::Create(mock_device_, proxy.BindNewPipeAndPassReceiver(),
                       std::move(client));

    // Set up mock handle calls to respond based on mock device configs
    // established by the test.
    ON_CALL(mock_device(), OpenInternal(_))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::OpenMockHandle));
    ON_CALL(mock_handle(), Close())
        .WillByDefault(Invoke(this, &USBDeviceImplTest::CloseMockHandle));
    ON_CALL(mock_handle(), SetConfigurationInternal(_, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::SetConfiguration));
    ON_CALL(mock_handle(), ClaimInterfaceInternal(_, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ClaimInterface));
    ON_CALL(mock_handle(), ReleaseInterfaceInternal(_, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ReleaseInterface));
    ON_CALL(mock_handle(), SetInterfaceAlternateSettingInternal(_, _, _))
        .WillByDefault(
            Invoke(this, &USBDeviceImplTest::SetInterfaceAlternateSetting));
    ON_CALL(mock_handle(), ResetDeviceInternal(_))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ResetDevice));
    ON_CALL(mock_handle(), ControlTransferInternal(_, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::ControlTransfer));
    ON_CALL(mock_handle(), GenericTransferInternal(_, _, _, _, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::GenericTransfer));
    ON_CALL(mock_handle(), IsochronousTransferInInternal(_, _, _, _))
        .WillByDefault(Invoke(this, &USBDeviceImplTest::IsochronousTransferIn));
    ON_CALL(mock_handle(), IsochronousTransferOutInternal(_, _, _, _, _))
        .WillByDefault(
            Invoke(this, &USBDeviceImplTest::IsochronousTransferOut));

    return proxy;
  }

  mojo::Remote<mojom::UsbDevice> GetMockDeviceProxy(
      mojo::PendingRemote<mojom::UsbDeviceClient> client) {
    return GetMockDeviceProxy(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF",
                              std::move(client));
  }

  mojo::Remote<mojom::UsbDevice> GetMockDeviceProxy() {
    return GetMockDeviceProxy(mojo::NullRemote());
  }

  void AddMockConfig(mojom::UsbConfigurationInfoPtr config) {
    DCHECK(!base::Contains(mock_configs_, config->configuration_value));
    mock_configs_.insert(
        std::make_pair(config->configuration_value, config.get()));
    mock_device_->AddMockConfig(std::move(config));
  }

  void AddMockInboundData(const std::vector<uint8_t>& data) {
    mock_inbound_data_.push(data);
  }

  void AddMockInboundPackets(const std::vector<uint8_t>& data,
                             std::vector<UsbIsochronousPacketPtr> packets) {
    mock_inbound_data_.push(data);
    mock_inbound_packets_.push(std::move(packets));
  }

  void AddMockOutboundData(const std::vector<uint8_t>& data) {
    mock_outbound_data_.push(data);
  }

  void AddMockOutboundPackets(const std::vector<uint8_t>& data,
                              std::vector<UsbIsochronousPacketPtr> packets) {
    mock_outbound_data_.push(data);
    mock_outbound_packets_.push(std::move(packets));
  }

 private:
  void OpenMockHandle(UsbDevice::OpenCallback& callback) {
    EXPECT_FALSE(is_device_open_);
    is_device_open_ = true;
    std::move(callback).Run(mock_handle_);
  }

  void CloseMockHandle() {
    EXPECT_TRUE(is_device_open_);
    is_device_open_ = false;
  }

  void SetConfiguration(uint8_t value,
                        UsbDeviceHandle::ResultCallback& callback) {
    if (mock_configs_.find(value) != mock_configs_.end()) {
      mock_device_->ActiveConfigurationChanged(value);
      std::move(callback).Run(true);
    } else {
      std::move(callback).Run(false);
    }
  }

  void ClaimInterface(uint8_t interface_number,
                      UsbDeviceHandle::ResultCallback& callback) {
    for (const auto& config : mock_configs_) {
      for (const auto& interface : config.second->interfaces) {
        if (interface->interface_number == interface_number) {
          claimed_interfaces_.insert(interface_number);
          std::move(callback).Run(true);
          return;
        }
      }
    }
    std::move(callback).Run(false);
  }

  void ReleaseInterface(uint8_t interface_number,
                        UsbDeviceHandle::ResultCallback& callback) {
    if (base::Contains(claimed_interfaces_, interface_number)) {
      claimed_interfaces_.erase(interface_number);
      std::move(callback).Run(true);
    } else {
      std::move(callback).Run(false);
    }
  }

  void SetInterfaceAlternateSetting(uint8_t interface_number,
                                    uint8_t alternate_setting,
                                    UsbDeviceHandle::ResultCallback& callback) {
    for (const auto& config : mock_configs_) {
      CombinedInterfaceInfo interface = FindInterfaceInfoFromConfig(
          config.second, interface_number, alternate_setting);
      if (interface.IsValid()) {
        std::move(callback).Run(true);
        return;
      }
    }
    std::move(callback).Run(false);
  }

  void ResetDevice(UsbDeviceHandle::ResultCallback& callback) {
    std::move(callback).Run(allow_reset_);
  }

  void InboundTransfer(UsbDeviceHandle::TransferCallback callback) {
    ASSERT_GE(mock_inbound_data_.size(), 1u);
    const std::vector<uint8_t>& bytes = mock_inbound_data_.front();
    size_t length = bytes.size();
    auto buffer = base::MakeRefCounted<base::RefCountedBytes>(bytes);
    mock_inbound_data_.pop();
    std::move(callback).Run(UsbTransferStatus::COMPLETED, buffer, length);
  }

  void OutboundTransfer(scoped_refptr<base::RefCountedBytes> buffer,
                        UsbDeviceHandle::TransferCallback callback) {
    ASSERT_GE(mock_outbound_data_.size(), 1u);
    const std::vector<uint8_t>& bytes = mock_outbound_data_.front();
    ASSERT_EQ(bytes.size(), buffer->size());
    for (size_t i = 0; i < bytes.size(); ++i) {
      EXPECT_EQ(bytes[i], buffer->front()[i])
          << "Contents differ at index: " << i;
    }
    mock_outbound_data_.pop();
    std::move(callback).Run(UsbTransferStatus::COMPLETED, buffer,
                            buffer->size());
  }

  void ControlTransfer(UsbTransferDirection direction,
                       UsbControlTransferType request_type,
                       UsbControlTransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       UsbDeviceHandle::TransferCallback& callback) {
    if (direction == UsbTransferDirection::INBOUND)
      InboundTransfer(std::move(callback));
    else
      OutboundTransfer(buffer, std::move(callback));
  }

  void GenericTransfer(UsbTransferDirection direction,
                       uint8_t endpoint,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       UsbDeviceHandle::TransferCallback& callback) {
    if (direction == UsbTransferDirection::INBOUND)
      InboundTransfer(std::move(callback));
    else
      OutboundTransfer(buffer, std::move(callback));
  }

  void IsochronousTransferIn(
      uint8_t endpoint_number,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      UsbDeviceHandle::IsochronousTransferCallback& callback) {
    ASSERT_FALSE(mock_inbound_data_.empty());
    const std::vector<uint8_t>& bytes = mock_inbound_data_.front();
    auto buffer = base::MakeRefCounted<base::RefCountedBytes>(bytes);
    mock_inbound_data_.pop();

    ASSERT_FALSE(mock_inbound_packets_.empty());
    std::vector<UsbIsochronousPacketPtr> packets =
        std::move(mock_inbound_packets_.front());
    mock_inbound_packets_.pop();

    ASSERT_EQ(packets.size(), packet_lengths.size());
    for (size_t i = 0; i < packets.size(); ++i) {
      EXPECT_EQ(packets[i]->length, packet_lengths[i])
          << "Packet lengths differ at index: " << i;
    }

    std::move(callback).Run(buffer, std::move(packets));
  }

  void IsochronousTransferOut(
      uint8_t endpoint_number,
      scoped_refptr<base::RefCountedBytes> buffer,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      UsbDeviceHandle::IsochronousTransferCallback& callback) {
    ASSERT_FALSE(mock_outbound_data_.empty());
    const std::vector<uint8_t>& bytes = mock_outbound_data_.front();
    size_t length =
        std::accumulate(packet_lengths.begin(), packet_lengths.end(), 0u);
    ASSERT_EQ(bytes.size(), length);
    for (size_t i = 0; i < length; ++i) {
      EXPECT_EQ(bytes[i], buffer->front()[i])
          << "Contents differ at index: " << i;
    }
    mock_outbound_data_.pop();

    ASSERT_FALSE(mock_outbound_packets_.empty());
    std::vector<UsbIsochronousPacketPtr> packets =
        std::move(mock_outbound_packets_.front());
    mock_outbound_packets_.pop();

    ASSERT_EQ(packets.size(), packet_lengths.size());
    for (size_t i = 0; i < packets.size(); ++i) {
      EXPECT_EQ(packets[i]->length, packet_lengths[i])
          << "Packet lengths differ at index: " << i;
    }

    std::move(callback).Run(buffer, std::move(packets));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<MockUsbDevice> mock_device_;
  scoped_refptr<MockUsbDeviceHandle> mock_handle_;
  bool is_device_open_;
  bool allow_reset_;

  std::map<uint8_t, const mojom::UsbConfigurationInfo*> mock_configs_;

  base::queue<std::vector<uint8_t>> mock_inbound_data_;
  base::queue<std::vector<uint8_t>> mock_outbound_data_;
  base::queue<std::vector<UsbIsochronousPacketPtr>> mock_inbound_packets_;
  base::queue<std::vector<UsbIsochronousPacketPtr>> mock_outbound_packets_;

  std::set<uint8_t> claimed_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(USBDeviceImplTest);
};

}  // namespace

TEST_F(USBDeviceImplTest, Disconnect) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  base::RunLoop loop;
  device.set_disconnect_handler(loop.QuitClosure());
  mock_device().NotifyDeviceRemoved();
  loop.Run();
}

TEST_F(USBDeviceImplTest, Open) {
  MockUsbDeviceClient device_client;
  mojo::Remote<mojom::UsbDevice> device =
      GetMockDeviceProxy(device_client.CreateInterfacePtrAndBind());

  EXPECT_FALSE(is_device_open());

  EXPECT_CALL(mock_device(), OpenInternal(_));
  EXPECT_CALL(device_client, OnDeviceOpened());

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(&ExpectOpenAndThen,
                                mojom::UsbOpenDeviceError::ALREADY_OPEN,
                                loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
  EXPECT_CALL(device_client, OnDeviceClosed());

  device.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(USBDeviceImplTest, OpenFailure) {
  MockUsbDeviceClient device_client;
  mojo::Remote<mojom::UsbDevice> device =
      GetMockDeviceProxy(device_client.CreateInterfacePtrAndBind());

  EXPECT_CALL(mock_device(), OpenInternal(_))
      .WillOnce(Invoke([](UsbDevice::OpenCallback& callback) {
        std::move(callback).Run(nullptr);
      }));
  EXPECT_CALL(device_client, OnDeviceOpened()).Times(0);
  EXPECT_CALL(device_client, OnDeviceClosed()).Times(0);

  base::RunLoop loop;
  device->Open(base::BindOnce(&ExpectOpenAndThen,
                              mojom::UsbOpenDeviceError::ACCESS_DENIED,
                              loop.QuitClosure()));
  loop.Run();
}

TEST_F(USBDeviceImplTest, OpenDelayedFailure) {
  MockUsbDeviceClient device_client;
  mojo::Remote<mojom::UsbDevice> device =
      GetMockDeviceProxy(device_client.CreateInterfacePtrAndBind());

  UsbDevice::OpenCallback saved_callback;
  EXPECT_CALL(mock_device(), OpenInternal(_))
      .WillOnce(Invoke([&saved_callback](UsbDevice::OpenCallback& callback) {
        saved_callback = std::move(callback);
      }));
  EXPECT_CALL(device_client, OnDeviceOpened()).Times(0);
  EXPECT_CALL(device_client, OnDeviceClosed()).Times(0);

  device->Open(
      base::BindOnce([](mojom::UsbOpenDeviceError result) { NOTREACHED(); }));
  device.reset();
  base::RunLoop().RunUntilIdle();

  std::move(saved_callback).Run(nullptr);
}

TEST_F(USBDeviceImplTest, Close) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  EXPECT_FALSE(is_device_open());

  EXPECT_CALL(mock_device(), OpenInternal(_));

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());

  {
    base::RunLoop loop;
    device->Close(loop.QuitClosure());
    loop.Run();
  }

  EXPECT_FALSE(is_device_open());
}

TEST_F(USBDeviceImplTest, SetInvalidConfiguration) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), OpenInternal(_));

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), SetConfigurationInternal(42, _));

  {
    // SetConfiguration should fail because 42 is not a valid mock
    // configuration.
    base::RunLoop loop;
    device->SetConfiguration(
        42, base::BindOnce(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, SetValidConfiguration) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), OpenInternal(_));

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), SetConfigurationInternal(42, _));

  AddMockConfig(ConfigBuilder(42).Build());

  {
    // SetConfiguration should succeed because 42 is a valid mock configuration.
    base::RunLoop loop;
    device->SetConfiguration(
        42, base::BindOnce(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

// Verify that the result of Reset() reflects the underlying UsbDeviceHandle's
// ResetDevice() result.
TEST_F(USBDeviceImplTest, Reset) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), OpenInternal(_));

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ResetDeviceInternal(_));

  set_allow_reset(true);

  {
    base::RunLoop loop;
    device->Reset(
        base::BindOnce(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ResetDeviceInternal(_));

  set_allow_reset(false);

  {
    base::RunLoop loop;
    device->Reset(
        base::BindOnce(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, ClaimAndReleaseInterface) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), OpenInternal(_));

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  // Now add a mock interface #1.
  AddMockConfig(ConfigBuilder(1).AddInterface(1, 0, 1, 2, 3).Build());

  EXPECT_CALL(mock_handle(), SetConfigurationInternal(1, _));

  {
    base::RunLoop loop;
    device->SetConfiguration(
        1, base::BindOnce(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  {
    // Try to claim an invalid interface and expect failure.
    base::RunLoop loop;
    device->ClaimInterface(
        2, base::BindOnce(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ClaimInterfaceInternal(1, _));

  {
    base::RunLoop loop;
    device->ClaimInterface(
        1, base::BindOnce(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ReleaseInterfaceInternal(2, _));

  {
    // Releasing a non-existent interface should fail.
    base::RunLoop loop;
    device->ReleaseInterface(
        2, base::BindOnce(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), ReleaseInterfaceInternal(1, _));

  {
    // Now this should release the claimed interface and close the handle.
    base::RunLoop loop;
    device->ReleaseInterface(
        1, base::BindOnce(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, SetInterfaceAlternateSetting) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), OpenInternal(_));

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  AddMockConfig(ConfigBuilder(1)
                    .AddInterface(1, 0, 1, 2, 3)
                    .AddInterface(1, 42, 1, 2, 3)
                    .AddInterface(2, 0, 1, 2, 3)
                    .Build());

  EXPECT_CALL(mock_handle(), SetInterfaceAlternateSettingInternal(1, 42, _));

  {
    base::RunLoop loop;
    device->SetInterfaceAlternateSetting(
        1, 42, base::BindOnce(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), SetInterfaceAlternateSettingInternal(1, 100, _));

  {
    base::RunLoop loop;
    device->SetInterfaceAlternateSetting(
        1, 100,
        base::BindOnce(&ExpectResultAndThen, false, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, ControlTransfer) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), OpenInternal(_));

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  AddMockConfig(ConfigBuilder(1).AddInterface(7, 0, 1, 2, 3).Build());

  EXPECT_CALL(mock_handle(), SetConfigurationInternal(1, _));

  {
    base::RunLoop loop;
    device->SetConfiguration(
        1, base::BindOnce(&ExpectResultAndThen, true, loop.QuitClosure()));
    loop.Run();
  }

  std::vector<uint8_t> fake_data;
  fake_data.push_back(41);
  fake_data.push_back(42);
  fake_data.push_back(43);

  AddMockInboundData(fake_data);

  EXPECT_CALL(mock_handle(),
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::DEVICE, 5, 6,
                                      7, _, 0, _));

  {
    auto params = mojom::UsbControlTransferParams::New();
    params->type = UsbControlTransferType::STANDARD;
    params->recipient = UsbControlTransferRecipient::DEVICE;
    params->request = 5;
    params->value = 6;
    params->index = 7;
    base::RunLoop loop;
    device->ControlTransferIn(
        std::move(params), static_cast<uint32_t>(fake_data.size()), 0,
        base::BindOnce(&ExpectTransferInAndThen,
                       mojom::UsbTransferStatus::COMPLETED, fake_data,
                       loop.QuitClosure()));
    loop.Run();
  }

  AddMockOutboundData(fake_data);

  EXPECT_CALL(mock_handle(),
              ControlTransferInternal(UsbTransferDirection::OUTBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::INTERFACE, 5,
                                      6, 7, _, 0, _));

  {
    auto params = mojom::UsbControlTransferParams::New();
    params->type = UsbControlTransferType::STANDARD;
    params->recipient = UsbControlTransferRecipient::INTERFACE;
    params->request = 5;
    params->value = 6;
    params->index = 7;
    base::RunLoop loop;
    device->ControlTransferOut(
        std::move(params), fake_data, 0,
        base::BindOnce(&ExpectTransferStatusAndThen,
                       mojom::UsbTransferStatus::COMPLETED,
                       loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, GenericTransfer) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), OpenInternal(_));

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  std::string message1 = "say hello please";
  std::vector<uint8_t> fake_outbound_data(message1.size());
  std::copy(message1.begin(), message1.end(), fake_outbound_data.begin());

  std::string message2 = "hello world!";
  std::vector<uint8_t> fake_inbound_data(message2.size());
  std::copy(message2.begin(), message2.end(), fake_inbound_data.begin());

  AddMockConfig(ConfigBuilder(1).AddInterface(7, 0, 1, 2, 3).Build());
  AddMockOutboundData(fake_outbound_data);
  AddMockInboundData(fake_inbound_data);

  EXPECT_CALL(
      mock_handle(),
      GenericTransferInternal(UsbTransferDirection::OUTBOUND, 0x01,
                              BufferSizeIs(fake_outbound_data.size()), 0, _));

  {
    base::RunLoop loop;
    device->GenericTransferOut(
        1, fake_outbound_data, 0,
        base::BindOnce(&ExpectTransferStatusAndThen,
                       mojom::UsbTransferStatus::COMPLETED,
                       loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), GenericTransferInternal(
                                 UsbTransferDirection::INBOUND, 0x81,
                                 BufferSizeIs(fake_inbound_data.size()), 0, _));

  {
    base::RunLoop loop;
    device->GenericTransferIn(
        1, fake_inbound_data.size(), 0,
        base::BindOnce(&ExpectTransferInAndThen,
                       mojom::UsbTransferStatus::COMPLETED, fake_inbound_data,
                       loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

TEST_F(USBDeviceImplTest, IsochronousTransfer) {
  mojo::Remote<mojom::UsbDevice> device = GetMockDeviceProxy();

  EXPECT_CALL(mock_device(), OpenInternal(_));

  {
    base::RunLoop loop;
    device->Open(base::BindOnce(
        &ExpectOpenAndThen, mojom::UsbOpenDeviceError::OK, loop.QuitClosure()));
    loop.Run();
  }

  std::vector<UsbIsochronousPacketPtr> fake_packets_in(4);
  for (size_t i = 0; i < fake_packets_in.size(); ++i) {
    fake_packets_in[i] = mojom::UsbIsochronousPacket::New();
    fake_packets_in[i]->length = 8;
    fake_packets_in[i]->transferred_length = 8;
    fake_packets_in[i]->status = UsbTransferStatus::COMPLETED;
  }
  std::vector<UsbIsochronousPacketPtr> fake_packets_out;
  for (const auto& packet : fake_packets_in) {
    fake_packets_out.push_back(packet->Clone());
  }
  std::vector<uint32_t> fake_packet_lengths(4, 8);

  std::vector<uint32_t> expected_transferred_lengths(4, 8);

  std::string outbound_data = "aaaaaaaabbbbbbbbccccccccdddddddd";
  std::vector<uint8_t> fake_outbound_data(outbound_data.size());
  std::copy(outbound_data.begin(), outbound_data.end(),
            fake_outbound_data.begin());

  std::string inbound_data = "ddddddddccccccccbbbbbbbbaaaaaaaa";
  std::vector<uint8_t> fake_inbound_data(inbound_data.size());
  std::copy(inbound_data.begin(), inbound_data.end(),
            fake_inbound_data.begin());

  AddMockConfig(ConfigBuilder(1).AddInterface(7, 0, 1, 2, 3).Build());
  AddMockOutboundPackets(fake_outbound_data, std::move(fake_packets_in));
  AddMockInboundPackets(fake_inbound_data, std::move(fake_packets_out));

  EXPECT_CALL(mock_handle(), IsochronousTransferOutInternal(
                                 0x01, _, fake_packet_lengths, 0, _));

  {
    base::RunLoop loop;
    device->IsochronousTransferOut(
        1, fake_outbound_data, fake_packet_lengths, 0,
        base::BindOnce(&ExpectPacketsOutAndThen, expected_transferred_lengths,
                       loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(),
              IsochronousTransferInInternal(0x81, fake_packet_lengths, 0, _));

  {
    base::RunLoop loop;
    device->IsochronousTransferIn(
        1, fake_packet_lengths, 0,
        base::BindOnce(&ExpectPacketsInAndThen, fake_inbound_data,
                       expected_transferred_lengths, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_CALL(mock_handle(), Close());
}

}  // namespace usb
}  // namespace device
