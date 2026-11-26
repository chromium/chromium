// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/mock_producer.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/shared_memory_virtual_device_mojo_adapter.h"
#include "services/video_capture/video_capture_service_impl.h"
#include "services/video_capture/video_source_provider_impl.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace video_capture {

namespace {

constexpr char kTestDeviceId[] = "/test/device";
constexpr char kTestDeviceName[] = "Test Device";
constexpr gfx::Size kTestFrameSize = {640 /* width */, 480 /* height */};
constexpr media::VideoPixelFormat kTestPixelFormat =
    media::VideoPixelFormat::PIXEL_FORMAT_I420;

}  // anonymous namespace

class VirtualDeviceTest : public ::testing::Test {
 public:
  VirtualDeviceTest() = default;
  ~VirtualDeviceTest() override = default;

  void SetUp() override {
    device_info_.descriptor.device_id = kTestDeviceId;
    device_info_.descriptor.set_display_name(kTestDeviceName);
    mojo::Remote<mojom::Producer> producer;
    producer_ =
        std::make_unique<MockProducer>(producer.BindNewPipeAndPassReceiver());
    device_adapter_ = std::make_unique<SharedMemoryVirtualDeviceMojoAdapter>(
        std::move(producer));
  }

  void OnFrameBufferReceived(bool valid_buffer_expected, int32_t buffer_id) {
    if (!valid_buffer_expected) {
      EXPECT_EQ(-1, buffer_id);
      return;
    }

    EXPECT_NE(-1, buffer_id);
    received_buffer_ids_.push_back(buffer_id);
  }

  void VerifyAndGetMaxFrameBuffers() {
    base::RunLoop wait_loop;
    EXPECT_CALL(*producer_, DoOnNewBuffer(_, _, _))
        .Times(SharedMemoryVirtualDeviceMojoAdapter::
                   max_buffer_pool_buffer_count())
        .WillRepeatedly([](int32_t buffer_id,
                           media::mojom::VideoBufferHandlePtr* handle,
                           mojom::Producer::OnNewBufferCallback& callback) {
          std::move(callback).Run();
        });
    // Should receive valid buffer for up to the maximum buffer count.
    for (int i = 0;
         i <
         SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count();
         i++) {
      device_adapter_->RequestFrameBuffer(
          kTestFrameSize, kTestPixelFormat, nullptr,
          base::BindOnce(&VirtualDeviceTest::OnFrameBufferReceived,
                         base::Unretained(this),
                         true /* valid_buffer_expected */));
    }

    // No more buffer available. Invalid buffer id should be returned.
    device_adapter_->RequestFrameBuffer(
        kTestFrameSize, kTestPixelFormat, nullptr,
        base::BindOnce(&VirtualDeviceTest::OnFrameBufferReceived,
                       base::Unretained(this),
                       false /* valid_buffer_expected */));

    wait_loop.RunUntilIdle();
    Mock::VerifyAndClearExpectations(producer_.get());
    EXPECT_EQ(
        SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count(),
        static_cast<int>(received_buffer_ids_.size()));
  }

 protected:
  std::unique_ptr<SharedMemoryVirtualDeviceMojoAdapter> device_adapter_;
  // ID of buffers received and owned by the producer.
  std::vector<int> received_buffer_ids_;
  std::unique_ptr<MockProducer> producer_;

