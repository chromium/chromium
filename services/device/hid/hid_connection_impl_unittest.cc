// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_connection_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/device_service_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::ElementsAre;

using ReadFuture = base::test::
    TestFuture<bool, uint8_t, const std::optional<std::vector<uint8_t>>&>;
using WriteFuture = base::test::TestFuture<bool>;
using GetFeatureFuture =
    base::test::TestFuture<bool, const std::optional<std::vector<uint8_t>>&>;

#if BUILDFLAG(IS_MAC)
const uint64_t kTestDeviceId = 123;
#elif BUILDFLAG(IS_WIN)
const wchar_t* kTestDeviceId = L"123";
#else
const char* kTestDeviceId = "123";
#endif

// The report ID to use for reports sent to or received from the test device.
const uint8_t kTestReportId = 0x42;

// The max size of reports for the test device.
const uint64_t kMaxReportSizeBytes = 10;

// A mock HidConnection implementation that allows the test to simulate reports.
class MockHidConnection : public HidConnection {
 public:
  explicit MockHidConnection(scoped_refptr<HidDeviceInfo> device)
      : HidConnection(device,
                      /*allow_protected_reports=*/false,
                      /*allow_fido_reports=*/false) {}
  MockHidConnection(const MockHidConnection&) = delete;
  MockHidConnection& operator=(const MockHidConnection&) = delete;

  // HidConnection implementation.
  void PlatformClose() override {}
  MOCK_METHOD2(PlatformWrite,
               void(scoped_refptr<base::RefCountedBytes>, WriteCallback));
  MOCK_METHOD2(PlatformGetFeatureReport, void(uint8_t, ReadCallback));
  MOCK_METHOD2(PlatformSendFeatureReport,
               void(scoped_refptr<base::RefCountedBytes>, WriteCallback));

  void SimulateInputReport(scoped_refptr<base::RefCountedBytes> buffer) {
    ProcessInputReport(buffer, buffer->size());
  }

 private:
  ~MockHidConnection() override = default;
};

// An implementation of HidConnectionClient that enables the test to wait until
// an input report is received.
class TestHidConnectionClient : public mojom::HidConnectionClient {
 public:
  TestHidConnectionClient() = default;
  TestHidConnectionClient(const TestHidConnectionClient&) = delete;
  TestHidConnectionClient& operator=(const TestHidConnectionClient&) = delete;
  ~TestHidConnectionClient() override = default;

  void Bind(mojo::PendingReceiver<mojom::HidConnectionClient> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void OnInputReport(uint8_t report_id,
                     const std::vector<uint8_t>& buffer) override {
    future_.SetValue(report_id, buffer);
  }

  std::pair<uint8_t, std::vector<uint8_t>> GetNextInputReport() {
    return future_.Take();
  }

 private:
  mojo::Receiver<mojom::HidConnectionClient> receiver_{this};
  base::test::TestFuture<uint8_t, std::vector<uint8_t>> future_;
};

}  // namespace

class HidConnectionImplTest : public DeviceServiceTestBase {
 public:
  HidConnectionImplTest() = default;
  HidConnectionImplTest(const HidConnectionImplTest&) = delete;
  HidConnectionImplTest& operator=(const HidConnectionImplTest&) = delete;

 protected:
  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // HidConnectionImpl is self-owned and will self-destruct when its mojo pipe
    // is disconnected. Allow disconnect handlers to run so HidConnectionImpl
    // can self-destruct before the end of the test.
    base::RunLoop().RunUntilIdle();
  }

  mojo::Remote<mojom::HidConnection> CreateHidConnection(
      bool with_connection_client) {
    mojo::PendingRemote<mojom::HidConnectionClient> hid_connection_client;
    if (with_connection_client) {
      connection_client_ = std::make_unique<TestHidConnectionClient>();
      connection_client_->Bind(
          hid_connection_client.InitWithNewPipeAndPassReceiver());
    }
    mock_connection_ = new MockHidConnection(CreateTestDevice());
    mojo::Remote<mojom::HidConnection> hid_connection;
    HidConnectionImpl::Create(mock_connection_,
                              hid_connection.BindNewPipeAndPassReceiver(),
                              std::move(hid_connection_client),
                              /*watcher=*/mojo::NullRemote());
    return hid_connection;
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
        kMaxReportSizeBytes);
  }

  std::vector<uint8_t> CreateTestReportBuffer(uint8_t report_id, size_t size) {
    std::vector<uint8_t> buffer(size);
    buffer[0] = report_id;
    for (size_t i = 1; i < size; ++i) {
      buffer[i] = i;
    }
    return buffer;
  }

  MockHidConnection& mock_connection() { return *mock_connection_.get(); }
  TestHidConnectionClient& connection_client() { return *connection_client_; }

 private:
  scoped_refptr<MockHidConnection> mock_connection_;
  std::unique_ptr<TestHidConnectionClient> connection_client_;
};

