// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_connection_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/device_service_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

#if BUILDFLAG(IS_MAC)
const uint64_t kTestDeviceId = 123;
#elif BUILDFLAG(IS_WIN)
const wchar_t* kTestDeviceId = L"123";
#else
const char* kTestDeviceId = "123";
#endif

// The report ID to use for reports sent to or received from the test device.
const uint8_t kTestReportId = 0x42;

// The max size of input and output reports for the test device. Feature reports
// are not used in this test.
const uint64_t kMaxReportSizeBytes = 10;

// A fake HidConnection implementation that allows the test to simulate an
// input report.
class FakeHidConnection : public HidConnection {
 public:
  explicit FakeHidConnection(scoped_refptr<HidDeviceInfo> device)
      : HidConnection(device,
                      /*allow_protected_reports=*/false,
                      /*allow_fido_reports=*/false) {}
  FakeHidConnection(const FakeHidConnection&) = delete;
  FakeHidConnection& operator=(const FakeHidConnection&) = delete;

  // HidConnection implementation.
  void PlatformClose() override {}
  void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                     WriteCallback callback) override {
    std::move(callback).Run(true);
  }
  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override {
    NOTIMPLEMENTED();
  }
  void PlatformSendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                                 WriteCallback callback) override {
    NOTIMPLEMENTED();
  }

  void SimulateInputReport(scoped_refptr<base::RefCountedBytes> buffer) {
    ProcessInputReport(buffer, buffer->size());
  }

 private:
  ~FakeHidConnection() override = default;
};

// A test implementation of HidConnectionClient that signals once an input
// report has been received. The contents of the input report are saved.
class TestHidConnectionClient : public mojom::HidConnectionClient {
 public:
  TestHidConnectionClient() = default;
  TestHidConnectionClient(const TestHidConnectionClient&) = delete;
  TestHidConnectionClient& operator=(const TestHidConnectionClient&) = delete;
  ~TestHidConnectionClient() override = default;

  void Bind(mojo::PendingReceiver<mojom::HidConnectionClient> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // mojom::HidConnectionClient implementation.
  void OnInputReport(uint8_t report_id,
                     const std::vector<uint8_t>& buffer) override {
    report_id_ = report_id;
    buffer_ = buffer;
    run_loop_.Quit();
  }

  void WaitForInputReport() { run_loop_.Run(); }

  uint8_t report_id() { return report_id_; }
  const std::vector<uint8_t>& buffer() { return buffer_; }

 private:
  base::RunLoop run_loop_;
  mojo::Receiver<mojom::HidConnectionClient> receiver_{this};
  uint8_t report_id_ = 0;
  std::vector<uint8_t> buffer_;
};

// A utility for capturing the state returned by mojom::HidConnection I/O
// callbacks.
class TestIoCallback {
 public:
  TestIoCallback() = default;
  TestIoCallback(const TestIoCallback&) = delete;
  TestIoCallback& operator=(const TestIoCallback&) = delete;
  ~TestIoCallback() = default;

  void SetReadResult(bool result,
                     uint8_t report_id,
                     const absl::optional<std::vector<uint8_t>>& buffer) {
    result_ = result;
    report_id_ = report_id;
    has_buffer_ = buffer.has_value();
    if (has_buffer_)
      buffer_ = *buffer;
    run_loop_.Quit();
  }

  void SetWriteResult(bool result) {
    result_ = result;
    run_loop_.Quit();
  }

  bool WaitForResult() {
    run_loop_.Run();
    return result_;
  }

  mojom::HidConnection::ReadCallback GetReadCallback() {
    return base::BindOnce(&TestIoCallback::SetReadResult,
                          base::Unretained(this));
  }

  mojom::HidConnection::WriteCallback GetWriteCallback() {
    return base::BindOnce(&TestIoCallback::SetWriteResult,
                          base::Unretained(this));
  }

  uint8_t report_id() { return report_id_; }
  bool has_buffer() { return has_buffer_; }
  const std::vector<uint8_t>& buffer() { return buffer_; }

 private:
  base::RunLoop run_loop_;
  bool result_ = false;
  uint8_t report_id_ = 0;
  bool has_buffer_ = false;
  std::vector<uint8_t> buffer_;
};

}  // namespace