  media::mojom::VideoFrameInfoPtr CreateValidVideoFrameInfo() {
    auto info = media::mojom::VideoFrameInfo::New();
    info->pixel_format = kTestPixelFormat;
    info->coded_size = kTestFrameSize;
    info->visible_rect = gfx::Rect(kTestFrameSize);
    info->natural_size = kTestFrameSize;
    return info;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  media::VideoCaptureDeviceInfo device_info_;
};

TEST_F(VirtualDeviceTest, OnFrameReadyInBufferWithoutReceiver) {
  // Obtain maximum number of buffers.
  VerifyAndGetMaxFrameBuffers();

  base::RunLoop wait_loop;

  // Release one buffer back to the pool, no consumer hold since there is no
  // receiver.
  device_adapter_->OnFrameReadyInBuffer(received_buffer_ids_.at(0),
                                        CreateValidVideoFrameInfo());

  // Verify there is a buffer available now, without creating a new
  // buffer.
  EXPECT_CALL(*producer_, DoOnNewBuffer(_, _, _)).Times(0);
  device_adapter_->RequestFrameBuffer(
      kTestFrameSize, kTestPixelFormat, nullptr,
      base::BindOnce(&VirtualDeviceTest::OnFrameBufferReceived,
                     base::Unretained(this), true /* valid_buffer_expected */));

  wait_loop.RunUntilIdle();
}

TEST_F(VirtualDeviceTest, OnFrameReadyInBufferWithReceiver) {
  // Obtain maximum number of buffers for the producer.
  VerifyAndGetMaxFrameBuffers();

  // Release all buffers back to consumer, then back to the pool
  // after |Receiver::OnFrameReadyInBuffer| is invoked.
  base::RunLoop wait_loop;
  mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote;
  MockVideoFrameHandler video_frame_handler(
      handler_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(video_frame_handler, OnStarted());
  EXPECT_CALL(video_frame_handler, DoOnNewBuffer(_, _))
      .Times(
          SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count());
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _))
      .Times(
          SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count());
  device_adapter_->Start(media::VideoCaptureParams(),
                         std::move(handler_remote));
  for (auto buffer_id : received_buffer_ids_) {
    device_adapter_->OnFrameReadyInBuffer(buffer_id,
                                          CreateValidVideoFrameInfo());
  }
  wait_loop.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&video_frame_handler);

  // Verify that requesting a buffer doesn't create a new one, will reuse
  // the available buffer in the pool.
  base::RunLoop wait_loop2;
  EXPECT_CALL(*producer_, DoOnNewBuffer(_, _, _)).Times(0);
  base::MockCallback<
      mojom::SharedMemoryVirtualDevice::RequestFrameBufferCallback>
      request_frame_buffer_callback;
  EXPECT_CALL(request_frame_buffer_callback, Run)
      .Times(1)
      .WillOnce([this](int32_t buffer_id) {
        // Verify that the returned |buffer_id| is a known buffer ID.
        EXPECT_TRUE(std::ranges::contains(received_buffer_ids_, buffer_id));
      });
  device_adapter_->RequestFrameBuffer(kTestFrameSize, kTestPixelFormat, nullptr,
                                      request_frame_buffer_callback.Get());
  wait_loop2.RunUntilIdle();

  // Verify that when stopping the device, the receiver receives calls to
  // OnBufferRetired() followed by a single call to OnStopped().
  base::RunLoop wait_for_stopped_loop;
  {
    testing::InSequence s;
    EXPECT_CALL(video_frame_handler, DoOnBufferRetired)
        .Times(SharedMemoryVirtualDeviceMojoAdapter::
                   max_buffer_pool_buffer_count());
    EXPECT_CALL(video_frame_handler, OnStopped())
        .WillOnce([&wait_for_stopped_loop]() { wait_for_stopped_loop.Quit(); });
  }
  device_adapter_->Stop();
  wait_for_stopped_loop.Run();
}

class MockDeviceFactory : public DeviceFactory {
 public:
  MockDeviceFactory() = default;
  ~MockDeviceFactory() override = default;

  MOCK_METHOD(void,
              GetDeviceInfos,
              (media::mojom::VideoCaptureDeviceEnumerationMode enumeration_mode,
               GetDeviceInfosCallback callback),
              (override));
  MOCK_METHOD(void,
              CreateDevice,
              (const std::string& device_id, CreateDeviceCallback callback),
              (override));
  MOCK_METHOD(void, StopDevice, (const std::string device_id), (override));
  MOCK_METHOD(void,
              AddSharedMemoryVirtualDevice,
              (const media::VideoCaptureDeviceInfo& device_info,
               mojo::PendingRemote<mojom::Producer> producer,
               mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
                   virtual_device_receiver,
               AddSharedMemoryVirtualDeviceCallback callback),
              (override));
  MOCK_METHOD(void,
              AddTextureVirtualDevice,
              (const media::VideoCaptureDeviceInfo& device_info,
               mojo::PendingReceiver<mojom::TextureVirtualDevice>
                   virtual_device_receiver),
              (override));
  MOCK_METHOD(void,
              AddGpuMemoryBufferVirtualDevice,
              (const media::VideoCaptureDeviceInfo& device_info,
               mojo::PendingReceiver<mojom::GpuMemoryBufferVirtualDevice>
                   virtual_device_receiver),
              (override));
  MOCK_METHOD(void,
              RegisterVirtualDevicesChangedObserver,
              (mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
               bool raise_event_if_virtual_devices_already_present),
              (override));
#if BUILDFLAG(IS_WIN)
  MOCK_METHOD(void, OnGpuInfoUpdate, (const CHROME_LUID& luid), (override));
#endif
};

