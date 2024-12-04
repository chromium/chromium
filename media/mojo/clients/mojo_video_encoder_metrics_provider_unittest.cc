// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;

namespace media {

constexpr uint64_t kEncoderId = 0u;

class MockMojomVideoEncoderMetricsProvider
    : public mojom::VideoEncoderMetricsProvider {
 public:
  MockMojomVideoEncoderMetricsProvider() = default;
  ~MockMojomVideoEncoderMetricsProvider() override = default;
  // mojom::VideoEncoderMetricsProvider implementation.
  MOCK_METHOD(void,
              Initialize,
              (uint64_t,
               mojom::VideoEncoderUseCase,
               VideoCodecProfile,
               const gfx::Size&,
               bool,
               SVCScalabilityMode),
              (override));
  MOCK_METHOD(void, SetEncodedFrameCount, (uint64_t, uint64_t), (override));
  MOCK_METHOD(void,
              SetError,
              (uint64_t, const media::EncoderStatus&),
              (override));
  MOCK_METHOD(void, Complete, (uint64_t), (override));
};

class MojoVideoEncoderMetricsProviderTest : public ::testing::Test {
 public:
  MojoVideoEncoderMetricsProviderTest() = default;

  static constexpr auto kUseCase = mojom::VideoEncoderUseCase::kMediaRecorder;

  void SetUp() override {
    mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote;
    mojo_encoder_metrics_receiver_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<MockMojomVideoEncoderMetricsProvider>(),
        pending_remote.InitWithNewPipeAndPassReceiver());
    mojo_encoder_metrics_provider_factory_ =
        base::MakeRefCounted<MojoVideoEncoderMetricsProviderFactory>(
            kUseCase, std::move(pending_remote));
    mojo_encoder_metrics_provider_ = mojo_encoder_metrics_provider_factory_
                                         ->CreateVideoEncoderMetricsProvider();
  }

  void TearDown() override {
    // The destruction of a mojo::SelfOwnedReceiver closes the bound message
    // pipe but does not destroy the implementation object(s): this needs to
    // happen manually by Close()ing it.
    if (mojo_encoder_metrics_receiver_) {
      mojo_encoder_metrics_receiver_->Close();
    }
  }

  MockMojomVideoEncoderMetricsProvider* mock_mojo_receiver() {
    return reinterpret_cast<MockMojomVideoEncoderMetricsProvider*>(
        mojo_encoder_metrics_receiver_->impl());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  // This member holds on to the mock implementation of the "service" side.
  mojo::SelfOwnedReceiverRef<mojom::VideoEncoderMetricsProvider>
      mojo_encoder_metrics_receiver_;

  scoped_refptr<MojoVideoEncoderMetricsProviderFactory>
      mojo_encoder_metrics_provider_factory_;
  std::unique_ptr<VideoEncoderMetricsProvider> mojo_encoder_metrics_provider_;
};

TEST_F(MojoVideoEncoderMetricsProviderTest, CreateAndDestroy) {}

TEST_F(MojoVideoEncoderMetricsProviderTest, CreateAndBoundAndInitialize) {
  constexpr auto kCodecProfile = media::VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;

  InSequence s;
  EXPECT_CALL(*mock_mojo_receiver(),
              Initialize(kEncoderId, kUseCase, kCodecProfile, kEncodeSize,
                         kIsHardwareEncoder, kSVCMode));
  mojo_encoder_metrics_provider_->Initialize(kCodecProfile, kEncodeSize,
                                             kIsHardwareEncoder, kSVCMode);
  EXPECT_CALL(*mock_mojo_receiver(), Complete(kEncoderId));
  mojo_encoder_metrics_provider_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoVideoEncoderMetricsProviderTest,
       CreateAndBoundAndInitializeSetEncodedFrameCount) {
  constexpr auto kCodecProfile = media::VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;

  InSequence s;
  EXPECT_CALL(*mock_mojo_receiver(),
              Initialize(kEncoderId, kUseCase, kCodecProfile, kEncodeSize,
                         kIsHardwareEncoder, kSVCMode));
  mojo_encoder_metrics_provider_->Initialize(kCodecProfile, kEncodeSize,
                                             kIsHardwareEncoder, kSVCMode);
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*mock_mojo_receiver(), SetEncodedFrameCount(kEncoderId, 1u));
  mojo_encoder_metrics_provider_->IncrementEncodedFrameCount();
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < 98; i++) {
    mojo_encoder_metrics_provider_->IncrementEncodedFrameCount();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*mock_mojo_receiver(), SetEncodedFrameCount(kEncoderId, 100u));
  mojo_encoder_metrics_provider_->IncrementEncodedFrameCount();
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < 99; i++) {
    mojo_encoder_metrics_provider_->IncrementEncodedFrameCount();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*mock_mojo_receiver(), SetEncodedFrameCount(kEncoderId, 200u));
  mojo_encoder_metrics_provider_->IncrementEncodedFrameCount();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoVideoEncoderMetricsProviderTest,
       CreateAndBoundAndInitializeAndSetError) {
  constexpr auto kCodecProfile = media::VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;

  InSequence s;
  EXPECT_CALL(*mock_mojo_receiver(),
              Initialize(kEncoderId, kUseCase, kCodecProfile, kEncodeSize,
                         kIsHardwareEncoder, kSVCMode));
  mojo_encoder_metrics_provider_->Initialize(kCodecProfile, kEncodeSize,
                                             kIsHardwareEncoder, kSVCMode);
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*mock_mojo_receiver(), SetEncodedFrameCount(kEncoderId, 1u));
  mojo_encoder_metrics_provider_->IncrementEncodedFrameCount();
  mojo_encoder_metrics_provider_->IncrementEncodedFrameCount();
  mojo_encoder_metrics_provider_->IncrementEncodedFrameCount();
  base::RunLoop().RunUntilIdle();
  const media::EncoderStatus kErrorStatus(
      media::EncoderStatus::Codes::kEncoderFailedEncode, "Encoder failed");
  mojo_encoder_metrics_provider_->SetError(kErrorStatus);
  EXPECT_CALL(*mock_mojo_receiver(), SetError(kEncoderId, kErrorStatus));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoVideoEncoderMetricsProviderTest, CreateAndSetError_NoCall) {
  InSequence s;
  const media::EncoderStatus kErrorStatus(
      media::EncoderStatus::Codes::kEncoderFailedEncode, "Encoder failed");
  mojo_encoder_metrics_provider_->SetError(kErrorStatus);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoVideoEncoderMetricsProviderTest,
       CreateAndIncrementEncodedFrameCount_NoCall) {
  InSequence s;
  mojo_encoder_metrics_provider_->IncrementEncodedFrameCount();
  base::RunLoop().RunUntilIdle();
}