TEST_F(HidConnectionImplTest, ReadWrite) {
  auto hid_connection = CreateHidConnection(/*with_connection_client=*/false);
  const size_t kTestBufferSize = kMaxReportSizeBytes;
  std::vector<uint8_t> buffer_vec =
      CreateTestReportBuffer(kTestReportId, kTestBufferSize);

  // Simulate an output report (host to device).
  EXPECT_CALL(mock_connection(), PlatformWrite)
      .WillOnce([](scoped_refptr<base::RefCountedBytes> buffer,
                   HidConnectionImpl::WriteCallback callback) {
        std::move(callback).Run(/*success=*/true);
      });
  WriteFuture write_future;
  hid_connection->Write(kTestReportId, buffer_vec, write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());

  // Simulate an input report (device to host).
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(buffer_vec);
  ASSERT_EQ(buffer->size(), kTestBufferSize);
  mock_connection().SimulateInputReport(buffer);

  // Simulate reading the input report.
  ReadFuture read_future;
  hid_connection->Read(read_future.GetCallback());
  EXPECT_TRUE(read_future.Get<0>());
  EXPECT_EQ(read_future.Get<1>(), kTestReportId);
  ASSERT_TRUE(read_future.Get<2>().has_value());
  const auto& read_buffer = read_future.Get<2>().value();
  ASSERT_EQ(read_buffer.size(), kTestBufferSize - 1);
  for (size_t i = 1; i < kTestBufferSize; ++i) {
    EXPECT_EQ(read_buffer[i - 1], buffer_vec[i])
        << "Mismatch at index " << i << ".";
  }
}

TEST_F(HidConnectionImplTest, ReadWriteWithConnectionClient) {
  auto hid_connection = CreateHidConnection(/*with_connection_client=*/true);
  const size_t kTestBufferSize = kMaxReportSizeBytes;
  std::vector<uint8_t> buffer_vec =
      CreateTestReportBuffer(kTestReportId, kTestBufferSize);

  // Simulate an output report (host to device).
  EXPECT_CALL(mock_connection(), PlatformWrite)
      .WillOnce([](scoped_refptr<base::RefCountedBytes> buffer,
                   HidConnectionImpl::WriteCallback callback) {
        std::move(callback).Run(/*success=*/true);
      });
  WriteFuture write_future;
  hid_connection->Write(kTestReportId, buffer_vec, write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());

  // Simulate an input report (device to host).
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(buffer_vec);
  ASSERT_EQ(buffer->size(), kTestBufferSize);
  mock_connection().SimulateInputReport(buffer);
  auto [report_id, in_buffer] = connection_client().GetNextInputReport();

  // The connection client should have been notified.
  EXPECT_EQ(report_id, kTestReportId);
  ASSERT_EQ(in_buffer.size(), kTestBufferSize - 1);
  for (size_t i = 1; i < kTestBufferSize; ++i) {
    EXPECT_EQ(in_buffer[i - 1], buffer_vec[i])
        << "Mismatch at index " << i << ".";
  }
}

TEST_F(HidConnectionImplTest, DestroyWithPendingInputReport) {
  auto hid_connection = CreateHidConnection(/*with_connection_client=*/false);
  const size_t kTestBufferSize = kMaxReportSizeBytes;
  std::vector<uint8_t> buffer_vec =
      CreateTestReportBuffer(kTestReportId, kTestBufferSize);

  // Simulate an input report (device to host).
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(buffer_vec);
  ASSERT_EQ(buffer->size(), kTestBufferSize);
  mock_connection().SimulateInputReport(buffer);

  // Destroy the connection without reading the report.
  hid_connection.reset();
}

TEST_F(HidConnectionImplTest, DestroyWithPendingRead) {
  auto hid_connection = CreateHidConnection(/*with_connection_client=*/false);

  // Simulate reading an input report.
  hid_connection->Read(base::DoNothing());

  // Destroy the connection without receiving an input report.
  hid_connection.reset();
}