class VirtualDeviceEnabledDeviceFactoryTest : public ::testing::Test {
 protected:
  VirtualDeviceEnabledDeviceFactoryTest() {
    auto mock_factory = std::make_unique<MockDeviceFactory>();
    mock_factory_ = mock_factory.get();
    factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
        std::move(mock_factory));
  }

  ~VirtualDeviceEnabledDeviceFactoryTest() override = default;

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<VirtualDeviceEnabledDeviceFactory> factory_;
  raw_ptr<MockDeviceFactory> mock_factory_ = nullptr;
};

class VirtualDeviceEnabledDeviceFactoryWithoutWrappedFactoryTest
    : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<VirtualDeviceEnabledDeviceFactory> factory_ =
      std::make_unique<VirtualDeviceEnabledDeviceFactory>(nullptr);
};

TEST_F(VirtualDeviceEnabledDeviceFactoryWithoutWrappedFactoryTest,
       ReturnsNoDevicesWithoutRegisteredVirtualDevices) {
  base::MockCallback<DeviceFactory::GetDeviceInfosCallback> infos_callback;
  EXPECT_CALL(infos_callback, Run)
      .WillOnce([](const std::vector<media::VideoCaptureDeviceInfo>& infos) {
        EXPECT_TRUE(infos.empty());
      });
  factory_->GetDeviceInfos(
      media::mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
      infos_callback.Get());

  base::MockCallback<DeviceFactory::CreateDeviceCallback> create_callback;
  EXPECT_CALL(create_callback, Run)
      .WillOnce([](DeviceFactory::DeviceInfo info) {
        EXPECT_EQ(
            info.result_code,
            media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
        EXPECT_EQ(info.device, nullptr);
      });
  factory_->CreateDevice("virtual-chromium-missing-device",
                         create_callback.Get());
}

TEST_F(VirtualDeviceEnabledDeviceFactoryWithoutWrappedFactoryTest,
       RegistersAndCreatesVirtualDevice) {
  const std::string device_id = "virtual-chromium-test-device";
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;

  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;
  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(), base::DoNothing());
  virtual_device.FlushForTesting();
  ASSERT_TRUE(virtual_device.is_connected());

  base::MockCallback<DeviceFactory::GetDeviceInfosCallback> infos_callback;
  EXPECT_CALL(infos_callback, Run)
      .WillOnce([&](const std::vector<media::VideoCaptureDeviceInfo>& infos) {
        ASSERT_EQ(infos.size(), 1u);
        EXPECT_EQ(infos.front().descriptor.device_id, device_id);
      });
  factory_->GetDeviceInfos(
      media::mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
      infos_callback.Get());

  base::MockCallback<DeviceFactory::CreateDeviceCallback> create_callback;
  EXPECT_CALL(create_callback, Run)
      .WillOnce([](DeviceFactory::DeviceInfo info) {
        EXPECT_EQ(info.result_code, media::VideoCaptureError::kNone);
        EXPECT_NE(info.device, nullptr);
      });
  factory_->CreateDevice(device_id, create_callback.Get());
}