class MojoVideoEncoderMetricsProviderFactoryTest : public ::testing::Test {
 public:
  MojoVideoEncoderMetricsProviderFactoryTest() = default;

  static constexpr auto kUseCase = mojom::VideoEncoderUseCase::kWebRTC;

  void SetUp() override {
    mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote;
    mojo_encoder_metrics_receiver_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<MockMojomVideoEncoderMetricsProvider>(),
        pending_remote.InitWithNewPipeAndPassReceiver());

    mojo_encoder_metrics_provider_factory_ =
        base::MakeRefCounted<MojoVideoEncoderMetricsProviderFactory>(
            mojom::VideoEncoderUseCase::kWebRTC, std::move(pending_remote));
  }

  void TearDown() override {
    // The destruction of a mojo::SelfOwnedReceiver closes the bound message
    // pipe but does not destroy the implementation object(s): this needs to
    // happen manually by Close()ing it.
    if (mojo_encoder_metrics_receiver_) {
      mojo_encoder_metrics_receiver_->Close();
    }
  }

  MockMojomVideoEncoderMetricsProvider* mock_mojo_receiver() {
    return reinterpret_cast<MockMojomVideoEncoderMetricsProvider*>(
        mojo_encoder_metrics_receiver_->impl());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // This member holds on to the mock implementation of the "service" side.
  mojo::SelfOwnedReceiverRef<mojom::VideoEncoderMetricsProvider>
      mojo_encoder_metrics_receiver_;

  scoped_refptr<MojoVideoEncoderMetricsProviderFactory>
      mojo_encoder_metrics_provider_factory_;
};

TEST_F(MojoVideoEncoderMetricsProviderFactoryTest, CreateMultipleEncoders) {
  constexpr auto kCodecProfile = media::VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;

  auto metrics_provider0 = mojo_encoder_metrics_provider_factory_
                               ->CreateVideoEncoderMetricsProvider();
  auto metrics_provider1 = mojo_encoder_metrics_provider_factory_
                               ->CreateVideoEncoderMetricsProvider();

  InSequence s;
  EXPECT_CALL(*mock_mojo_receiver(),
              Initialize(0u, kUseCase, kCodecProfile, kEncodeSize,
                         kIsHardwareEncoder, kSVCMode));
  EXPECT_CALL(*mock_mojo_receiver(),
              Initialize(1u, kUseCase, kCodecProfile, kEncodeSize,
                         kIsHardwareEncoder, kSVCMode));
  metrics_provider0->Initialize(kCodecProfile, kEncodeSize, kIsHardwareEncoder,
                                kSVCMode);
  metrics_provider1->Initialize(kCodecProfile, kEncodeSize, kIsHardwareEncoder,
                                kSVCMode);

  EXPECT_CALL(*mock_mojo_receiver(), Complete(0u));
  EXPECT_CALL(*mock_mojo_receiver(), Complete(1u));
  metrics_provider0.reset();
  metrics_provider1.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoVideoEncoderMetricsProviderFactoryTest,
       CanOperateAfterDestroyingOtherMetricsProvider) {
  constexpr auto kCodecProfile = media::VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;

  auto metrics_provider0 = mojo_encoder_metrics_provider_factory_
                               ->CreateVideoEncoderMetricsProvider();
  auto metrics_provider1 = mojo_encoder_metrics_provider_factory_
                               ->CreateVideoEncoderMetricsProvider();

  InSequence s;
  EXPECT_CALL(*mock_mojo_receiver(),
              Initialize(0u, kUseCase, kCodecProfile, kEncodeSize,
                         kIsHardwareEncoder, kSVCMode));
  EXPECT_CALL(*mock_mojo_receiver(),
              Initialize(1u, kUseCase, kCodecProfile, kEncodeSize,
                         kIsHardwareEncoder, kSVCMode));
  metrics_provider0->Initialize(kCodecProfile, kEncodeSize, kIsHardwareEncoder,
                                kSVCMode);
  metrics_provider1->Initialize(kCodecProfile, kEncodeSize, kIsHardwareEncoder,
                                kSVCMode);

  EXPECT_CALL(*mock_mojo_receiver(), Complete(0u));
  metrics_provider0.reset();

  EXPECT_CALL(*mock_mojo_receiver(), SetEncodedFrameCount(1u, 1u));
  metrics_provider1->IncrementEncodedFrameCount();
  EXPECT_CALL(*mock_mojo_receiver(), Complete(1u));
  metrics_provider1.reset();

  base::RunLoop().RunUntilIdle();
}

}  // namespace media