class HidConnectionImplTest : public DeviceServiceTestBase {
 public:
  HidConnectionImplTest() = default;
  HidConnectionImplTest(HidConnectionImplTest&) = delete;
  HidConnectionImplTest& operator=(HidConnectionImplTest&) = delete;

 protected:
  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    base::RunLoop().RunUntilIdle();
  }

  void CreateHidConnection(bool with_connection_client) {
    mojo::PendingRemote<mojom::HidConnectionClient> hid_connection_client;
    if (with_connection_client) {
      connection_client_ = std::make_unique<TestHidConnectionClient>();
      connection_client_->Bind(
          hid_connection_client.InitWithNewPipeAndPassReceiver());
    }
    fake_connection_ = new FakeHidConnection(CreateTestDevice());
    hid_connection_impl_ = new HidConnectionImpl(
        fake_connection_, hid_connection_.InitWithNewPipeAndPassReceiver(),
        std::move(hid_connection_client),
        /*watcher=*/mojo::NullRemote());
  }

  scoped_refptr<HidDeviceInfo> CreateTestDevice() {
    auto collection = mojom::HidCollectionInfo::New();
    collection->usage = mojom::HidUsageAndPage::New(0, 0);
    collection->report_ids.push_back(kTestReportId);
    return base::MakeRefCounted<HidDeviceInfo>(
        kTestDeviceId, /*physical_device_id=*/"1", "interface id",
        /*vendor_id=*/0x1234, /*product_id=*/0xabcd, "product name",
        "serial number", mojom::HidBusType::kHIDBusTypeUSB,
        std::move(collection), kMaxReportSizeBytes, kMaxReportSizeBytes,
        /*max_feature_report_size=*/0);
  }

  std::vector<uint8_t> CreateTestReportBuffer(uint8_t report_id, size_t size) {
    std::vector<uint8_t> buffer(size);
    buffer[0] = report_id;
    for (size_t i = 1; i < size; ++i)
      buffer[i] = i;
    return buffer;
  }

  mojo::PendingRemote<mojom::HidConnection> hid_connection_;
  raw_ptr<HidConnectionImpl>
      hid_connection_impl_;  // Owned by |hid_connection_|.
  scoped_refptr<FakeHidConnection> fake_connection_;
  std::unique_ptr<TestHidConnectionClient> connection_client_;
};

TEST_F(HidConnectionImplTest, ReadWrite) {
  CreateHidConnection(/*with_connection_client=*/false);
  const size_t kTestBufferSize = kMaxReportSizeBytes;
  std::vector<uint8_t> buffer_vec =
      CreateTestReportBuffer(kTestReportId, kTestBufferSize);

  // Simulate an output report (host to device).
  TestIoCallback write_callback;
  hid_connection_impl_->Write(kTestReportId, buffer_vec,
                              write_callback.GetWriteCallback());
  ASSERT_TRUE(write_callback.WaitForResult());

  // Simulate an input report (device to host).
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(buffer_vec);
  ASSERT_EQ(buffer->size(), kTestBufferSize);
  fake_connection_->SimulateInputReport(buffer);

  // Simulate reading the input report.
  TestIoCallback read_callback;
  hid_connection_impl_->Read(read_callback.GetReadCallback());
  ASSERT_TRUE(read_callback.WaitForResult());
  EXPECT_EQ(read_callback.report_id(), kTestReportId);
  ASSERT_TRUE(read_callback.has_buffer());
  const auto& read_buffer = read_callback.buffer();
  ASSERT_EQ(read_buffer.size(), kTestBufferSize - 1);
  for (size_t i = 1; i < kTestBufferSize; ++i) {
    EXPECT_EQ(read_buffer[i - 1], buffer_vec[i])
        << "Mismatch at index " << i << ".";
  }
}

