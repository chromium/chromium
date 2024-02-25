// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <numeric>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/cpp/test/mock_usb_mojo_device.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace extensions {

namespace {

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;

using ::device::mojom::UsbControlTransferParams;
using ::device::mojom::UsbControlTransferRecipient;
using ::device::mojom::UsbControlTransferType;
using ::device::mojom::UsbIsochronousPacket;
using ::device::mojom::UsbTransferDirection;
using ::device::mojom::UsbTransferStatus;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

ACTION_TEMPLATE(BuildIsochronousTransferReturnValue,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(transferred_length, success_packets)) {
  const std::vector<uint32_t>& packet_lengths = std::get<k>(args);
  std::vector<UsbIsochronousPacket> packets(packet_lengths.size());
  for (size_t i = 0; i < packets.size(); ++i) {
    packets[i].length = packet_lengths[i];
    if (i < success_packets) {
      packets[i].transferred_length = transferred_length;
      packets[i].status = UsbTransferStatus::COMPLETED;
    } else {
      packets[i].transferred_length = 0;
      packets[i].status = UsbTransferStatus::TRANSFER_ERROR;
    }
  }
  return packets;
}

MATCHER_P(BufferSizeIs, size, "") {
  return arg.size() == size;
}

MATCHER_P(UsbControlTransferParamsEquals, expected, "") {
  return arg->Equals(expected);
}

struct UsbOpenDeviceSuccess {
  void operator()(device::mojom::UsbDevice::OpenCallback callback) {
    std::move(callback).Run(device::mojom::UsbOpenDeviceResult::NewSuccess(
        device::mojom::UsbOpenDeviceSuccess::OK));
  }
};

class TestDevicePermissionsPrompt
    : public DevicePermissionsPrompt,
      public DevicePermissionsPrompt::Prompt::Observer {
 public:
  explicit TestDevicePermissionsPrompt(content::WebContents* web_contents)
      : DevicePermissionsPrompt(web_contents) {}

  void ShowDialog() override { prompt()->SetObserver(this); }

  void OnDevicesInitialized() override {
    for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
      prompt()->GrantDevicePermission(i);
      if (!prompt()->multiple()) {
        break;
      }
    }

    prompt()->Dismissed();
  }

  void OnDeviceAdded(size_t index, const std::u16string& device_name) override {
  }

  void OnDeviceRemoved(size_t index,
                       const std::u16string& device_name) override {}
};

class TestExtensionsAPIClient : public ShellExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() = default;

  std::unique_ptr<DevicePermissionsPrompt> CreateDevicePermissionsPrompt(
      content::WebContents* web_contents) const override {
    return std::make_unique<TestDevicePermissionsPrompt>(web_contents);
  }

#if BUILDFLAG(IS_CHROMEOS)
  bool ShouldAllowDetachingUsb(int vid, int pid) const override {
    return vid == 1 && pid == 2;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
};

class UsbApiTest : public ShellApiTest {
 public:
  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    // Set fake USB device manager for extensions::UsbDeviceManager.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
    fake_usb_manager_.AddReceiver(usb_manager.InitWithNewPipeAndPassReceiver());
    UsbDeviceManager::Get(browser_context())
        ->SetDeviceManagerForTesting(std::move(usb_manager));
    base::RunLoop().RunUntilIdle();

    std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
    configs.push_back(
        device::FakeUsbDeviceInfo::CreateConfiguration(0xff, 0x00, 0x00, 1));
    configs.push_back(
        device::FakeUsbDeviceInfo::CreateConfiguration(0xff, 0x00, 0x00, 2));

