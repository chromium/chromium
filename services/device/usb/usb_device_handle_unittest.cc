// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_io_thread.h"
#include "services/device/test/usb_test_gadget.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;

namespace {

class UsbDeviceHandleTest : public ::testing::Test {
 public:
  UsbDeviceHandleTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        usb_service_(UsbService::Create()),
        io_thread_(base::TestIOThread::kAutoStart) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<UsbService> usb_service_;
  base::TestIOThread io_thread_;
};

class TestOpenCallback {
 public:
  TestOpenCallback() = default;

  scoped_refptr<UsbDeviceHandle> WaitForResult() {
    run_loop_.Run();
    return device_handle_;
  }

  UsbDevice::OpenCallback GetCallback() {
    return base::BindOnce(&TestOpenCallback::SetResult, base::Unretained(this));
  }

 private:
  void SetResult(scoped_refptr<UsbDeviceHandle> device_handle) {
    device_handle_ = device_handle;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  scoped_refptr<UsbDeviceHandle> device_handle_;
};

class TestResultCallback {
 public:
  TestResultCallback() = default;

  bool WaitForResult() {
    run_loop_.Run();
    return success_;
  }

  UsbDeviceHandle::ResultCallback GetCallback() {
    return base::BindOnce(&TestResultCallback::SetResult,
                          base::Unretained(this));
  }

 private:
  void SetResult(bool success) {
    success_ = success;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool success_;
};

class TestCompletionCallback {
 public:
  TestCompletionCallback() = default;

  void WaitForResult() { run_loop_.Run(); }

  UsbDeviceHandle::TransferCallback GetCallback() {
    return base::BindOnce(&TestCompletionCallback::SetResult,
                          base::Unretained(this));
  }
  UsbTransferStatus status() const { return status_; }
  size_t transferred() const { return transferred_; }

 private:
  void SetResult(UsbTransferStatus status,
                 scoped_refptr<base::RefCountedBytes> buffer,
                 size_t transferred) {
    status_ = status;
    transferred_ = transferred;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  UsbTransferStatus status_;
  size_t transferred_;
};

void ExpectTimeoutAndClose(scoped_refptr<UsbDeviceHandle> handle,
                           const base::Closure& quit_closure,
                           UsbTransferStatus status,
                           scoped_refptr<base::RefCountedBytes> buffer,
                           size_t transferred) {
  EXPECT_EQ(UsbTransferStatus::TIMEOUT, status);
  handle->Close();
  quit_closure.Run();
}

TEST_F(UsbDeviceHandleTest, InterruptTransfer) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(0, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  const mojom::UsbInterfaceInfo* interface =
      handle->FindInterfaceByEndpoint(0x81);
  EXPECT_TRUE(interface);
  EXPECT_EQ(0, interface->interface_number);
  interface = handle->FindInterfaceByEndpoint(0x01);
  EXPECT_TRUE(interface);
  EXPECT_EQ(0, interface->interface_number);
  EXPECT_FALSE(handle->FindInterfaceByEndpoint(0x82));
  EXPECT_FALSE(handle->FindInterfaceByEndpoint(0x02));

  auto in_buffer = base::MakeRefCounted<base::RefCountedBytes>(64);
  TestCompletionCallback in_completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x81, in_buffer,
                          5000,  // 5 second timeout
                          in_completion.GetCallback());

  auto out_buffer =
      base::MakeRefCounted<base::RefCountedBytes>(in_buffer->size());
  TestCompletionCallback out_completion;
  for (size_t i = 0; i < out_buffer->size(); ++i) {
    out_buffer->data()[i] = i;
  }

  handle->GenericTransfer(UsbTransferDirection::OUTBOUND, 0x01, out_buffer,
                          5000,  // 5 second timeout
                          out_completion.GetCallback());
  out_completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, out_completion.status());
  EXPECT_EQ(static_cast<size_t>(out_buffer->size()),
            out_completion.transferred());

  in_completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, in_completion.status());
  EXPECT_EQ(static_cast<size_t>(in_buffer->size()),
            in_completion.transferred());
  for (size_t i = 0; i < in_completion.transferred(); ++i) {
    EXPECT_EQ(out_buffer->front()[i], in_buffer->front()[i])
        << "Mismatch at index " << i << ".";
  }

  TestResultCallback release_interface;
  handle->ReleaseInterface(0, release_interface.GetCallback());
  ASSERT_TRUE(release_interface.WaitForResult());

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, BulkTransfer) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  EXPECT_FALSE(handle->FindInterfaceByEndpoint(0x81));
  EXPECT_FALSE(handle->FindInterfaceByEndpoint(0x01));
  const mojom::UsbInterfaceInfo* interface =
      handle->FindInterfaceByEndpoint(0x82);
  EXPECT_TRUE(interface);
  EXPECT_EQ(1, interface->interface_number);
  interface = handle->FindInterfaceByEndpoint(0x02);
  EXPECT_TRUE(interface);
  EXPECT_EQ(1, interface->interface_number);