#if BUILDFLAG(IS_WIN)
TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       SourceProviderForwardsDevicesOnlyEnumerationMode) {
  VideoSourceProviderImpl source_provider(factory_.get(),
                                          base::BindRepeating([] {}));
  EXPECT_CALL(
      *mock_factory_,
      GetDeviceInfos(
          media::mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly, _))
      .WillOnce([](media::mojom::VideoCaptureDeviceEnumerationMode,
                   DeviceFactory::GetDeviceInfosCallback callback) {
        std::move(callback).Run({});
      });
  base::MockCallback<mojom::VideoSourceProvider::GetSourceInfosCallback>
      result_callback;
  EXPECT_CALL(
      result_callback,
      Run(mojom::VideoSourceProvider::GetSourceInfosResult::kSuccess, _));

  source_provider.GetSourceInfos(
      media::mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly,
      result_callback.Get());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       SharedMemoryRegistrationCallbackWaitsForInsertion) {
  using Result = mojom::VideoSourceProvider::AddVirtualDeviceResult;
  base::test::TestFuture<const std::vector<media::VideoCaptureDeviceInfo>&>
      infos;
  DeviceFactory::GetDeviceInfosCallback pending_device_infos;
  EXPECT_CALL(*mock_factory_, GetDeviceInfos(_, _))
      .WillOnce([&](media::mojom::VideoCaptureDeviceEnumerationMode,
                    DeviceFactory::GetDeviceInfosCallback callback) {
        pending_device_infos = std::move(callback);
      })
      .WillOnce([](media::mojom::VideoCaptureDeviceEnumerationMode,
                   DeviceFactory::GetDeviceInfosCallback callback) {
        std::move(callback).Run({});
      });

  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = "virtual-chromium-registration-test";
  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;
  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting([&](Result result) {
        ASSERT_EQ(Result::kSuccess, result);
        // Query from the callback, not after AddSharedMemoryVirtualDevice
        // returns, to catch an acknowledgement sent before insertion.
        factory_->GetDeviceInfos(
            media::mojom::VideoCaptureDeviceEnumerationMode::
                kDevicesWithCapabilities,
            infos.GetCallback());
      }));

  ASSERT_FALSE(pending_device_infos.is_null());
  EXPECT_FALSE(infos.IsReady());
  std::move(pending_device_infos).Run({});

  ASSERT_TRUE(infos.IsReady());
  ASSERT_EQ(1u, infos.Get().size());
  EXPECT_EQ(device_info.descriptor.device_id,
            infos.Get().front().descriptor.device_id);
}

TEST_F(VirtualDeviceEnabledDeviceFactoryWithoutWrappedFactoryTest,
       SharedMemoryRegistrationCallbackRunsAfterInsertion) {
  using Result = mojom::VideoSourceProvider::AddVirtualDeviceResult;
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = "virtual-chromium-registration-test";
  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;
  base::test::TestFuture<const std::vector<media::VideoCaptureDeviceInfo>&>
      infos;
  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting([&](Result result) {
        ASSERT_EQ(Result::kSuccess, result);
        factory_->GetDeviceInfos(
            media::mojom::VideoCaptureDeviceEnumerationMode::
                kDevicesWithCapabilities,
            infos.GetCallback());
      }));

  ASSERT_TRUE(infos.IsReady());
  ASSERT_EQ(1u, infos.Get().size());
  EXPECT_EQ(device_info.descriptor.device_id,
            infos.Get().front().descriptor.device_id);
}

TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       SharedMemoryRegistrationCallbackReportsInvalidDevice) {
  using Result = mojom::VideoSourceProvider::AddVirtualDeviceResult;
  EXPECT_CALL(*mock_factory_, GetDeviceInfos(_, _)).Times(0);
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = "missing-virtual-device-prefix";
  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;
  base::test::TestFuture<Result> registered;
  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(), registered.GetCallback());

  ASSERT_TRUE(registered.IsReady());
  EXPECT_EQ(Result::kErrorInvalidDeviceInfo, registered.Get());
}

TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       SharedMemoryRegistrationCallbackReportsHardwareIdCollision) {
  using Result = mojom::VideoSourceProvider::AddVirtualDeviceResult;
  base::test::TestFuture<Result> registered;
  DeviceFactory::GetDeviceInfosCallback pending_device_infos;
  EXPECT_CALL(*mock_factory_, GetDeviceInfos(_, _))
      .WillOnce([&](media::mojom::VideoCaptureDeviceEnumerationMode,
                    DeviceFactory::GetDeviceInfosCallback callback) {
        pending_device_infos = std::move(callback);
      })
      .WillOnce([](media::mojom::VideoCaptureDeviceEnumerationMode,
                   DeviceFactory::GetDeviceInfosCallback callback) {
        std::move(callback).Run({});
      });

  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = "virtual-chromium-registration-test";
  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;
  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(), registered.GetCallback());

  ASSERT_FALSE(pending_device_infos.is_null());
  EXPECT_FALSE(registered.IsReady());
  std::move(pending_device_infos).Run({device_info});
  ASSERT_TRUE(registered.IsReady());
  EXPECT_EQ(Result::kErrorHardwareDeviceIdCollision, registered.Get());

  // With the wrapped factory now reporting no devices, a nonempty result would
  // reveal an incorrectly inserted virtual camera.
  base::test::TestFuture<const std::vector<media::VideoCaptureDeviceInfo>&>
      infos;
  factory_->GetDeviceInfos(
      media::mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
      infos.GetCallback());
  ASSERT_TRUE(infos.IsReady());
  EXPECT_TRUE(infos.Get().empty());
}

TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       DiscardedEnumerationReleasesRegistrationCallback) {
  using Result = mojom::VideoSourceProvider::AddVirtualDeviceResult;
  DeviceFactory::GetDeviceInfosCallback pending_infos;
  EXPECT_CALL(*mock_factory_, GetDeviceInfos(_, _))
      .WillOnce([&](media::mojom::VideoCaptureDeviceEnumerationMode,
                    DeviceFactory::GetDeviceInfosCallback callback) {
        pending_infos = std::move(callback);
      });

  base::test::TestFuture<Result> replied;
  base::test::TestFuture<void> discarded;
  media::VideoCaptureDeviceInfo info;
  info.descriptor.device_id = "virtual-chromium-discarded-registration";
  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> device;
  factory_->AddSharedMemoryVirtualDevice(
      info, std::move(producer), device.BindNewPipeAndPassReceiver(),
      mojo::WrapCallbackWithDropHandler(replied.GetCallback(),
                                        discarded.GetCallback()));

  EXPECT_FALSE(pending_infos.is_null());
  EXPECT_FALSE(replied.IsReady());
  EXPECT_FALSE(discarded.IsReady());
  // A plain callback lets us observe its destruction without triggering the
  // Mojo missing-response DCHECK. No response should be synthesized here.
  pending_infos.Reset();
  EXPECT_TRUE(discarded.IsReady());
  EXPECT_FALSE(replied.IsReady());
  mock_factory_ = nullptr;
  factory_.reset();
  EXPECT_FALSE(replied.IsReady());
}

TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       CompletesRegistrationBeforeLateEnumeration) {
  using Result = mojom::VideoSourceProvider::AddVirtualDeviceResult;
  std::vector<DeviceFactory::GetDeviceInfosCallback> pending_infos;
  EXPECT_CALL(*mock_factory_, GetDeviceInfos(_, _))
      .Times(2)
      .WillRepeatedly([&](media::mojom::VideoCaptureDeviceEnumerationMode,
                          DeviceFactory::GetDeviceInfosCallback callback) {
        pending_infos.push_back(std::move(callback));
      });

  media::VideoCaptureDeviceInfo info;
  info.descriptor.device_id = "virtual-chromium-late-enumeration";
  std::array<base::test::TestFuture<Result>, 2> completed;
  std::array<mojo::Remote<mojom::SharedMemoryVirtualDevice>, 2> devices;
  std::vector<std::unique_ptr<MockProducer>> producers;
  for (size_t i = 0; i < completed.size(); ++i) {
    mojo::PendingRemote<mojom::Producer> producer;
    producers.push_back(std::make_unique<MockProducer>(
        producer.InitWithNewPipeAndPassReceiver()));
    factory_->AddSharedMemoryVirtualDevice(
        info, std::move(producer), devices[i].BindNewPipeAndPassReceiver(),
        completed[i].GetCallback());
    EXPECT_FALSE(completed[i].IsReady());
  }
  ASSERT_EQ(2u, pending_infos.size());

  mock_factory_ = nullptr;
  factory_.reset();
  for (auto& reply : completed) {
    ASSERT_TRUE(reply.IsReady());
    EXPECT_EQ(Result::kErrorDroppedRequest, reply.Get());
  }

  // Both invoking and discarding a continuation retained outside the factory
  // must be harmless after its registration has already been completed.
  std::move(pending_infos[0]).Run({});
  pending_infos[1].Reset();
  for (auto& reply : completed) {
    EXPECT_EQ(Result::kErrorDroppedRequest, reply.Get());
  }
}

// Use the actual service destructor, with enumeration held in its owned
// factory, to distinguish actual responses from callbacks the client drops.
class VideoCaptureServiceShutdownTest : public testing::Test {
 protected:
  class FactoryWithPendingEnumeration : public MockDeviceFactory {
   public:
    std::vector<GetDeviceInfosCallback> pending_infos;
  };

