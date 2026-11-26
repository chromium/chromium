// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/mock_capture_device_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom-test-utils.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using AddResult = MockCaptureDeviceController::AddMockCameraResult;
using RegistrationResult =
    video_capture::mojom::VideoSourceProvider::AddVirtualDeviceResult;

MockCameraConfig MakeConfig(const char* id = "camera-1") {
  return MockCameraConfig{
      .device_id = base::StrCat({media::kVirtualDeviceIdPrefix, id}),
  };
}

// Holds registration replies so tests can distinguish a pending camera from a
// successfully registered one. No capture service or frame buffers are used.
class RecordingVideoSourceProvider
    : public video_capture::mojom::VideoSourceProviderInterceptorForTesting {
 public:
  struct Registration {
    media::VideoCaptureDeviceInfo info;
    base::test::TestFuture<void> disconnected;
    mojo::Remote<video_capture::mojom::Producer> producer;
    mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
        device_receiver;
    AddSharedMemoryVirtualDeviceCallback callback;
  };

  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingRemote<video_capture::mojom::Producer> producer,
      mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
          device_receiver,
      AddSharedMemoryVirtualDeviceCallback callback) override {
    auto registration = std::make_unique<Registration>();
    registration->info = device_info;
    registration->producer.Bind(std::move(producer));
    registration->device_receiver = std::move(device_receiver);
    registration->producer.set_disconnect_handler(
        registration->disconnected.GetCallback());
    registration->callback = std::move(callback);
    registrations_.push_back(std::move(registration));
  }

  video_capture::mojom::VideoSourceProvider* GetForwardingInterface() override {
    NOTREACHED();
  }

  const auto& registrations() const { return registrations_; }

  void Reply(size_t index, RegistrationResult result) {
    std::move(registrations_.at(index)->callback).Run(result);
  }

  void RejectPendingRegistrations() {
    for (const auto& registration : registrations_) {
      if (registration->callback) {
        std::move(registration->callback)
            .Run(RegistrationResult::kErrorDroppedRequest);
      }
    }
  }

 private:
  std::vector<std::unique_ptr<Registration>> registrations_;
};

class MockCaptureDeviceControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    controller_ = std::make_unique<MockCaptureDeviceController>(
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::BindRepeating(&MockCaptureDeviceControllerTest::BindProvider,
                            weak_factory_.GetWeakPtr()));
  }

  void TearDown() override {
    weak_factory_.InvalidateWeakPtrs();
    controller_.reset();
    // Explicitly finish replies retained by the test instead of dropping them.
    provider_.RejectPendingRegistrations();
    if (provider_receiver_.is_bound()) {
      ASSERT_TRUE(provider_disconnected_.Wait());
    }
    for (const auto& registration : provider_.registrations()) {
      EXPECT_TRUE(registration->disconnected.Wait());
    }
  }

  void BindProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
          receiver) {
    provider_receiver_.Bind(std::move(receiver));
    provider_receiver_.set_disconnect_handler(
        provider_disconnected_.GetCallback());
    provider_bound_.SetValue();
  }

  void FlushRegistrationRequests() {
    ASSERT_TRUE(provider_bound_.Wait());
    ASSERT_FALSE(provider_disconnected_.IsReady());
    // This round trip follows earlier registration requests on the same pipe.
    provider_receiver_.FlushForTesting();
    ASSERT_FALSE(provider_disconnected_.IsReady());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  RecordingVideoSourceProvider provider_;
  base::test::TestFuture<void> provider_bound_;
  base::test::TestFuture<void> provider_disconnected_;
  mojo::Receiver<video_capture::mojom::VideoSourceProvider> provider_receiver_{
      &provider_};
  std::unique_ptr<MockCaptureDeviceController> controller_;
  base::WeakPtrFactory<MockCaptureDeviceControllerTest> weak_factory_{this};
};

