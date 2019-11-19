// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_connection.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_io_thread.h"
#include "services/device/hid/hid_service.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/device/test/usb_test_gadget.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

// Helper class that can be used to block until a HID device with a particular
// serial number is available. Example usage:
//
//   DeviceCatcher device_catcher("ABC123");
//   std::string device_guid = device_catcher.WaitForDevice();
//   /* Call HidService::Connect(device_guid) to open the device. */
//
class DeviceCatcher : HidService::Observer {
 public:
  DeviceCatcher(HidService* hid_service, const base::string16& serial_number)
      : serial_number_(base::UTF16ToUTF8(serial_number)), observer_(this) {
    hid_service->GetDevices(
        base::BindOnce(&DeviceCatcher::OnEnumerationComplete,
                       base::Unretained(this), hid_service));
  }

  const std::string& WaitForDevice() {
    run_loop_.Run();
    observer_.RemoveAll();
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
    observer_.Add(hid_service);
  }

  void OnDeviceAdded(mojom::HidDeviceInfoPtr device_info) override {
    if (device_info->serial_number == serial_number_) {
      device_guid_ = device_info->guid;
      run_loop_.Quit();
    }
  }

  std::string serial_number_;
  ScopedObserver<HidService, HidService::Observer> observer_;
  base::RunLoop run_loop_;
  std::string device_guid_;
};

class TestConnectCallback {
 public:
  TestConnectCallback() {}
  ~TestConnectCallback() {}

  void SetConnection(scoped_refptr<HidConnection> connection) {
    connection_ = connection;
    run_loop_.Quit();
  }

  scoped_refptr<HidConnection> WaitForConnection() {
    run_loop_.Run();
    return connection_;
  }

  HidService::ConnectCallback GetCallback() {
    return base::Bind(&TestConnectCallback::SetConnection,
                      base::Unretained(this));
  }

 private:
  base::RunLoop run_loop_;
  scoped_refptr<HidConnection> connection_;
};

class TestIoCallback {
 public:
  TestIoCallback() {}
  ~TestIoCallback() {}

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

 protected:
  void SetUp() override {
    if (!UsbTestGadget::IsTestEnabled() || !usb_service_)
      return;

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
  if (!UsbTestGadget::IsTestEnabled())
    return;

  TestConnectCallback connect_callback;
  service_->Connect(device_guid_, connect_callback.GetCallback());
  scoped_refptr<HidConnection> conn = connect_callback.WaitForConnection();
  ASSERT_TRUE(conn.get());

  const char kBufferSize = 9;
  for (char i = 0; i < 8; ++i) {
    auto buffer = base::MakeRefCounted<base::RefCountedBytes>(kBufferSize);
    buffer->data()[0] = 0;
    for (unsigned char j = 1; j < kBufferSize; ++j) {
      buffer->data()[j] = i + j - 1;
    }

    TestIoCallback write_callback;
    conn->Write(buffer, write_callback.GetWriteCallback());
    ASSERT_TRUE(write_callback.WaitForResult());

    TestIoCallback read_callback;
    conn->Read(read_callback.GetReadCallback());
    ASSERT_TRUE(read_callback.WaitForResult());
    ASSERT_EQ(9UL, read_callback.size());
    ASSERT_EQ(0, read_callback.buffer()->data()[0]);
    for (unsigned char j = 1; j < kBufferSize; ++j) {
      ASSERT_EQ(i + j - 1, read_callback.buffer()->data()[j]);
    }
  }

  conn->Close();
}

}  // namespace device