  void SetUp() override {
    auto factory = std::make_unique<FactoryWithPendingEnumeration>();
    pending_factory_ = factory.get();
    EXPECT_CALL(*pending_factory_, GetDeviceInfos(_, _))
        .WillRepeatedly([this](media::mojom::VideoCaptureDeviceEnumerationMode,
                               DeviceFactory::GetDeviceInfosCallback callback) {
          pending_factory_->pending_infos.push_back(std::move(callback));
        });
    service_ = std::make_unique<VideoCaptureServiceImpl>(
        service_remote_.BindNewPipeAndPassReceiver(),
        task_environment_.GetMainThreadTaskRunner(),
        /*create_system_monitor=*/false);
    // Inject before the provider is created, avoiding the platform backend.
    service_->device_factory_ =
        std::make_unique<VirtualDeviceEnabledDeviceFactory>(std::move(factory));
    service_->ConnectToVideoSourceProvider(
        provider_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override { DestroyService(); }

  void DestroyService() {
    pending_factory_ = nullptr;
    service_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::VideoCaptureService> service_remote_;
  mojo::Remote<mojom::VideoSourceProvider> provider_;
  std::unique_ptr<VideoCaptureServiceImpl> service_;
  raw_ptr<FactoryWithPendingEnumeration> pending_factory_ = nullptr;
};

TEST_F(VideoCaptureServiceShutdownTest,
       CompletesPendingRequestsDuringShutdown) {
  using Result = mojom::VideoSourceProvider::AddVirtualDeviceResult;
  using InfosResult = mojom::VideoSourceProvider::GetSourceInfosResult;
  media::VideoCaptureDeviceInfo info;
  info.descriptor.device_id = "virtual-chromium-shutdown-registration";
  std::array<base::test::TestFuture<bool, Result>, 3> completed;
  std::array<mojo::Remote<mojom::SharedMemoryVirtualDevice>, 3> devices;
  std::vector<std::unique_ptr<MockProducer>> producers;
  base::test::TestFuture<void> disconnected;
  provider_.set_disconnect_handler(disconnected.GetCallback());
  for (size_t i = 0; i < completed.size(); ++i) {
    mojo::PendingRemote<mojom::Producer> producer;
    producers.push_back(std::make_unique<MockProducer>(
        producer.InitWithNewPipeAndPassReceiver()));
    // Keep the ID identical: ownership belongs to each request, not each ID.
    provider_->AddSharedMemoryVirtualDevice(
        info, std::move(producer), devices[i].BindNewPipeAndPassReceiver(),
        mojo::WrapCallbackWithDropHandler(
            base::BindOnce(completed[i].GetCallback(), true),
            base::BindOnce(completed[i].GetCallback(), false,
                           Result::kErrorDroppedRequest)));
  }
  base::test::TestFuture<bool, InfosResult,
                         const std::vector<media::VideoCaptureDeviceInfo>&>
      infos;
  provider_->GetSourceInfos(
      media::mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(infos.GetCallback(), true),
          base::BindOnce(infos.GetCallback(), false,
                         InfosResult::kErrorDroppedRequest,
                         std::vector<media::VideoCaptureDeviceInfo>())));
  provider_.FlushForTesting();
  ASSERT_EQ(4u, pending_factory_->pending_infos.size());

  // Complete one registration normally. Shutdown must not complete it again.
  std::move(pending_factory_->pending_infos[0]).Run({});
  ASSERT_TRUE(completed[0].Wait());
  ASSERT_TRUE(completed[0].Get<0>());
  EXPECT_EQ(Result::kSuccess, completed[0].Get<1>());
  EXPECT_FALSE(completed[1].IsReady());
  EXPECT_FALSE(completed[2].IsReady());
  EXPECT_FALSE(infos.IsReady());
  ASSERT_FALSE(disconnected.IsReady());

  // With the factory destroyed before the provider, both registration and
  // ordinary enumeration must receive actual failure responses.
  DestroyService();
  for (size_t i = 1; i < completed.size(); ++i) {
    ASSERT_TRUE(completed[i].Wait());
    ASSERT_TRUE(completed[i].Get<0>()) << "Registration callback was dropped";
    EXPECT_EQ(Result::kErrorDroppedRequest, completed[i].Get<1>());
  }
  ASSERT_TRUE(infos.Wait());
  ASSERT_TRUE(infos.Get<0>()) << "GetSourceInfos callback was dropped";
  EXPECT_EQ(InfosResult::kErrorDroppedRequest, infos.Get<1>());
  EXPECT_TRUE(infos.Get<2>().empty());
  ASSERT_TRUE(disconnected.Wait());
  EXPECT_EQ(Result::kSuccess, completed[0].Get<1>());
  EXPECT_FALSE(provider_.is_connected());
}

// Tests that when an underlying device creation succeeds, any old virtual
// device connection with the same device ID is severed.
TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       CreateDeviceSucceedsOldVirtualDeviceConnectionSevered) {
  const std::string device_id = "virtual-chromium-test-device";

  // Register the virtual device first by having GetDeviceInfos return empty
  EXPECT_CALL(*mock_factory_, GetDeviceInfos(_, _))
      .WillOnce([](media::mojom::VideoCaptureDeviceEnumerationMode,
                   DeviceFactory::GetDeviceInfosCallback callback) {
        std::move(callback).Run(std::vector<media::VideoCaptureDeviceInfo>());
      });

  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;
  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;

  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(), base::DoNothing());
  virtual_device.FlushForTesting();

