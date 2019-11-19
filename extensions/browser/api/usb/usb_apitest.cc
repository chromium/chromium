// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <numeric>

#include "base/memory/ref_counted_memory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/usb/usb_api.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/cpp/test/mock_usb_mojo_device.h"
#include "services/device/public/mojom/usb_device.mojom.h"

using device::mojom::UsbControlTransferParams;
using device::mojom::UsbControlTransferRecipient;
using device::mojom::UsbControlTransferType;
using device::mojom::UsbIsochronousPacket;
using device::mojom::UsbOpenDeviceError;
using device::mojom::UsbTransferDirection;
using device::mojom::UsbTransferStatus;

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;

namespace extensions {

namespace {
ACTION_TEMPLATE(InvokeCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p1)) {
  std::move(*std::get<k>(args)).Run(p1);
}

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

ACTION_P(SetConfiguration, fake_device) {
  fake_device->SetActiveConfig(arg0);
  std::move(*arg1).Run(true);
}

ACTION(InvokeClosureCallback) {
  std::move(*arg0).Run();
}

MATCHER_P(BufferSizeIs, size, "") {
  return arg.size() == size;
}

MATCHER_P(UsbControlTransferParamsEquals, expected, "") {
  return arg.Equals(expected);
}

class TestDevicePermissionsPrompt
    : public DevicePermissionsPrompt,
      public DevicePermissionsPrompt::Prompt::Observer {
 public:
  explicit TestDevicePermissionsPrompt(content::WebContents* web_contents)
      : DevicePermissionsPrompt(web_contents) {}

  void ShowDialog() override { prompt()->SetObserver(this); }

  void OnDeviceAdded(size_t index, const base::string16& device_name) override {
    OnDevicesChanged();
  }

  void OnDeviceRemoved(size_t index,
                       const base::string16& device_name) override {
    OnDevicesChanged();
  }

 private:
  void OnDevicesChanged() {
    for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
      prompt()->GrantDevicePermission(i);
      if (!prompt()->multiple()) {
        break;
      }
    }
    prompt()->Dismissed();
  }
};