TEST_F(MockCaptureDeviceControllerTest,
       DuplicateAddDuringRegistrationIsRejected) {
  base::test::TestFuture<AddResult> first;
  base::test::TestFuture<AddResult> duplicate;
  controller_->AddMockCamera(MakeConfig(), first.GetCallback());
  controller_->AddMockCamera(MakeConfig(), duplicate.GetCallback());
  // Even a locally rejected request completes asynchronously.
  EXPECT_FALSE(duplicate.IsReady());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(1u, provider_.registrations().size());
  EXPECT_EQ(AddResult::kAlreadyExists, duplicate.Get());
  EXPECT_FALSE(first.IsReady());

  // A duplicate after dispatch is also rejected without completing the first.
  base::test::TestFuture<AddResult> after_dispatch;
  controller_->AddMockCamera(MakeConfig(), after_dispatch.GetCallback());
  EXPECT_EQ(AddResult::kAlreadyExists, after_dispatch.Get());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(1u, provider_.registrations().size());
  EXPECT_FALSE(first.IsReady());

  provider_.Reply(0, RegistrationResult::kSuccess);
  EXPECT_EQ(AddResult::kSuccess, first.Get());
  provider_.registrations()[0]->producer.FlushForTesting();
  EXPECT_TRUE(provider_.registrations()[0]->producer.is_connected());
}