TEST_F(HidConnectionImplTest, WatcherClosedWhenHidConnectionClosed) {
  mojo::PendingRemote<mojom::HidConnectionWatcher> watcher;
  auto watcher_receiver = mojo::MakeSelfOwnedReceiver(
      std::make_unique<mojom::HidConnectionWatcher>(),
      watcher.InitWithNewPipeAndPassReceiver());

  mojo::Remote<mojom::HidConnection> hid_connection;
  HidConnectionImpl::Create(
      base::MakeRefCounted<MockHidConnection>(CreateTestDevice()),
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
      base::MakeRefCounted<MockHidConnection>(CreateTestDevice()),
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

TEST_F(HidConnectionImplTest, ReadZeroLengthInputReport) {
  auto hid_connection = CreateHidConnection(/*with_connection_client=*/false);
  mock_connection().SimulateInputReport(
      base::MakeRefCounted<base::RefCountedBytes>(
          CreateTestReportBuffer(kTestReportId, /*size=*/1u)));
  ReadFuture read_future;
  hid_connection->Read(read_future.GetCallback());
  EXPECT_TRUE(read_future.Get<0>());
  EXPECT_EQ(read_future.Get<1>(), kTestReportId);
  ASSERT_TRUE(read_future.Get<2>().has_value());
  EXPECT_EQ(read_future.Get<2>().value().size(), 0u);
}

TEST_F(HidConnectionImplTest, ReadZeroLengthInputReportWithClient) {
  auto hid_connection = CreateHidConnection(/*with_connection_client=*/true);
  mock_connection().SimulateInputReport(
      base::MakeRefCounted<base::RefCountedBytes>(
          CreateTestReportBuffer(kTestReportId, /*size=*/1u)));
  auto [report_id, in_buffer] = connection_client().GetNextInputReport();
  EXPECT_EQ(report_id, kTestReportId);
  EXPECT_EQ(in_buffer.size(), 0u);
}

TEST_F(HidConnectionImplTest, WriteZeroLengthOutputReport) {
  auto hid_connection = CreateHidConnection(/*with_connection_client=*/false);
  EXPECT_CALL(mock_connection(), PlatformWrite)
      .WillOnce([](scoped_refptr<base::RefCountedBytes> buffer,
                   HidConnectionImpl::WriteCallback callback) {
        std::move(callback).Run(/*success=*/true);
      });
  WriteFuture write_future;
  hid_connection->Write(kTestReportId, /*buffer=*/{},
                        write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());
}

TEST_F(HidConnectionImplTest, ReadZeroLengthFeatureReport) {
  auto hid_connection = CreateHidConnection(/*with_connection_client=*/false);
  EXPECT_CALL(mock_connection(), PlatformGetFeatureReport)
      .WillOnce([](uint8_t report_id, HidConnection::ReadCallback callback) {
        std::move(callback).Run(/*success=*/true,
                                base::MakeRefCounted<base::RefCountedBytes>(
                                    std::vector<uint8_t>{report_id}),
                                /*size=*/1u);
      });
  GetFeatureFuture get_feature_future;
  hid_connection->GetFeatureReport(kTestReportId,
                                   get_feature_future.GetCallback());
  EXPECT_TRUE(get_feature_future.Get<0>());
  ASSERT_TRUE(get_feature_future.Get<1>().has_value());
  EXPECT_EQ(get_feature_future.Get<1>().value().size(), 1u);
}

TEST_F(HidConnectionImplTest, WriteZeroLengthFeatureReport) {
  auto hid_connection = CreateHidConnection(/*with_connection_client=*/false);
  scoped_refptr<base::RefCountedBytes> feature_buffer;
  EXPECT_CALL(mock_connection(), PlatformSendFeatureReport)
      .WillOnce([&feature_buffer](scoped_refptr<base::RefCountedBytes> buffer,
                                  HidConnectionImpl::WriteCallback callback) {
        feature_buffer = buffer;
        std::move(callback).Run(/*success=*/true);
      });
  WriteFuture write_future;
  hid_connection->SendFeatureReport(kTestReportId, /*buffer=*/{},
                                    write_future.GetCallback());
  EXPECT_TRUE(write_future.Get());
  ASSERT_TRUE(feature_buffer);
  EXPECT_THAT(feature_buffer->as_vector(), ElementsAre(kTestReportId));
}

}  // namespace device