TEST_F(HidConnectionImplTest, ReadWriteWithConnectionClient) {
  CreateHidConnection(/*with_connection_client=*/true);
  const size_t kTestBufferSize = kMaxReportSizeBytes;
  std::vector<uint8_t> buffer_vec =
      CreateTestReportBuffer(kTestReportId, kTestBufferSize);

  // Simulate an output report (host to device).
  TestIoCallback write_callback;
  hid_connection_impl_->Write(kTestReportId, buffer_vec,
                              write_callback.GetWriteCallback());
  ASSERT_TRUE(write_callback.WaitForResult());

  // Simulate an input report (device to host).
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(buffer_vec);
  ASSERT_EQ(buffer->size(), kTestBufferSize);
  fake_connection_->SimulateInputReport(buffer);
  connection_client_->WaitForInputReport();

  // The connection client should have been notified.
  EXPECT_EQ(connection_client_->report_id(), kTestReportId);
  const std::vector<uint8_t>& in_buffer = connection_client_->buffer();
  ASSERT_EQ(in_buffer.size(), kTestBufferSize - 1);
  for (size_t i = 1; i < kTestBufferSize; ++i) {
    EXPECT_EQ(in_buffer[i - 1], buffer_vec[i])
        << "Mismatch at index " << i << ".";
  }
}

TEST_F(HidConnectionImplTest, DestroyWithPendingInputReport) {
  CreateHidConnection(/*with_connection_client=*/false);
  const size_t kTestBufferSize = kMaxReportSizeBytes;
  std::vector<uint8_t> buffer_vec =
      CreateTestReportBuffer(kTestReportId, kTestBufferSize);

  // Simulate an input report (device to host).
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(buffer_vec);
  ASSERT_EQ(buffer->size(), kTestBufferSize);
  fake_connection_->SimulateInputReport(buffer);

  // Destroy the connection without reading the report.
  hid_connection_.reset();
}

TEST_F(HidConnectionImplTest, DestroyWithPendingRead) {
  CreateHidConnection(/*with_connection_client=*/false);

  // Simulate reading an input report.
  TestIoCallback read_callback;
  hid_connection_impl_->Read(read_callback.GetReadCallback());

  // Destroy the connection without receiving an input report.
  hid_connection_.reset();
}

TEST_F(HidConnectionImplTest, WatcherClosedWhenHidConnectionClosed) {
  mojo::PendingRemote<mojom::HidConnectionWatcher> watcher;
  auto watcher_receiver = mojo::MakeSelfOwnedReceiver(
      std::make_unique<mojom::HidConnectionWatcher>(),
      watcher.InitWithNewPipeAndPassReceiver());

  mojo::Remote<mojom::HidConnection> hid_connection;
  HidConnectionImpl::Create(
      base::MakeRefCounted<FakeHidConnection>(CreateTestDevice()),
      hid_connection.BindNewPipeAndPassReceiver(),
      /*connection_client=*/mojo::NullRemote(), std::move(watcher));

  // To start with both the HID connection and the connection watcher connection
  // should remain open.
  hid_connection.FlushForTesting();
  EXPECT_TRUE(hid_connection.is_connected());
  watcher_receiver->FlushForTesting();
  EXPECT_TRUE(watcher_receiver);

  // When the HID connection is closed the watcher connection should be closed.
  hid_connection.reset();
  watcher_receiver->FlushForTesting();
  EXPECT_FALSE(watcher_receiver);
}

TEST_F(HidConnectionImplTest, HidConnectionClosedWhenWatcherClosed) {
  mojo::PendingRemote<mojom::HidConnectionWatcher> watcher;
  auto watcher_receiver = mojo::MakeSelfOwnedReceiver(
      std::make_unique<mojom::HidConnectionWatcher>(),
      watcher.InitWithNewPipeAndPassReceiver());

  mojo::Remote<mojom::HidConnection> hid_connection;
  HidConnectionImpl::Create(
      base::MakeRefCounted<FakeHidConnection>(CreateTestDevice()),
      hid_connection.BindNewPipeAndPassReceiver(),
      /*connection_client=*/mojo::NullRemote(), std::move(watcher));

  // To start with both the HID connection and the connection watcher connection
  // should remain open.
  hid_connection.FlushForTesting();
  EXPECT_TRUE(hid_connection.is_connected());
  watcher_receiver->FlushForTesting();
  EXPECT_TRUE(watcher_receiver);

  // When the watcher connection is closed, for safety, the HID connection
  // should also be closed.
  watcher_receiver->Close();
  hid_connection.FlushForTesting();
  EXPECT_FALSE(hid_connection.is_connected());
}

}  // namespace device
