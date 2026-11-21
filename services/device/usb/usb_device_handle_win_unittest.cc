// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_win.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/win/object_watcher.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithArg;
using ::testing::WithArgs;

constexpr uint8_t kBulkEndpointNumber = 1;
constexpr uint8_t kBulkInEndpointAddress = 0x81;
constexpr uint8_t kBulkOutEndpointAddress = 0x01;
constexpr uint8_t kBulkTransferAttributes = 0x02;
const WINUSB_INTERFACE_HANDLE kFakeInterfaceHandle =
    reinterpret_cast<WINUSB_INTERFACE_HANDLE>(static_cast<uintptr_t>(1));

template <typename BufferSpan>
auto ReturnPipeIoPending(
    base::test::TestFuture<BufferSpan, OVERLAPPED*>& future) {
  return DoAll(SetArgPointee<4>(ERROR_IO_PENDING),
               WithArgs<2, 3>(base::test::InvokeFuture(future)), Return(FALSE));
}

auto ReturnPipeIoPending(base::test::TestFuture<OVERLAPPED*>& future) {
  return DoAll(SetArgPointee<4>(ERROR_IO_PENDING),
               WithArg<3>(base::test::InvokeFuture(future)), Return(FALSE));
}

auto ReturnControlIoPending(base::test::TestFuture<OVERLAPPED*>& future) {
  return DoAll(SetArgPointee<7>(ERROR_IO_PENDING),
               WithArg<6>(base::test::InvokeFuture(future)), Return(FALSE));
}

auto ReturnPipeError(DWORD error) {
  return DoAll(SetArgPointee<4>(error), Return(FALSE));
}

auto ReturnOverlappedResult(BOOL result,
                            DWORD bytes_transferred,
                            DWORD last_error) {
  return DoAll(SetArgPointee<2>(bytes_transferred),
               SetArgPointee<3>(last_error), Return(result));
}

class UsbRequestEventTest : public ::testing::Test,
                            public base::win::ObjectWatcher::Delegate {
 public:
  void OnObjectSignaled(HANDLE object) override {
    ++callback_count_;
    // Use nonblocking waits so an auto-reset regression fails, not hangs.
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(object, 0));
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(object, 0));
    run_loop_.Quit();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  int callback_count_ = 0;
};

TEST_F(UsbRequestEventTest, SignalBeforeWatching) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           TestTimeouts::action_timeout());
  auto event = CreateUsbRequestEvent();
  ASSERT_TRUE(event.is_valid());
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(event.Get(), 0));

  base::win::ObjectWatcher watcher;
  ASSERT_TRUE(SetEvent(event.Get()));
  ASSERT_TRUE(watcher.StartWatchingOnce(event.Get(), this));

  run_loop_.Run();
  EXPECT_FALSE(watcher.IsWatching());
  EXPECT_EQ(1, callback_count_);

  auto next_event = CreateUsbRequestEvent();
  ASSERT_TRUE(next_event.is_valid());
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(next_event.Get(), 0));
}

TEST_F(UsbRequestEventTest, SignalAfterWatching) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           TestTimeouts::action_timeout());
  auto event = CreateUsbRequestEvent();
  ASSERT_TRUE(event.is_valid());
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(event.Get(), 0));

  base::win::ObjectWatcher watcher;
  ASSERT_TRUE(watcher.StartWatchingOnce(event.Get(), this));
  ASSERT_TRUE(SetEvent(event.Get()));

  run_loop_.Run();
  EXPECT_FALSE(watcher.IsWatching());
  EXPECT_EQ(1, callback_count_);

  auto next_event = CreateUsbRequestEvent();
  ASSERT_TRUE(next_event.is_valid());
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(next_event.Get(), 0));
}