  auto in_buffer = base::MakeRefCounted<base::RefCountedBytes>(512);
  TestCompletionCallback in_completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x82, in_buffer,
                          5000,  // 5 second timeout
                          in_completion.GetCallback());

  auto out_buffer =
      base::MakeRefCounted<base::RefCountedBytes>(in_buffer->size());
  TestCompletionCallback out_completion;
  for (size_t i = 0; i < out_buffer->size(); ++i) {
    out_buffer->data()[i] = i;
  }

  handle->GenericTransfer(UsbTransferDirection::OUTBOUND, 0x02, out_buffer,
                          5000,  // 5 second timeout
                          out_completion.GetCallback());
  out_completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, out_completion.status());
  EXPECT_EQ(static_cast<size_t>(out_buffer->size()),
            out_completion.transferred());

  in_completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, in_completion.status());
  EXPECT_EQ(static_cast<size_t>(in_buffer->size()),
            in_completion.transferred());
  for (size_t i = 0; i < in_completion.transferred(); ++i) {
    EXPECT_EQ(out_buffer->front()[i], in_buffer->front()[i])
        << "Mismatch at index " << i << ".";
  }

  TestResultCallback release_interface;
  handle->ReleaseInterface(1, release_interface.GetCallback());
  ASSERT_TRUE(release_interface.WaitForResult());

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, ControlTransfer) {
  if (!UsbTestGadget::IsTestEnabled())
    return;

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(255);
  TestCompletionCallback completion;
  handle->ControlTransfer(UsbTransferDirection::INBOUND,
                          UsbControlTransferType::STANDARD,
                          UsbControlTransferRecipient::DEVICE, 0x06, 0x0301,
                          0x0409, buffer, 0, completion.GetCallback());
  completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::COMPLETED, completion.status());
  const char expected_str[] = "\x18\x03G\0o\0o\0g\0l\0e\0 \0I\0n\0c\0.\0";
  EXPECT_EQ(sizeof(expected_str) - 1, completion.transferred());
  for (size_t i = 0; i < completion.transferred(); ++i) {
    EXPECT_EQ(expected_str[i], buffer->front()[i])
        << "Mismatch at index " << i << ".";
  }

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, SetInterfaceAlternateSetting) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(2, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  TestResultCallback set_interface;
  handle->SetInterfaceAlternateSetting(2, 1, set_interface.GetCallback());
  ASSERT_TRUE(set_interface.WaitForResult());

  TestResultCallback release_interface;
  handle->ReleaseInterface(2, release_interface.GetCallback());
  ASSERT_TRUE(release_interface.WaitForResult());

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, CancelOnClose) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(512);
  TestCompletionCallback completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x82, buffer,
                          5000,  // 5 second timeout
                          completion.GetCallback());

  handle->Close();
  completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::CANCELLED, completion.status());
}

TEST_F(UsbDeviceHandleTest, ErrorOnDisconnect) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(512);
  TestCompletionCallback completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x82, buffer,
                          5000,  // 5 second timeout
                          completion.GetCallback());

  ASSERT_TRUE(gadget->Disconnect());
  completion.WaitForResult();
  // Depending on timing the transfer can be cancelled by the disconnection, be
  // rejected because the device is already missing or result in another generic
  // error as the device drops off the bus.
  EXPECT_TRUE(completion.status() == UsbTransferStatus::CANCELLED ||
              completion.status() == UsbTransferStatus::DISCONNECT ||
              completion.status() == UsbTransferStatus::TRANSFER_ERROR);

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, Timeout) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(512);
  TestCompletionCallback completion;
  handle->GenericTransfer(UsbTransferDirection::INBOUND, 0x82, buffer,
                          10,  // 10 millisecond timeout
                          completion.GetCallback());

  completion.WaitForResult();
  ASSERT_EQ(UsbTransferStatus::TIMEOUT, completion.status());

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, CloseReentrancy) {
  if (!UsbTestGadget::IsTestEnabled())
    return;

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.GetCallback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.GetCallback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  base::RunLoop run_loop;
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(512);
  handle->GenericTransfer(
      UsbTransferDirection::INBOUND, 0x82, buffer,
      10,  // 10 millisecond timeout
      base::BindOnce(&ExpectTimeoutAndClose, handle, run_loop.QuitClosure()));
  // Drop handle so that the completion callback holds the last reference.
  handle = nullptr;
  run_loop.Run();
}

}  // namespace

}  // namespace device