class TestExtensionsAPIClient : public ShellExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() : ShellExtensionsAPIClient() {}

  std::unique_ptr<DevicePermissionsPrompt> CreateDevicePermissionsPrompt(
      content::WebContents* web_contents) const override {
    return std::make_unique<TestDevicePermissionsPrompt>(web_contents);
  }
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
    auto config_1 = device::mojom::UsbConfigurationInfo::New();
    config_1->configuration_value = 1;
    configs.push_back(std::move(config_1));
    auto config_2 = device::mojom::UsbConfigurationInfo::New();
    config_2->configuration_value = 2;
    configs.push_back(std::move(config_2));

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

  device::FakeUsbDeviceManager fake_usb_manager_;
  scoped_refptr<device::FakeUsbDeviceInfo> fake_device_;
  device::MockUsbMojoDevice mock_device_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UsbApiTest, DeviceHandling) {
  SetActiveConfigForFakeDevice(1);
  EXPECT_CALL(mock_device_, OpenInternal(_))
      .WillOnce(InvokeCallback<0>(UsbOpenDeviceError::OK))
      .WillOnce(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_, CloseInternal(_))
      .WillOnce(InvokeClosureCallback())
      .WillOnce(InvokeClosureCallback());
  ASSERT_TRUE(RunAppTest("api_test/usb/device_handling"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ResetDevice) {
  EXPECT_CALL(mock_device_, OpenInternal(_))
      .WillOnce(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_, CloseInternal(_)).WillOnce(InvokeClosureCallback());

  EXPECT_CALL(mock_device_, ResetInternal(_))
      .WillOnce(InvokeCallback<0>(true))
      .WillOnce(InvokeCallback<0>(false));
  EXPECT_CALL(mock_device_,
              GenericTransferOutInternal(2, BufferSizeIs(1u), _, _))
      .WillOnce(InvokeCallback<3>(UsbTransferStatus::COMPLETED));
  ASSERT_TRUE(RunAppTest("api_test/usb/reset_device"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, SetConfiguration) {
  EXPECT_CALL(mock_device_, OpenInternal(_))
      .WillOnce(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_, CloseInternal(_)).WillOnce(InvokeClosureCallback());

  EXPECT_CALL(mock_device_, SetConfigurationInternal(1, _))
      .WillOnce(SetConfiguration(fake_device_.get()));

  ASSERT_TRUE(RunAppTest("api_test/usb/set_configuration"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ListInterfaces) {
  SetActiveConfigForFakeDevice(1);
  EXPECT_CALL(mock_device_, OpenInternal(_))
      .WillOnce(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_, CloseInternal(_)).WillOnce(InvokeClosureCallback());
  ASSERT_TRUE(RunAppTest("api_test/usb/list_interfaces"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferEvent) {
  auto expectedParams = UsbControlTransferParams::New();
  expectedParams->type = UsbControlTransferType::STANDARD;
  expectedParams->recipient = UsbControlTransferRecipient::DEVICE;
  expectedParams->request = 1;
  expectedParams->value = 2;
  expectedParams->index = 3;

  EXPECT_CALL(mock_device_, OpenInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_, ControlTransferOutInternal(
                                UsbControlTransferParamsEquals(*expectedParams),
                                BufferSizeIs(1u), _, _))
      .WillOnce(InvokeCallback<3>(UsbTransferStatus::COMPLETED));
  EXPECT_CALL(mock_device_,
              GenericTransferOutInternal(1, BufferSizeIs(1u), _, _))
      .WillOnce(InvokeCallback<3>(UsbTransferStatus::COMPLETED));
  EXPECT_CALL(mock_device_,
              GenericTransferOutInternal(2, BufferSizeIs(1u), _, _))
      .WillOnce(InvokeCallback<3>(UsbTransferStatus::COMPLETED));
  EXPECT_CALL(mock_device_, IsochronousTransferOutInternal(3, _, _, _))
      .WillOnce(BuildIsochronousTransferReturnValue<2>(1, 1u));
  EXPECT_CALL(mock_device_, CloseInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeClosureCallback());
  ASSERT_TRUE(RunAppTest("api_test/usb/transfer_event"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ZeroLengthTransfer) {
  EXPECT_CALL(mock_device_, OpenInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_,
              GenericTransferOutInternal(_, BufferSizeIs(0u), _, _))
      .WillOnce(InvokeCallback<3>(UsbTransferStatus::COMPLETED));
  EXPECT_CALL(mock_device_, CloseInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeClosureCallback());
  ASSERT_TRUE(RunAppTest("api_test/usb/zero_length_transfer"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferFailure) {
  EXPECT_CALL(mock_device_, OpenInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_, GenericTransferOutInternal(1, _, _, _))
      .WillOnce(InvokeCallback<3>(UsbTransferStatus::COMPLETED))
      .WillOnce(InvokeCallback<3>(UsbTransferStatus::TRANSFER_ERROR))
      .WillOnce(InvokeCallback<3>(UsbTransferStatus::TIMEOUT));
  EXPECT_CALL(mock_device_, IsochronousTransferInInternal(2, _, _))
      .WillOnce(BuildIsochronousTransferReturnValue<1>(8, 10u))
      .WillOnce(BuildIsochronousTransferReturnValue<1>(8, 5u));
  EXPECT_CALL(mock_device_, CloseInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeClosureCallback());
  ASSERT_TRUE(RunAppTest("api_test/usb/transfer_failure"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, InvalidLengthTransfer) {
  EXPECT_CALL(mock_device_, OpenInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_, CloseInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeClosureCallback());
  ASSERT_TRUE(RunAppTest("api_test/usb/invalid_length_transfer"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, InvalidTimeout) {
  EXPECT_CALL(mock_device_, OpenInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_, CloseInternal(_))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeClosureCallback());
  ASSERT_TRUE(RunAppTest("api_test/usb/invalid_timeout"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, CallsAfterDisconnect) {
  ExtensionTestMessageListener ready_listener("ready", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  EXPECT_CALL(mock_device_, OpenInternal(_))
      .WillOnce(InvokeCallback<0>(UsbOpenDeviceError::OK));

  ASSERT_TRUE(LoadApp("api_test/usb/calls_after_disconnect"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  fake_usb_manager_.RemoveDevice(fake_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferFailureOnDisconnect) {
  ExtensionTestMessageListener ready_listener("ready", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  EXPECT_CALL(mock_device_, OpenInternal(_))
      .WillOnce(InvokeCallback<0>(UsbOpenDeviceError::OK));

  device::mojom::UsbDevice::GenericTransferInCallback saved_callback;
  EXPECT_CALL(mock_device_, GenericTransferInInternal(_, _, _, _))
      .WillOnce(
          [&saved_callback](
              uint8_t endpoint_number, uint32_t length, uint32_t timeout,
              device::MockUsbMojoDevice::GenericTransferInCallback* callback) {
            saved_callback = std::move(*callback);
          });

  ASSERT_TRUE(LoadApp("api_test/usb/transfer_failure_on_disconnect"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  fake_usb_manager_.RemoveDevice(fake_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, OnDeviceAdded) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/usb/add_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  fake_usb_manager_.CreateAndAddDevice(0x18D1, 0x58F0);
  fake_usb_manager_.CreateAndAddDevice(0x18D1, 0x58F1);

  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, OnDeviceRemoved) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/usb/remove_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  fake_usb_manager_.RemoveDevice(fake_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, GetUserSelectedDevices) {
  ExtensionTestMessageListener ready_listener("opened_device", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  EXPECT_CALL(mock_device_, OpenInternal(_))
      .WillOnce(InvokeCallback<0>(UsbOpenDeviceError::OK));
  EXPECT_CALL(mock_device_, CloseInternal(_)).WillOnce(InvokeClosureCallback());

  TestExtensionsAPIClient test_api_client;
  ASSERT_TRUE(LoadApp("api_test/usb/get_user_selected_devices"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  fake_usb_manager_.RemoveDevice(fake_device_);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

}  // namespace extensions