class TestUsbDeviceWin : public UsbDeviceWin {
 public:
  TestUsbDeviceWin()
      : UsbDeviceWin(
            /*device_path=*/L"\\\\?\\usb#vid_0000&pid_0000#test_device",
            /*hub_path=*/L"\\\\?\\usb#root_hub#test_hub",
            base::flat_map<int, FunctionInfo>{
                {0,
                 {0, L"winusb", L"\\\\?\\usb#vid_0000&pid_0000#test_device"}}},
            /*bus_number=*/1,
            /*port_number=*/1,
            DriverType::kWinUSB) {
    mojom::UsbConfigurationInfoPtr config = BuildUsbConfigurationInfoPtr(
        /*configuration_value=*/1, /*self_powered=*/false,
        /*remote_wakeup=*/false, /*maximum_power=*/0);
    mojom::UsbInterfaceInfoPtr interface_info = BuildUsbInterfaceInfoPtr(
        /*interface_number=*/0, /*alternate_setting=*/0,
        /*interface_class=*/0xff, /*interface_subclass=*/0,
        /*interface_protocol=*/0);
    interface_info->alternates[0]->endpoints.push_back(BuildUsbEndpointInfoPtr(
        kBulkInEndpointAddress, kBulkTransferAttributes,
        /*maximum_packet_size=*/64, /*polling_interval=*/0));
    interface_info->alternates[0]->endpoints.push_back(BuildUsbEndpointInfoPtr(
        kBulkOutEndpointAddress, kBulkTransferAttributes,
        /*maximum_packet_size=*/64, /*polling_interval=*/0));
    config->interfaces.push_back(std::move(interface_info));
    AssignFirstInterfaceNumbers(config.get());
    device_info_->configurations.push_back(std::move(config));
    ActiveConfigurationChanged(1);
  }

 private:
  ~TestUsbDeviceWin() override = default;
};

}  // namespace

class MockWinUsbDelegate : public UsbDeviceHandleWin::WinUsbDelegate {
 public:
  MockWinUsbDelegate() = default;
  ~MockWinUsbDelegate() override = default;

  MOCK_METHOD(base::win::ScopedHandle,
              OpenFunctionHandle,
              (const std::wstring& path),
              (override));
  MOCK_METHOD(BOOL,
              Initialize,
              (HANDLE device_handle, WINUSB_INTERFACE_HANDLE* interface_handle),
              (override));
  MOCK_METHOD(void,
              Free,
              (WINUSB_INTERFACE_HANDLE interface_handle),
              (override));
  MOCK_METHOD(BOOL, CancelIo, (HANDLE function_handle), (override));
  MOCK_METHOD(BOOL,
              ReadPipe,
              (WINUSB_INTERFACE_HANDLE interface_handle,
               uint8_t endpoint_address,
               base::span<uint8_t> buffer,
               OVERLAPPED* overlapped,
               DWORD* last_error),
              (override));
  MOCK_METHOD(BOOL,
              WritePipe,
              (WINUSB_INTERFACE_HANDLE interface_handle,
               uint8_t endpoint_address,
               base::span<uint8_t> buffer,
               OVERLAPPED* overlapped,
               DWORD* last_error),
              (override));
  MOCK_METHOD(BOOL,
              ControlTransfer,
              (WINUSB_INTERFACE_HANDLE interface_handle,
               uint8_t request_type,
               uint8_t request,
               uint16_t value,
               uint16_t index,
               base::span<uint8_t> buffer,
               OVERLAPPED* overlapped,
               DWORD* last_error),
              (override));
  MOCK_METHOD(BOOL,
              GetOverlappedResult,
              (WINUSB_INTERFACE_HANDLE interface_handle,
               OVERLAPPED* overlapped,
               DWORD* bytes_transferred,
               DWORD* last_error),
              (override));
};

class UsbDeviceHandleWinTest : public ::testing::Test {
 protected:
  using TransferResultFuture =
      base::test::TestFuture<mojom::UsbTransferStatus,
                             scoped_refptr<base::RefCountedBytes>,
                             size_t>;

  void SetUp() override {
    ON_CALL(delegate_, OpenFunctionHandle)
        .WillByDefault([](const std::wstring& path) {
          // Return a valid dummy HANDLE so ScopedHandle::is_valid() succeeds.
          return CreateUsbRequestEvent();
        });
    ON_CALL(delegate_, Initialize)
        .WillByDefault([](HANDLE device_handle,
                          WINUSB_INTERFACE_HANDLE* interface_handle) {
          *interface_handle = kFakeInterfaceHandle;
          return TRUE;
        });
    ON_CALL(delegate_, CancelIo).WillByDefault([](HANDLE function_handle) {
      EXPECT_NE(function_handle, nullptr);
      return TRUE;
    });
    UsbDeviceHandleWin::SetWinUsbDelegateForTesting(&delegate_);
  }

  void TearDown() override {
    if (handle_) {
      handle_->Close();
      handle_.reset();
    }
    device_.reset();
    UsbDeviceHandleWin::SetWinUsbDelegateForTesting(nullptr);
  }