  // Call CreateDevice, mock the underlying factory to succeed
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_factory_, CreateDevice(device_id, _))
      .WillOnce([](const std::string& device_id,
                   DeviceFactory::CreateDeviceCallback callback) {
        DeviceFactory::DeviceInfo info{nullptr,
                                       media::VideoCaptureError::kNone};
        std::move(callback).Run(std::move(info));
      });

  base::MockCallback<DeviceFactory::CreateDeviceCallback> create_callback;
  EXPECT_CALL(create_callback, Run)
      .WillOnce([&run_loop](DeviceFactory::DeviceInfo info) {
        EXPECT_EQ(info.result_code, media::VideoCaptureError::kNone);
        run_loop.Quit();
      });

  factory_->CreateDevice(device_id, create_callback.Get());
  run_loop.Run();

  // Verify that the virtual device connection is severed
  base::RunLoop disconnect_loop;
  virtual_device.set_disconnect_handler(disconnect_loop.QuitClosure());
  disconnect_loop.Run();

  EXPECT_FALSE(virtual_device.is_connected());
}

// Tests that when an underlying device creation fails with
// kVideoCaptureSystemDeviceIdNotFound, the factory falls back to the registered
// virtual device and does NOT sever its connection.
TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       CreateDeviceFailsOldVirtualDeviceConnectionNotSevered) {
  const std::string device_id = "virtual-chromium-test-device";

  // Register the virtual device first
  EXPECT_CALL(*mock_factory_, GetDeviceInfos(_, _))
      .WillOnce([](media::mojom::VideoCaptureDeviceEnumerationMode,
                   DeviceFactory::GetDeviceInfosCallback callback) {
        std::move(callback).Run(std::vector<media::VideoCaptureDeviceInfo>());
      });

  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;
  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;

  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(), base::DoNothing());
  virtual_device.FlushForTesting();

  // Call CreateDevice, mock the underlying factory to fail with
  // kVideoCaptureSystemDeviceIdNotFound
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_factory_, CreateDevice(device_id, _))
      .WillOnce([](const std::string& device_id,
                   DeviceFactory::CreateDeviceCallback callback) {
        DeviceFactory::DeviceInfo info{
            nullptr,
            media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound};
        std::move(callback).Run(std::move(info));
      });

  base::MockCallback<DeviceFactory::CreateDeviceCallback> create_callback;
  EXPECT_CALL(create_callback, Run)
      .WillOnce([&run_loop](DeviceFactory::DeviceInfo info) {
        // Falls back to the virtual device, so it should succeed!
        EXPECT_EQ(info.result_code, media::VideoCaptureError::kNone);
        EXPECT_NE(info.device, nullptr);
        run_loop.Quit();
      });

  factory_->CreateDevice(device_id, create_callback.Get());
  run_loop.Run();

  // Verify that the virtual device is NOT severed
  virtual_device.FlushForTesting();
  EXPECT_TRUE(virtual_device.is_connected());
}

// Tests that when an underlying device creation fails with
// kVideoCaptureSystemDeviceIdNotFound, and no virtual device is registered with
// that ID, the factory returns the original failure.
TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       CreateDeviceFailsNoVirtualDeviceFallback) {
  const std::string device_id = "virtual-chromium-test-device";

  // Call CreateDevice without registering any virtual device
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_factory_, CreateDevice(device_id, _))
      .WillOnce([](const std::string& device_id,
                   DeviceFactory::CreateDeviceCallback callback) {
        DeviceFactory::DeviceInfo info{
            nullptr,
            media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound};
        std::move(callback).Run(std::move(info));
      });

  base::MockCallback<DeviceFactory::CreateDeviceCallback> create_callback;
  EXPECT_CALL(create_callback, Run)
      .WillOnce([&run_loop](DeviceFactory::DeviceInfo info) {
        // Returns the original failure since there is no fallback
        EXPECT_EQ(
            info.result_code,
            media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
        EXPECT_EQ(info.device, nullptr);
        run_loop.Quit();
      });

  factory_->CreateDevice(device_id, create_callback.Get());
  run_loop.Run();
}

