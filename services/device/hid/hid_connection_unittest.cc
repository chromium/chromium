// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_connection.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_io_thread.h"
#include "build/build_config.h"
#include "services/device/hid/hid_connection.h"
#include "services/device/hid/hid_service.h"
#include "services/device/public/cpp/test/test_report_descriptors.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/device/test/usb_test_gadget.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::base::test::TestFuture;

// Helper class that can be used to block until a HID device with a particular
// serial number is available. Example usage:
//
//   DeviceCatcher device_catcher("ABC123");
//   std::string device_guid = device_catcher.WaitForDevice();
//   /* Call HidService::Connect(device_guid) to open the device. */
//
class DeviceCatcher : HidService::Observer {
 public:
  DeviceCatcher(HidService* hid_service, const std::u16string& serial_number)
      : serial_number_(base::UTF16ToUTF8(serial_number)) {
    hid_service->GetDevices(
        base::BindOnce(&DeviceCatcher::OnEnumerationComplete,
                       base::Unretained(this), hid_service));
  }
  DeviceCatcher(DeviceCatcher&) = delete;
  DeviceCatcher& operator=(DeviceCatcher&) = delete;

  const std::string& WaitForDevice() {
    run_loop_.Run();
    observation_.Reset();
    return device_guid_;
  }

 private:
  void OnEnumerationComplete(HidService* hid_service,
                             std::vector<mojom::HidDeviceInfoPtr> devices) {
    for (auto& device_info : devices) {
      if (device_info->serial_number == serial_number_) {
        device_guid_ = device_info->guid;
        run_loop_.Quit();
        break;
      }
    }
    observation_.Observe(hid_service);
  }

  void OnDeviceAdded(mojom::HidDeviceInfoPtr device_info) override {
    if (device_info->serial_number == serial_number_) {
      device_guid_ = device_info->guid;
      run_loop_.Quit();
    }
  }

  std::string serial_number_;
  base::ScopedObservation<HidService, HidService::Observer> observation_{this};
  base::RunLoop run_loop_;
  std::string device_guid_;
};

class TestConnectCallback {
 public:
  TestConnectCallback() = default;
  TestConnectCallback(TestConnectCallback&) = delete;
  TestConnectCallback& operator=(TestConnectCallback&) = delete;
  ~TestConnectCallback() = default;

  void SetConnection(scoped_refptr<HidConnection> connection) {
    connection_ = connection;
    run_loop_.Quit();
  }

  scoped_refptr<HidConnection> WaitForConnection() {
    run_loop_.Run();
    return connection_;
  }

  HidService::ConnectCallback GetCallback() {
    return base::BindOnce(&TestConnectCallback::SetConnection,
                          base::Unretained(this));
  }

 private:
  base::RunLoop run_loop_;
  scoped_refptr<HidConnection> connection_;
};

class TestIoCallback {
 public:
  TestIoCallback() = default;
  TestIoCallback(TestIoCallback&) = delete;
  TestIoCallback& operator=(TestIoCallback&) = delete;
  ~TestIoCallback() = default;

  void SetReadResult(bool success,
                     scoped_refptr<base::RefCountedBytes> buffer,
                     size_t size) {
    result_ = success;
    buffer_ = buffer;
    size_ = size;
    run_loop_.Quit();
  }

  void SetWriteResult(bool success) {
    result_ = success;
    run_loop_.Quit();
  }

  bool WaitForResult() {
    run_loop_.Run();
    return result_;
  }

  HidConnection::ReadCallback GetReadCallback() {
    return base::BindOnce(&TestIoCallback::SetReadResult,
                          base::Unretained(this));
  }
  HidConnection::WriteCallback GetWriteCallback() {
    return base::BindOnce(&TestIoCallback::SetWriteResult,
                          base::Unretained(this));
  }
  scoped_refptr<base::RefCountedBytes> buffer() const { return buffer_; }
  size_t size() const { return size_; }

 private:
  base::RunLoop run_loop_;
  bool result_;
  size_t size_;
  scoped_refptr<base::RefCountedBytes> buffer_;
};

}  // namespace

class HidConnectionTest : public testing::Test {
 public:
  HidConnectionTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        io_thread_(base::TestIOThread::kAutoStart) {}
  HidConnectionTest(HidConnectionTest&) = delete;
  HidConnectionTest& operator=(HidConnectionTest&) = delete;