  scoped_refptr<UsbDeviceHandle> OpenAndClaimInterface0() {
    device_ = base::MakeRefCounted<TestUsbDeviceWin>();
    base::test::TestFuture<scoped_refptr<UsbDeviceHandle>> open_future;
    device_->Open(open_future.GetCallback());
    handle_ = open_future.Take();
    EXPECT_TRUE(handle_);
    if (!handle_) {
      return nullptr;
    }

    base::test::TestFuture<bool> claim_future;
    handle_->ClaimInterface(0, claim_future.GetCallback());
    EXPECT_TRUE(claim_future.Get());
    return handle_;
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kSafeUsbDeviceHandleWinClose};
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedRunLoopTimeout timeout_{FROM_HERE,
                                            TestTimeouts::action_timeout()};
  NiceMock<MockWinUsbDelegate> delegate_;
  scoped_refptr<TestUsbDeviceWin> device_;
  scoped_refptr<UsbDeviceHandle> handle_;
};

TEST_F(UsbDeviceHandleWinTest,
       GenericTransferInRetainsBufferAcrossCloseUntilSignaled) {
  scoped_refptr<UsbDeviceHandle> handle = OpenAndClaimInterface0();
  ASSERT_TRUE(handle);

  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(64);
  ASSERT_TRUE(buffer->HasOneRef());

  base::test::TestFuture<base::span<uint8_t>, OVERLAPPED*> read_future;
  EXPECT_CALL(delegate_,
              ReadPipe(kFakeInterfaceHandle, kBulkInEndpointAddress, _, _, _))
      .WillOnce(ReturnPipeIoPending(read_future));
  TransferResultFuture transfer_future;
  handle->GenericTransfer(mojom::UsbTransferDirection::INBOUND,
                          kBulkEndpointNumber, buffer, /*timeout=*/5000,
                          transfer_future.GetCallback());
  auto [read_buffer, overlapped] = read_future.Take();
  HANDLE event = overlapped->hEvent;
  EXPECT_FALSE(buffer->HasOneRef());

  // Closing and releasing the handle aborts the transfer callback immediately,
  // while the detached `Request` must continue retaining `buffer` until the OS
  // signals overlapped completion.
  EXPECT_CALL(delegate_, CancelIo(_));
  EXPECT_CALL(delegate_, Free(kFakeInterfaceHandle));
  handle->Close();
  handle.reset();
  handle_.reset();

  EXPECT_EQ(transfer_future.Get<0>(), mojom::UsbTransferStatus::CANCELLED);
  EXPECT_EQ(transfer_future.Get<1>(), nullptr);
  EXPECT_EQ(transfer_future.Get<2>(), 0u);

  ASSERT_FALSE(buffer->HasOneRef());
  std::ranges::fill(read_buffer, 0x5a);
  read_buffer = {};

  ASSERT_TRUE(SetEvent(event));

  // Once the OS signals completion on the ObjectWatcher wait thread and the
  // posted `DeleteSoon` task runs, the detached `Request` releases `buffer`.
  EXPECT_TRUE(base::test::RunUntil([&] { return buffer->HasOneRef(); }));
}

TEST_F(UsbDeviceHandleWinTest,
       GenericTransferOutRetainsBufferAcrossCloseUntilSignaled) {
  scoped_refptr<UsbDeviceHandle> handle = OpenAndClaimInterface0();
  ASSERT_TRUE(handle);

  constexpr auto kPayload = std::to_array<uint8_t>({0x10, 0x20, 0x30, 0x40});
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(kPayload);
  ASSERT_TRUE(buffer->HasOneRef());

  base::test::TestFuture<base::span<uint8_t>, OVERLAPPED*> write_future;
  EXPECT_CALL(delegate_,
              WritePipe(kFakeInterfaceHandle, kBulkOutEndpointAddress, _, _, _))
      .WillOnce(ReturnPipeIoPending(write_future));
  TransferResultFuture transfer_future;
  handle->GenericTransfer(mojom::UsbTransferDirection::OUTBOUND,
                          kBulkEndpointNumber, buffer, /*timeout=*/5000,
                          transfer_future.GetCallback());
  auto [write_buffer, overlapped] = write_future.Take();
  HANDLE event = overlapped->hEvent;
  EXPECT_FALSE(buffer->HasOneRef());

  handle->Close();
  handle.reset();
  handle_.reset();

  EXPECT_EQ(transfer_future.Get<0>(), mojom::UsbTransferStatus::CANCELLED);
  EXPECT_EQ(transfer_future.Get<1>(), nullptr);
  EXPECT_EQ(transfer_future.Get<2>(), 0u);

  ASSERT_FALSE(buffer->HasOneRef());
  EXPECT_EQ(write_buffer, base::span(kPayload));
  write_buffer = {};

  ASSERT_TRUE(SetEvent(event));
  EXPECT_TRUE(base::test::RunUntil([&] { return buffer->HasOneRef(); }));
}