// Tests that when an underlying device creation fails with an error other than
// not found, any old virtual device connection is severed and the factory
// returns the failure (no fallback).
TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       CreateDeviceFailsWithOtherErrorOldVirtualDeviceSevered) {
  const std::string device_id = "virtual-chromium-test-device";

  // Register the virtual device first
  EXPECT_CALL(*mock_factory_, GetDeviceInfos(_, _))
      .WillOnce([](media::mojom::VideoCaptureDeviceEnumerationMode,
                   DeviceFactory::GetDeviceInfosCallback callback) {
        std::move(callback).Run(std::vector<media::VideoCaptureDeviceInfo>());
      });

  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = device_id;
  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;

  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(), base::DoNothing());
  virtual_device.FlushForTesting();
  ASSERT_TRUE(virtual_device.is_connected());

  // Call CreateDevice, mock underlying factory to fail with a different error
  // (e.g. SecondCreateDenied)
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_factory_, CreateDevice(device_id, _))
      .WillOnce([](const std::string& device_id,
                   DeviceFactory::CreateDeviceCallback callback) {
        DeviceFactory::DeviceInfo info{
            nullptr, media::VideoCaptureError::
                         kVideoCaptureDeviceFactorySecondCreateDenied};
        std::move(callback).Run(std::move(info));
      });

  base::MockCallback<DeviceFactory::CreateDeviceCallback> create_callback;
  EXPECT_CALL(create_callback, Run)
      .WillOnce([&run_loop](DeviceFactory::DeviceInfo info) {
        // Returns the failure code, no fallback should occur!
        EXPECT_EQ(info.result_code,
                  media::VideoCaptureError::
                      kVideoCaptureDeviceFactorySecondCreateDenied);
        EXPECT_EQ(info.device, nullptr);
        run_loop.Quit();
      });

  factory_->CreateDevice(device_id, create_callback.Get());
  run_loop.Run();

  // Verify that the virtual device connection is severed
  base::RunLoop disconnect_loop;
  virtual_device.set_disconnect_handler(disconnect_loop.QuitClosure());
  disconnect_loop.Run();

  EXPECT_FALSE(virtual_device.is_connected());
}

TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       RejectVirtualDeviceWithInvalidIdPrefix) {
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = "invalid-id";
  device_info.descriptor.set_display_name("Test Device");

  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;

  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(), base::DoNothing());

  base::RunLoop disconnect_loop;
  virtual_device.set_disconnect_handler(disconnect_loop.QuitClosure());
  disconnect_loop.Run();

  EXPECT_FALSE(virtual_device.is_connected());
}

TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       RejectVirtualDeviceWithInvalidModelId) {
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = "chromium-virtual";
  device_info.descriptor.model_id = "0123:4567";

  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;

  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(), base::DoNothing());

  base::RunLoop disconnect_loop;
  virtual_device.set_disconnect_handler(disconnect_loop.QuitClosure());
  disconnect_loop.Run();

  EXPECT_FALSE(virtual_device.is_connected());
}

TEST_F(VirtualDeviceEnabledDeviceFactoryTest,
       RejectVirtualDeviceWithForbiddenDisplayNamePrefix) {
  media::VideoCaptureDeviceInfo device_info;
  device_info.descriptor.device_id = "virtual-chromium-device";
  device_info.descriptor.set_display_name("Real Camera");

  mojo::PendingRemote<mojom::Producer> producer;
  MockProducer mock_producer(producer.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::SharedMemoryVirtualDevice> virtual_device;

  factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      virtual_device.BindNewPipeAndPassReceiver(), base::DoNothing());

  base::RunLoop disconnect_loop;
  virtual_device.set_disconnect_handler(disconnect_loop.QuitClosure());
  disconnect_loop.Run();

  EXPECT_FALSE(virtual_device.is_connected());
}

}  // namespace video_capture