 protected:
  void SetUp() override {
    if (!UsbTestGadget::IsTestEnabled() || !usb_service_) {
      return;
    }

    service_ = HidService::Create();
    ASSERT_TRUE(service_);

    usb_service_ = UsbService::Create();
    test_gadget_ =
        UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
    ASSERT_TRUE(test_gadget_);
    ASSERT_TRUE(test_gadget_->SetType(UsbTestGadget::HID_ECHO));

    DeviceCatcher device_catcher(service_.get(),
                                 test_gadget_->GetDevice()->serial_number());
    device_guid_ = device_catcher.WaitForDevice();
    ASSERT_FALSE(device_guid_.empty());
  }

  base::test::TaskEnvironment task_environment_;
  base::TestIOThread io_thread_;
  std::unique_ptr<HidService> service_;
  std::unique_ptr<UsbTestGadget> test_gadget_;
  std::unique_ptr<UsbService> usb_service_;
  std::string device_guid_;
};

TEST_F(HidConnectionTest, ReadWrite) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  TestConnectCallback connect_callback;
  service_->Connect(device_guid_, /*allow_protected_reports=*/false,
                    /*allow_fido_reports=*/false,
                    connect_callback.GetCallback());
  scoped_refptr<HidConnection> conn = connect_callback.WaitForConnection();
  ASSERT_TRUE(conn.get());

  const char kBufferSize = 9;
  for (char i = 0; i < 8; ++i) {
    auto buffer = base::MakeRefCounted<base::RefCountedBytes>(kBufferSize);
    buffer->as_vector()[0] = 0;
    for (unsigned char j = 1; j < kBufferSize; ++j) {
      buffer->as_vector()[j] = i + j - 1;
    }

    TestIoCallback write_callback;
    conn->Write(buffer, write_callback.GetWriteCallback());
    ASSERT_TRUE(write_callback.WaitForResult());

    TestIoCallback read_callback;
    conn->Read(read_callback.GetReadCallback());
    ASSERT_TRUE(read_callback.WaitForResult());
    ASSERT_EQ(9UL, read_callback.size());
    ASSERT_EQ(0, read_callback.buffer()->as_vector()[0]);
    for (unsigned char j = 1; j < kBufferSize; ++j) {
      ASSERT_EQ(i + j - 1, read_callback.buffer()->as_vector()[j]);
    }
  }

  conn->Close();
}

namespace {

// A test implementation of HidConnection where the platform operations always
// succeed.
class TestHidConnection : public HidConnection {
 public:
  explicit TestHidConnection(scoped_refptr<HidDeviceInfo> device_info)
      : HidConnection(device_info,
                      /*allow_protected_reports=*/false,
                      /*allow_fido_reports=*/false) {}
  TestHidConnection(const TestHidConnection&) = delete;
  TestHidConnection& operator=(TestHidConnection&) = delete;

  void PlatformClose() override {}
  void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                     WriteCallback callback) override {
    std::move(callback).Run(/*success=*/true);
  }
  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override {
    auto buffer =
        base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{42});
    std::move(callback).Run(/*success=*/true, buffer, buffer->size());
  }
  void PlatformSendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                                 WriteCallback callback) override {
    std::move(callback).Run(/*success=*/true);
  }
  void SimulateInputReport(scoped_refptr<base::RefCountedBytes> buffer) {
    ProcessInputReport(buffer, buffer->size());
  }

 private:
  ~TestHidConnection() override = default;
};

}  // namespace