TEST_F(UsbDeviceHandleWinTest,
       ControlTransferRetainsBufferAcrossCloseUntilSignaled) {
  scoped_refptr<UsbDeviceHandle> handle = OpenAndClaimInterface0();
  ASSERT_TRUE(handle);

  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(32);
  ASSERT_TRUE(buffer->HasOneRef());

  base::test::TestFuture<OVERLAPPED*> control_future;
  EXPECT_CALL(delegate_,
              ControlTransfer(kFakeInterfaceHandle, _, _, _, _, _, _, _))
      .WillOnce(ReturnControlIoPending(control_future));
  TransferResultFuture transfer_future;
  handle->ControlTransfer(mojom::UsbTransferDirection::INBOUND,
                          mojom::UsbControlTransferType::VENDOR,
                          mojom::UsbControlTransferRecipient::DEVICE,
                          /*request=*/0x01, /*value=*/0, /*index=*/0, buffer,
                          /*timeout=*/5000, transfer_future.GetCallback());
  HANDLE event = control_future.Take()->hEvent;
  EXPECT_FALSE(buffer->HasOneRef());

  handle->Close();
  handle.reset();
  handle_.reset();

  EXPECT_EQ(transfer_future.Get<0>(), mojom::UsbTransferStatus::CANCELLED);
  EXPECT_EQ(transfer_future.Get<1>(), nullptr);
  EXPECT_EQ(transfer_future.Get<2>(), 0u);
  EXPECT_FALSE(buffer->HasOneRef());

  ASSERT_TRUE(SetEvent(event));
  EXPECT_TRUE(base::test::RunUntil([&] { return buffer->HasOneRef(); }));
}

TEST_F(UsbDeviceHandleWinTest, GenericTransferAsyncAndSyncCompletion) {
  scoped_refptr<UsbDeviceHandle> handle = OpenAndClaimInterface0();
  ASSERT_TRUE(handle);

  // 1. Asynchronous INBOUND completion transfers exclusive buffer ownership.
  auto in_buffer = base::MakeRefCounted<base::RefCountedBytes>(16);
  constexpr auto kResponse = std::to_array<uint8_t>({1, 2, 3, 4, 5});
  base::test::TestFuture<base::span<uint8_t>, OVERLAPPED*> read_future;
  EXPECT_CALL(delegate_,
              ReadPipe(kFakeInterfaceHandle, kBulkInEndpointAddress, _, _, _))
      .WillOnce(ReturnPipeIoPending(read_future));
  TransferResultFuture in_future;
  handle->GenericTransfer(mojom::UsbTransferDirection::INBOUND,
                          kBulkEndpointNumber, std::move(in_buffer),
                          /*timeout=*/5000, in_future.GetCallback());
  auto [read_buffer, overlapped] = read_future.Take();

  read_buffer.copy_prefix_from(kResponse);
  read_buffer = {};
  EXPECT_CALL(delegate_, GetOverlappedResult(kFakeInterfaceHandle, _, _, _))
      .WillOnce(ReturnOverlappedResult(
          TRUE, static_cast<DWORD>(kResponse.size()), ERROR_SUCCESS));
  ASSERT_TRUE(SetEvent(overlapped->hEvent));
  auto [in_status, completed_in_buffer, in_bytes] = in_future.Take();
  EXPECT_EQ(in_status, mojom::UsbTransferStatus::COMPLETED);
  ASSERT_NE(completed_in_buffer, nullptr);
  EXPECT_EQ(in_bytes, kResponse.size());
  EXPECT_EQ(base::span(*completed_in_buffer).first(in_bytes),
            base::span(kResponse));
  EXPECT_TRUE(completed_in_buffer->HasOneRef());

  // 2. Synchronous OUTBOUND completion transfers exclusive buffer ownership.
  constexpr auto kPayload = std::to_array<uint8_t>({9, 8, 7, 6});
  auto out_buffer = base::MakeRefCounted<base::RefCountedBytes>(kPayload);

  base::test::TestFuture<std::vector<uint8_t>> written_bytes_future;
  EXPECT_CALL(delegate_,
              WritePipe(kFakeInterfaceHandle, kBulkOutEndpointAddress, _, _, _))
      .WillOnce([&](WINUSB_INTERFACE_HANDLE interface_handle,
                    uint8_t endpoint_address, base::span<uint8_t> buffer,
                    OVERLAPPED* overlapped, DWORD* last_error) {
        written_bytes_future.SetValue(
            std::vector<uint8_t>(buffer.begin(), buffer.end()));
        *last_error = ERROR_SUCCESS;
        SetEvent(overlapped->hEvent);
        return TRUE;
      });
  EXPECT_CALL(delegate_, GetOverlappedResult(kFakeInterfaceHandle, _, _, _))
      .WillOnce(ReturnOverlappedResult(
          TRUE, static_cast<DWORD>(kPayload.size()), ERROR_SUCCESS));

  TransferResultFuture out_future;
  handle->GenericTransfer(mojom::UsbTransferDirection::OUTBOUND,
                          kBulkEndpointNumber, std::move(out_buffer),
                          /*timeout=*/5000, out_future.GetCallback());
  EXPECT_EQ(base::span(written_bytes_future.Get()), base::span(kPayload));

  auto [out_status, completed_out_buffer, out_bytes] = out_future.Take();
  EXPECT_EQ(out_status, mojom::UsbTransferStatus::COMPLETED);
  ASSERT_NE(completed_out_buffer, nullptr);
  EXPECT_EQ(out_bytes, kPayload.size());
  EXPECT_TRUE(completed_out_buffer->HasOneRef());

  handle->Close();
}