    fake_device_ = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        0, 0, "Test Manufacturer", "Test Device", "ABC123", std::move(configs));
    fake_usb_manager_.AddDevice(fake_device_);
    fake_usb_manager_.SetMockForDevice(fake_device_->guid(), &mock_device_);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void SetActiveConfigForFakeDevice(uint8_t config_value) {
    fake_device_->SetActiveConfig(config_value);
    UsbDeviceManager::Get(browser_context())
        ->UpdateActiveConfig(fake_device_->guid(), config_value);
  }

  // `mock_device_`, `fake_device_`, and `fake_usb_manager_` must be declared in
  // this order to avoid dangling pointers.
  device::MockUsbMojoDevice mock_device_;
  scoped_refptr<device::FakeUsbDeviceInfo> fake_device_;
  device::FakeUsbDeviceManager fake_usb_manager_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UsbApiTest, DeviceHandling) {
  SetActiveConfigForFakeDevice(1);
  EXPECT_CALL(mock_device_, Open)
      .WillOnce(UsbOpenDeviceSuccess())
      .WillOnce(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, Close)
      .WillOnce(RunOnceClosure<0>())
      .WillOnce(RunOnceClosure<0>());
  ASSERT_TRUE(RunAppTest("api_test/usb/device_handling"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ResetDevice) {
  EXPECT_CALL(mock_device_, Open).WillOnce(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, Close).WillOnce(RunOnceClosure<0>());

  EXPECT_CALL(mock_device_, Reset)
      .WillOnce(RunOnceCallback<0>(true))
      .WillOnce(RunOnceCallback<0>(false));
  EXPECT_CALL(mock_device_, GenericTransferOut(2, BufferSizeIs(1u), _, _))
      .WillOnce(RunOnceCallback<3>(UsbTransferStatus::COMPLETED));
  ASSERT_TRUE(RunAppTest("api_test/usb/reset_device"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, SetConfiguration) {
  EXPECT_CALL(mock_device_, Open).WillOnce(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, Close).WillOnce(RunOnceClosure<0>());

  EXPECT_CALL(mock_device_, SetConfiguration(1, _))
      .WillOnce([&](uint8_t value, auto callback) {
        SetActiveConfigForFakeDevice(value);
        std::move(callback).Run(true);
      });

  ASSERT_TRUE(RunAppTest("api_test/usb/set_configuration"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ListInterfaces) {
  SetActiveConfigForFakeDevice(1);
  EXPECT_CALL(mock_device_, Open).WillOnce(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, Close).WillOnce(RunOnceClosure<0>());
  ASSERT_TRUE(RunAppTest("api_test/usb/list_interfaces"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferEvent) {
  auto expectedParams = UsbControlTransferParams::New();
  expectedParams->type = UsbControlTransferType::STANDARD;
  expectedParams->recipient = UsbControlTransferRecipient::DEVICE;
  expectedParams->request = 1;
  expectedParams->value = 2;
  expectedParams->index = 3;

  EXPECT_CALL(mock_device_, Open)
      .Times(AnyNumber())
      .WillRepeatedly(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, ControlTransferOut(
                                UsbControlTransferParamsEquals(*expectedParams),
                                BufferSizeIs(1u), _, _))
      .WillOnce(RunOnceCallback<3>(UsbTransferStatus::COMPLETED));
  EXPECT_CALL(mock_device_, GenericTransferOut(1, BufferSizeIs(1u), _, _))
      .WillOnce(RunOnceCallback<3>(UsbTransferStatus::COMPLETED));
  EXPECT_CALL(mock_device_, GenericTransferOut(2, BufferSizeIs(1u), _, _))
      .WillOnce(RunOnceCallback<3>(UsbTransferStatus::COMPLETED));
  EXPECT_CALL(mock_device_, IsochronousTransferOutInternal(3, _, _, _))
      .WillOnce(BuildIsochronousTransferReturnValue<2>(1, 1u));
  EXPECT_CALL(mock_device_, Close)
      .Times(AnyNumber())
      .WillRepeatedly(RunOnceClosure<0>());
  ASSERT_TRUE(RunAppTest("api_test/usb/transfer_event"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ZeroLengthTransfer) {
  EXPECT_CALL(mock_device_, Open)
      .Times(AnyNumber())
      .WillRepeatedly(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, GenericTransferOut(_, BufferSizeIs(0u), _, _))
      .WillOnce(RunOnceCallback<3>(UsbTransferStatus::COMPLETED));
  EXPECT_CALL(mock_device_, Close)
      .Times(AnyNumber())
      .WillRepeatedly(RunOnceClosure<0>());
  ASSERT_TRUE(RunAppTest("api_test/usb/zero_length_transfer"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferFailure) {
  EXPECT_CALL(mock_device_, Open)
      .Times(AnyNumber())
      .WillRepeatedly(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, GenericTransferOut(1, _, _, _))
      .WillOnce(RunOnceCallback<3>(UsbTransferStatus::COMPLETED))
      .WillOnce(RunOnceCallback<3>(UsbTransferStatus::TRANSFER_ERROR))
      .WillOnce(RunOnceCallback<3>(UsbTransferStatus::TIMEOUT));
  EXPECT_CALL(mock_device_, IsochronousTransferInInternal(2, _, _))
      .WillOnce(BuildIsochronousTransferReturnValue<1>(8, 10u))
      .WillOnce(BuildIsochronousTransferReturnValue<1>(8, 5u));
  EXPECT_CALL(mock_device_, Close)
      .Times(AnyNumber())
      .WillRepeatedly(RunOnceClosure<0>());
  ASSERT_TRUE(RunAppTest("api_test/usb/transfer_failure"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, InvalidLengthTransfer) {
  EXPECT_CALL(mock_device_, Open)
      .Times(AnyNumber())
      .WillRepeatedly(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, Close)
      .Times(AnyNumber())
      .WillRepeatedly(RunOnceClosure<0>());
  ASSERT_TRUE(RunAppTest("api_test/usb/invalid_length_transfer"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, InvalidTimeout) {
  EXPECT_CALL(mock_device_, Open)
      .Times(AnyNumber())
      .WillRepeatedly(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, Close)
      .Times(AnyNumber())
      .WillRepeatedly(RunOnceClosure<0>());
  ASSERT_TRUE(RunAppTest("api_test/usb/invalid_timeout"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, CallsAfterDisconnect) {
  ExtensionTestMessageListener ready_listener("ready");
  ExtensionTestMessageListener result_listener("success");
  result_listener.set_failure_message("failure");

  EXPECT_CALL(mock_device_, Open).WillOnce(UsbOpenDeviceSuccess());

  ASSERT_TRUE(LoadApp("api_test/usb/calls_after_disconnect"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  fake_usb_manager_.RemoveDevice(fake_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferFailureOnDisconnect) {
  ExtensionTestMessageListener ready_listener("ready");
  ExtensionTestMessageListener result_listener("success");
  result_listener.set_failure_message("failure");

  EXPECT_CALL(mock_device_, Open).WillOnce(UsbOpenDeviceSuccess());

  device::mojom::UsbDevice::GenericTransferInCallback saved_callback;
  EXPECT_CALL(mock_device_, GenericTransferIn)
      .WillOnce(MoveArg<3>(&saved_callback));

  ASSERT_TRUE(LoadApp("api_test/usb/transfer_failure_on_disconnect"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  fake_usb_manager_.RemoveDevice(fake_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, OnDeviceAdded) {
  ExtensionTestMessageListener load_listener("loaded");
  ExtensionTestMessageListener result_listener("success");
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/usb/add_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  fake_usb_manager_.CreateAndAddDevice(0x18D1, 0x58F0);
  fake_usb_manager_.CreateAndAddDevice(0x18D1, 0x58F1);

  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, OnDeviceRemoved) {
  ExtensionTestMessageListener load_listener("loaded");
  ExtensionTestMessageListener result_listener("success");
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/usb/remove_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  fake_usb_manager_.RemoveDevice(fake_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, GetUserSelectedDevices) {
  ExtensionTestMessageListener ready_listener("opened_device");
  ExtensionTestMessageListener result_listener("success");
  result_listener.set_failure_message("failure");

  EXPECT_CALL(mock_device_, Open).WillOnce(UsbOpenDeviceSuccess());
  EXPECT_CALL(mock_device_, Close).WillOnce(RunOnceClosure<0>());

  TestExtensionsAPIClient test_api_client;
  ASSERT_TRUE(LoadApp("api_test/usb/get_user_selected_devices"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  fake_usb_manager_.RemoveDevice(fake_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(UsbApiTest, MassStorage) {
  ExtensionTestMessageListener ready_listener("ready");
  ready_listener.set_failure_message("failure");
  ExtensionTestMessageListener result_listener("success");
  result_listener.set_failure_message("failure");

  // Mass storage devices should be hidden unless allowed in policy.
  // The TestExtensionsAPIClient allows only vid=1, pid=2.
  TestExtensionsAPIClient test_api_client;
  std::vector<device::mojom::UsbConfigurationInfoPtr> storage_configs;
  auto storage_config = device::FakeUsbDeviceInfo::CreateConfiguration(
      /* mass storage */ 0x08, 0x06, 0x50);
  storage_configs.push_back(storage_config->Clone());
  device::mojom::UsbDeviceInfoPtr device_1 =
      fake_usb_manager_.CreateAndAddDevice(0x1, 0x2, 0x00,
                                           std::move(storage_configs));

  storage_configs.clear();
  storage_configs.push_back(storage_config->Clone());
  device::mojom::UsbDeviceInfoPtr device_2 =
      fake_usb_manager_.CreateAndAddDevice(0x5, 0x6, 0x00,
                                           std::move(storage_configs));

  ASSERT_TRUE(LoadApp("api_test/usb/mass_storage"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  fake_usb_manager_.RemoveDevice(device_2->guid);
  fake_usb_manager_.RemoveDevice(device_1->guid);

  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