TEST_F(MockCaptureDeviceControllerTest,
       DuplicateAddWithDifferentConfigPreservesCamera) {
  MockCameraConfig first_config = MakeConfig();
  first_config.label = "Original camera";
  base::test::TestFuture<AddResult> first;
  controller_->AddMockCamera(first_config, first.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(1u, provider_.registrations().size());
  provider_.Reply(0, RegistrationResult::kSuccess);
  ASSERT_EQ(AddResult::kSuccess, first.Get());
  const std::string original_name =
      provider_.registrations()[0]->info.descriptor.display_name();

  MockCameraConfig changed = first_config;
  changed.label = "Changed camera";
  changed.size = gfx::Size(320, 240);
  changed.frame_rate = 10.0;
  base::test::TestFuture<AddResult> duplicate;
  controller_->AddMockCamera(std::move(changed), duplicate.GetCallback());
  EXPECT_EQ(AddResult::kAlreadyExists, duplicate.Get());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(1u, provider_.registrations().size());
  EXPECT_EQ(original_name,
            provider_.registrations()[0]->info.descriptor.display_name());
  provider_.registrations()[0]->producer.FlushForTesting();
  EXPECT_TRUE(provider_.registrations()[0]->producer.is_connected());
}

TEST_F(MockCaptureDeviceControllerTest, DifferentIdsHaveIndependentProducers) {
  base::test::TestFuture<AddResult> first;
  base::test::TestFuture<AddResult> second;
  controller_->AddMockCamera(MakeConfig("camera-1"), first.GetCallback());
  controller_->AddMockCamera(MakeConfig("camera-2"), second.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(2u, provider_.registrations().size());

  provider_.Reply(0, RegistrationResult::kSuccess);
  provider_.Reply(1, RegistrationResult::kSuccess);
  ASSERT_EQ(AddResult::kSuccess, first.Get());
  ASSERT_EQ(AddResult::kSuccess, second.Get());

  controller_->RemoveMockCamera(MakeConfig("camera-1").device_id);
  ASSERT_TRUE(provider_.registrations()[0]->disconnected.Wait());
  EXPECT_FALSE(provider_.registrations()[0]->producer.is_connected());
  provider_.registrations()[1]->producer.FlushForTesting();
  EXPECT_TRUE(provider_.registrations()[1]->producer.is_connected());
}

TEST_F(MockCaptureDeviceControllerTest,
       RegistrationFailureReasonsArePreserved) {
  const struct {
    RegistrationResult service_result;
    AddResult expected_result;
  } cases[] = {
      {RegistrationResult::kErrorInvalidDeviceInfo,
       AddResult::kInvalidDeviceInfo},
      {RegistrationResult::kErrorHardwareDeviceIdCollision,
       AddResult::kHardwareDeviceIdCollision},
      {RegistrationResult::kErrorNotSupported, AddResult::kNotSupported},
      {RegistrationResult::kErrorDroppedRequest, AddResult::kAborted},
  };
  for (const auto& test_case : cases) {
    const size_t index = provider_.registrations().size();
    base::test::TestFuture<AddResult> result;
    controller_->AddMockCamera(MakeConfig(), result.GetCallback());
    ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
    ASSERT_EQ(index + 1, provider_.registrations().size());
    EXPECT_FALSE(result.IsReady());
    provider_.Reply(index, test_case.service_result);
    EXPECT_EQ(test_case.expected_result, result.Get());
    ASSERT_TRUE(provider_.registrations()[index]->disconnected.Wait());
  }
}

TEST_F(MockCaptureDeviceControllerTest, RegistrationFailureAllowsRetry) {
  base::test::TestFuture<AddResult> first;
  controller_->AddMockCamera(MakeConfig(), first.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(1u, provider_.registrations().size());
  EXPECT_FALSE(first.IsReady());
  provider_.Reply(0, RegistrationResult::kErrorInvalidDeviceInfo);
  EXPECT_EQ(AddResult::kInvalidDeviceInfo, first.Get());
  ASSERT_TRUE(provider_.registrations()[0]->disconnected.Wait());

  base::test::TestFuture<AddResult> retry;
  controller_->AddMockCamera(MakeConfig(), retry.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(2u, provider_.registrations().size());
  EXPECT_FALSE(retry.IsReady());
  provider_.Reply(1, RegistrationResult::kSuccess);
  EXPECT_EQ(AddResult::kSuccess, retry.Get());
}

TEST_F(MockCaptureDeviceControllerTest,
       ProviderDisconnectFailsPendingRequests) {
  base::test::TestFuture<AddResult> first;
  base::test::TestFuture<AddResult> second;
  controller_->AddMockCamera(MakeConfig("camera-1"), first.GetCallback());
  controller_->AddMockCamera(MakeConfig("camera-2"), second.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(2u, provider_.registrations().size());

  provider_receiver_.reset();
  EXPECT_EQ(AddResult::kAborted, first.Get());
  EXPECT_EQ(AddResult::kAborted, second.Get());
  ASSERT_TRUE(provider_.registrations()[0]->disconnected.Wait());
  ASSERT_TRUE(provider_.registrations()[1]->disconnected.Wait());
}

TEST_F(MockCaptureDeviceControllerTest,
       DestructionFailsPendingRequestsAndClosesProducers) {
  base::test::TestFuture<AddResult> first;
  base::test::TestFuture<AddResult> second;
  controller_->AddMockCamera(MakeConfig("camera-1"), first.GetCallback());
  controller_->AddMockCamera(MakeConfig("camera-2"), second.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(2u, provider_.registrations().size());

  controller_.reset();
  EXPECT_EQ(AddResult::kAborted, first.Get());
  EXPECT_EQ(AddResult::kAborted, second.Get());
  ASSERT_TRUE(provider_.registrations()[0]->disconnected.Wait());
  ASSERT_TRUE(provider_.registrations()[1]->disconnected.Wait());
  // Service replies racing with destruction must not access the controller.
  provider_.Reply(0, RegistrationResult::kSuccess);
  provider_.Reply(1, RegistrationResult::kSuccess);
}

TEST_F(MockCaptureDeviceControllerTest,
       LateReplyDoesNotCompleteNewRegistration) {
  base::test::TestFuture<AddResult> old_request;
  controller_->AddMockCamera(MakeConfig(), old_request.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(1u, provider_.registrations().size());
  controller_->RemoveMockCamera(MakeConfig().device_id);
  EXPECT_EQ(AddResult::kAborted, old_request.Take());
  ASSERT_TRUE(provider_.registrations()[0]->disconnected.Wait());
  EXPECT_FALSE(provider_.registrations()[0]->producer.is_connected());

  base::test::TestFuture<AddResult> new_request;
  controller_->AddMockCamera(MakeConfig(), new_request.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(2u, provider_.registrations().size());
  provider_.Reply(0, RegistrationResult::kSuccess);
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  EXPECT_FALSE(old_request.IsReady());
  EXPECT_FALSE(new_request.IsReady());
  provider_.Reply(1, RegistrationResult::kSuccess);
  EXPECT_EQ(AddResult::kSuccess, new_request.Get());
}

TEST_F(MockCaptureDeviceControllerTest,
       ResetFailsPendingRequestsAndAllowsRetry) {
  base::test::TestFuture<AddResult> first;
  base::test::TestFuture<AddResult> second;
  controller_->AddMockCamera(MakeConfig("camera-1"), first.GetCallback());
  controller_->AddMockCamera(MakeConfig("camera-2"), second.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(2u, provider_.registrations().size());

  controller_->Reset();
  EXPECT_EQ(AddResult::kAborted, first.Take());
  EXPECT_EQ(AddResult::kAborted, second.Take());
  ASSERT_TRUE(provider_.registrations()[0]->disconnected.Wait());
  ASSERT_TRUE(provider_.registrations()[1]->disconnected.Wait());

  base::test::TestFuture<AddResult> retry;
  controller_->AddMockCamera(MakeConfig("camera-1"), retry.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(3u, provider_.registrations().size());
  provider_.Reply(0, RegistrationResult::kSuccess);
  provider_.Reply(1, RegistrationResult::kSuccess);
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  EXPECT_FALSE(first.IsReady());
  EXPECT_FALSE(second.IsReady());
  EXPECT_FALSE(retry.IsReady());
  provider_.Reply(2, RegistrationResult::kSuccess);
  EXPECT_EQ(AddResult::kSuccess, retry.Get());
}

TEST_F(MockCaptureDeviceControllerTest,
       RegisteredCameraCanBeAddedAfterProviderReconnects) {
  base::test::TestFuture<AddResult> first;
  controller_->AddMockCamera(MakeConfig(), first.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(1u, provider_.registrations().size());
  provider_.Reply(0, RegistrationResult::kSuccess);
  ASSERT_EQ(AddResult::kSuccess, first.Get());

  provider_receiver_.reset();
  // The producer closes once the controller processes the provider disconnect.
  ASSERT_TRUE(provider_.registrations()[0]->disconnected.Wait());
  provider_bound_.Clear();

  base::test::TestFuture<AddResult> retry;
  controller_->AddMockCamera(MakeConfig(), retry.GetCallback());
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(2u, provider_.registrations().size());
  EXPECT_FALSE(retry.IsReady());
  provider_.Reply(1, RegistrationResult::kSuccess);
  EXPECT_EQ(AddResult::kSuccess, retry.Get());
  provider_.registrations()[1]->producer.FlushForTesting();
  EXPECT_TRUE(provider_.registrations()[1]->producer.is_connected());
}

TEST_F(MockCaptureDeviceControllerTest, CompletionCanDestroyController) {
  base::test::TestFuture<AddResult> completed;
  controller_->AddMockCamera(
      MakeConfig(),
      base::BindLambdaForTesting(
          [this, callback = completed.GetCallback()](AddResult result) mutable {
            controller_.reset();
            std::move(callback).Run(result);
          }));
  ASSERT_NO_FATAL_FAILURE(FlushRegistrationRequests());
  ASSERT_EQ(1u, provider_.registrations().size());
  EXPECT_FALSE(completed.IsReady());

  provider_.Reply(0, RegistrationResult::kSuccess);
  EXPECT_EQ(AddResult::kSuccess, completed.Get());
  EXPECT_FALSE(controller_);
  ASSERT_TRUE(provider_.registrations()[0]->disconnected.Wait());
}

}  // namespace
}  // namespace content