TEST_F(UsbDeviceHandleWinTest, GenericTransferErrorsReleaseBuffer) {
  scoped_refptr<UsbDeviceHandle> handle = OpenAndClaimInterface0();
  ASSERT_TRUE(handle);

  // 1. Immediate WinUSB error releases `Request`'s buffer reference.
  EXPECT_CALL(delegate_,
              ReadPipe(kFakeInterfaceHandle, kBulkInEndpointAddress, _, _, _))
      .WillOnce(ReturnPipeError(ERROR_GEN_FAILURE));
  auto sync_err_buffer = base::MakeRefCounted<base::RefCountedBytes>(32);
  TransferResultFuture sync_err_future;
  handle->GenericTransfer(mojom::UsbTransferDirection::INBOUND,
                          kBulkEndpointNumber, sync_err_buffer,
                          /*timeout=*/5000, sync_err_future.GetCallback());
  EXPECT_EQ(sync_err_future.Get<0>(), mojom::UsbTransferStatus::TRANSFER_ERROR);
  EXPECT_EQ(sync_err_future.Get<1>(), nullptr);
  EXPECT_EQ(sync_err_future.Get<2>(), 0u);
  EXPECT_TRUE(sync_err_buffer->HasOneRef());

  // 2. FALSE return with ERROR_SUCCESS still reports TRANSFER_ERROR, never
  // COMPLETED with a null buffer.
  EXPECT_CALL(delegate_,
              ReadPipe(kFakeInterfaceHandle, kBulkInEndpointAddress, _, _, _))
      .WillOnce(ReturnPipeError(ERROR_SUCCESS));
  auto clamped_err_buffer = base::MakeRefCounted<base::RefCountedBytes>(32);
  TransferResultFuture clamped_err_future;
  handle->GenericTransfer(mojom::UsbTransferDirection::INBOUND,
                          kBulkEndpointNumber, clamped_err_buffer,
                          /*timeout=*/5000, clamped_err_future.GetCallback());
  EXPECT_EQ(clamped_err_future.Get<0>(),
            mojom::UsbTransferStatus::TRANSFER_ERROR);
  EXPECT_EQ(clamped_err_future.Get<1>(), nullptr);
  EXPECT_TRUE(clamped_err_buffer->HasOneRef());

  // 3. Asynchronous `GetOverlappedResult()` failure (including ERROR_SUCCESS
  // clamping) releases `Request`'s buffer reference and reports TRANSFER_ERROR.
  base::test::TestFuture<OVERLAPPED*> async_read_future;
  EXPECT_CALL(delegate_,
              ReadPipe(kFakeInterfaceHandle, kBulkInEndpointAddress, _, _, _))
      .WillOnce(ReturnPipeIoPending(async_read_future));
  auto async_err_buffer = base::MakeRefCounted<base::RefCountedBytes>(32);
  TransferResultFuture async_err_future;
  handle->GenericTransfer(mojom::UsbTransferDirection::INBOUND,
                          kBulkEndpointNumber, async_err_buffer,
                          /*timeout=*/5000, async_err_future.GetCallback());
  OVERLAPPED* overlapped = async_read_future.Take();
  EXPECT_FALSE(async_err_buffer->HasOneRef());

  EXPECT_CALL(delegate_, GetOverlappedResult(kFakeInterfaceHandle, _, _, _))
      .WillOnce(ReturnOverlappedResult(FALSE, 0, ERROR_SUCCESS));
  ASSERT_TRUE(SetEvent(overlapped->hEvent));
  EXPECT_EQ(async_err_future.Get<0>(),
            mojom::UsbTransferStatus::TRANSFER_ERROR);
  EXPECT_EQ(async_err_future.Get<1>(), nullptr);
  EXPECT_EQ(async_err_future.Get<2>(), 0u);
  EXPECT_TRUE(async_err_buffer->HasOneRef());

  handle->Close();
}