class HidConnectionProtectedReportTest : public testing::Test,
                                         HidConnection::Client {
 public:
  HidConnectionProtectedReportTest() = default;
  HidConnectionProtectedReportTest(const HidConnectionProtectedReportTest&) =
      delete;
  HidConnectionProtectedReportTest& operator=(
      const HidConnectionProtectedReportTest&) = delete;
  ~HidConnectionProtectedReportTest() override = default;

  scoped_refptr<HidDeviceInfo> CreateHidDeviceInfo(
      base::span<const uint8_t> report_descriptor) {
#if BUILDFLAG(IS_MAC)
    const uint64_t kTestDeviceId = 0;
#elif BUILDFLAG(IS_WIN)
    const wchar_t* const kTestDeviceId = L"0";
#else
    const char* const kTestDeviceId = "0";
#endif
    return base::MakeRefCounted<HidDeviceInfo>(
        kTestDeviceId, "physical-device-id", /*vendor_id=*/0x1234,
        /*product_id=*/0xabcd, "product-name", "serial-number",
        mojom::HidBusType::kHIDBusTypeUSB, report_descriptor);
  }

  void CreateConnection(scoped_refptr<HidDeviceInfo> device_info) {
    connection_ = base::MakeRefCounted<TestHidConnection>(device_info);
  }

  void SetConnectionClient() { connection_->SetClient(this); }

  TestHidConnection& connection() { return *connection_.get(); }

  bool HasNextInputReport() { return input_report_future_.IsReady(); }

  std::pair<scoped_refptr<base::RefCountedBytes>, size_t>
  TakeNextInputReport() {
    return input_report_future_.Take();
  }

 private:
  // HidConnection::Client:
  void OnInputReport(scoped_refptr<base::RefCountedBytes> buffer,
                     size_t size) override {
    input_report_future_.SetValue(buffer, size);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<TestHidConnection> connection_;
  base::test::TestFuture<scoped_refptr<base::RefCountedBytes>, size_t>
      input_report_future_;
};

TEST_F(HidConnectionProtectedReportTest, UnprotectedReadWrite) {
  // Simulate a connection to a device with input, output, and feature reports.
  // No reports are protected.
  auto device_info =
      CreateHidDeviceInfo(TestReportDescriptors::SonyDualshock3Usb());
  ASSERT_TRUE(device_info);
  CreateConnection(device_info);

  // Simulate an input report and read it.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{1});
  connection().SimulateInputReport(buffer);
  connection().Read(read_future.GetCallback());
  EXPECT_TRUE(read_future.Get<0>());

  SetConnectionClient();

  // Simulate another input report. This time it should notify the connection
  // client.
  connection().SimulateInputReport(buffer);
  TakeNextInputReport();

  // Write an output report.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());

  // Get a feature report.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t>
      get_feature_future;
  connection().GetFeatureReport(/*report_id=*/1,
                                get_feature_future.GetCallback());
  EXPECT_TRUE(get_feature_future.Get<0>());

  // Send a feature report.
  TestFuture<bool> send_feature_future;
  connection().SendFeatureReport(buffer, send_feature_future.GetCallback());
  EXPECT_TRUE(send_feature_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest, ProtectedReportsWithClient) {
  // Simulate a connection to a device with protected input and output reports
  // and an unprotected feature report. Input and output reports are protected
  // because the device has the Generic Desktop Keyboard usage.
  auto device_info =
      CreateHidDeviceInfo(TestReportDescriptors::RfideasPcproxBadgeReader());
  ASSERT_TRUE(device_info);
  CreateConnection(device_info);

  SetConnectionClient();

  // Simulate an input report after setting the connection client. The
  // report should not be received by the client.
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{0});
  connection().SimulateInputReport(buffer);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasNextInputReport());

  // Try to write a protected output report. It should be blocked.
  TestFuture<bool> write_future;
  connection().Write(buffer, write_future.GetCallback());
  EXPECT_FALSE(write_future.Get());

  // Get a feature report. It should succeed.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t>
      get_feature_future;
  connection().GetFeatureReport(/*report_id=*/0,
                                get_feature_future.GetCallback());
  EXPECT_TRUE(get_feature_future.Get<0>());

  // Send a feature report. It should succeed.
  TestFuture<bool> send_feature_future;
  connection().SendFeatureReport(buffer, send_feature_future.GetCallback());
  EXPECT_TRUE(send_feature_future.Get());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

TEST_F(HidConnectionProtectedReportTest, ProtectedInputReportWithoutClient) {
  // Simulate a connection to a device with protected input reports.
  auto device_info = CreateHidDeviceInfo(TestReportDescriptors::Mouse());
  ASSERT_TRUE(device_info);
  CreateConnection(device_info);

  // Simulate an input report. It should be ignored.
  TestFuture<bool, scoped_refptr<base::RefCountedBytes>, size_t> read_future;
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(std::vector<uint8_t>{0});
  connection().SimulateInputReport(buffer);
  connection().Read(read_future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(read_future.IsReady());

  // Close the connection.
  connection().Close();
  EXPECT_TRUE(connection().closed());
}

}  // namespace device