TEST_F(UsbDeviceHandleWinTest,
       CloseReleasesBufferImmediatelyWhenSafeCloseDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kSafeUsbDeviceHandleWinClose);

  scoped_refptr<UsbDeviceHandle> handle = OpenAndClaimInterface0();
  ASSERT_TRUE(handle);

  base::test::TestFuture<OVERLAPPED*> read_future;
  EXPECT_CALL(delegate_,
              ReadPipe(kFakeInterfaceHandle, kBulkInEndpointAddress, _, _, _))
      .WillOnce(ReturnPipeIoPending(read_future));

  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(32);
  TransferResultFuture transfer_future;
  handle->GenericTransfer(mojom::UsbTransferDirection::INBOUND,
                          kBulkEndpointNumber, buffer, /*timeout=*/5000,
                          transfer_future.GetCallback());
  EXPECT_TRUE(read_future.IsReady());
  EXPECT_FALSE(buffer->HasOneRef());

  handle->Close();
  handle.reset();
  handle_.reset();

  EXPECT_EQ(transfer_future.Get<0>(), mojom::UsbTransferStatus::CANCELLED);
  EXPECT_EQ(transfer_future.Get<1>(), nullptr);
  EXPECT_TRUE(buffer->HasOneRef());
}

TEST_F(UsbDeviceHandleWinTest, CloseCalledReentrantlyFromTransferCallback) {
  scoped_refptr<UsbDeviceHandle> handle = OpenAndClaimInterface0();
  ASSERT_TRUE(handle);
  handle_.reset();

  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(16);
  constexpr auto kPayload = std::to_array<uint8_t>({0xaa, 0xbb});
  base::test::TestFuture<base::span<uint8_t>, OVERLAPPED*> read_future;
  EXPECT_CALL(delegate_,
              ReadPipe(kFakeInterfaceHandle, kBulkInEndpointAddress, _, _, _))
      .WillOnce(ReturnPipeIoPending(read_future));
  TransferResultFuture result_future;

  handle->GenericTransfer(
      mojom::UsbTransferDirection::INBOUND, kBulkEndpointNumber,
      std::move(buffer), /*timeout=*/5000,
      base::BindLambdaForTesting(
          [&](mojom::UsbTransferStatus status,
              scoped_refptr<base::RefCountedBytes> completed_buffer,
              size_t bytes_transferred) {
            handle->Close();
            handle.reset();
            result_future.SetValue(status, std::move(completed_buffer),
                                   bytes_transferred);
          }));
  auto [read_buffer, overlapped] = read_future.Take();

  read_buffer.copy_prefix_from(kPayload);
  read_buffer = {};
  EXPECT_CALL(delegate_, GetOverlappedResult(kFakeInterfaceHandle, _, _, _))
      .WillOnce(ReturnOverlappedResult(
          TRUE, static_cast<DWORD>(kPayload.size()), ERROR_SUCCESS));
  ASSERT_TRUE(SetEvent(overlapped->hEvent));
  auto [status, completed_buffer, bytes_transferred] = result_future.Take();
  EXPECT_EQ(status, mojom::UsbTransferStatus::COMPLETED);
  ASSERT_NE(completed_buffer, nullptr);
  EXPECT_EQ(bytes_transferred, kPayload.size());
  EXPECT_TRUE(completed_buffer->HasOneRef());
}

}  // namespace device
