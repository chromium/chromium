// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/service/mock_texture_owner.h"
#include "gpu/command_buffer/service/ref_counted_lock_for_test.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android/test_destruction_observable.h"
#include "media/base/limits.h"
#include "media/gpu/android/codec_buffer_wait_coordinator.h"
#include "media/gpu/android/maybe_render_early_manager.h"
#include "media/gpu/android/mock_codec_image.h"
#include "media/gpu/android/mock_shared_image_video_provider.h"
#include "media/gpu/android/shared_image_video_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::Return;
using testing::SaveArg;

namespace gpu {
class CommandBufferStub;
}  // namespace gpu

namespace media {

class MockMaybeRenderEarlyManager : public MaybeRenderEarlyManager {
 public:
  MOCK_METHOD1(SetSurfaceBundle, void(scoped_refptr<CodecSurfaceBundle>));
  MOCK_METHOD1(AddCodecImage, void(scoped_refptr<CodecImageHolder>));
  MOCK_METHOD0(MaybeRenderEarly, void());
};

class MockFrameInfoHelper : public FrameInfoHelper,
                            public DestructionObservable {
 public:
  void GetFrameInfo(std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
                    FrameInfoReadyCB cb) override {
    FrameInfo info;
    info.coded_size = buffer_renderer->size();
    info.visible_rect = gfx::Rect(info.coded_size);

    std::move(cb).Run(std::move(buffer_renderer), info);
  }

  MOCK_CONST_METHOD0(IsStalled, bool());
};

class VideoFrameFactoryImplTest : public testing::Test {
 public:
  VideoFrameFactoryImplTest()
      : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
    auto image_provider = std::make_unique<MockSharedImageVideoProvider>();
    image_provider_raw_ = image_provider.get();

    auto mre_manager = std::make_unique<MockMaybeRenderEarlyManager>();
    mre_manager_raw_ = mre_manager.get();

    auto info_helper = std::make_unique<MockFrameInfoHelper>();
    info_helper_raw_ = info_helper.get();

    impl_ = std::make_unique<VideoFrameFactoryImpl>(
        task_runner_, gpu_preferences_, std::move(image_provider),
        std::move(mre_manager), std::move(info_helper),
        features::NeedThreadSafeAndroidMedia()
            ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
            : nullptr);
    auto texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
        0, nullptr, nullptr, true);
    auto codec_buffer_wait_coordinator =
        base::MakeRefCounted<CodecBufferWaitCoordinator>(
            std::move(texture_owner),
            features::NeedThreadSafeAndroidMedia()
                ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
                : nullptr);

    // Provide a non-null |codec_buffer_wait_coordinator| to |impl_|.
    impl_->SetCodecBufferWaitCorrdinatorForTesting(
        std::move(codec_buffer_wait_coordinator));
  }

  ~VideoFrameFactoryImplTest() override = default;

  struct {
    gfx::Size coded_size{100, 100};
    gfx::Rect visible_rect{coded_size};
    gfx::Size natural_size{coded_size};
    gfx::ColorSpace color_space{gfx::ColorSpace::CreateSRGBLinear()};
  } video_frame_params_;

  void RequestVideoFrame() {
    auto output_buffer = CodecOutputBuffer::CreateForTesting(
        0, video_frame_params_.coded_size, video_frame_params_.color_space,
        std::nullopt);
    ASSERT_TRUE(VideoFrame::IsValidConfig(
        PIXEL_FORMAT_ARGB, VideoFrame::STORAGE_OPAQUE,
        video_frame_params_.coded_size, video_frame_params_.visible_rect,
        video_frame_params_.natural_size));

    // Save a copy in case the test wants it.
    output_buffer_raw_ = output_buffer.get();

    // We should get a call to the output callback, but no calls to the
    // provider.
    // TODO(liberato): Verify that it's sending the proper TextureOwner.
    // However, we haven't actually given it a TextureOwner yet.
    output_buffer_raw_ = output_buffer.get();
    EXPECT_CALL(*image_provider_raw_, MockRequestImage());

    impl_->CreateVideoFrame(std::move(output_buffer), base::TimeDelta(),
                            video_frame_params_.natural_size,
                            base::NullCallback(), output_cb_.Get());
    base::RunLoop().RunUntilIdle();

    // TODO(liberato): Verify that it requested a shared image.
  }

  // |release_cb_called_flag| will be set when the record's |release_cb| runs.
  SharedImageVideoProvider::ImageRecord MakeImageRecord(
      bool* release_cb_called_flag = nullptr) {
    SharedImageVideoProvider::ImageRecord record;
    record.shared_image = gpu::ClientSharedImage::CreateForTesting();
    if (release_cb_called_flag)
      *release_cb_called_flag = false;
    record.release_cb = base::BindOnce(
        [](bool* flag, const gpu::SyncToken&) {
          if (flag)
            *flag = true;
        },
        base::Unretained(release_cb_called_flag));
    auto codec_image =
        base::MakeRefCounted<MockCodecImage>(gfx::Size(100, 100));
    record.codec_image_holder = base::MakeRefCounted<CodecImageHolder>(
        task_runner_, codec_image,
        features::NeedThreadSafeAndroidMedia()
            ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
            : nullptr);
    return record;
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<VideoFrameFactoryImpl> impl_;

  raw_ptr<MockMaybeRenderEarlyManager> mre_manager_raw_ = nullptr;
  raw_ptr<MockSharedImageVideoProvider> image_provider_raw_ = nullptr;
  raw_ptr<MockFrameInfoHelper> info_helper_raw_ = nullptr;

  // Most recently created CodecOutputBuffer.
  raw_ptr<CodecOutputBuffer> output_buffer_raw_ = nullptr;

  // Sent to |impl_| by RequestVideoFrame..
  base::MockCallback<VideoFrameFactory::OnceOutputCB> output_cb_;

  std::unique_ptr<DestructionObserver> ycbcr_destruction_observer_;

  gpu::GpuPreferences gpu_preferences_;
};

TEST_F(VideoFrameFactoryImplTest, ImageProviderInitFailure) {
  // If the image provider fails to init, then our init cb should be called with
  // no TextureOwner.
  EXPECT_CALL(*image_provider_raw_, Initialize_(_))
      .Times(1)
      .WillOnce(RunOnceCallback<0>(nullptr));
  base::MockCallback<VideoFrameFactory::InitCB> init_cb;
  EXPECT_CALL(init_cb, Run(scoped_refptr<gpu::TextureOwner>(nullptr)));
  impl_->Initialize(VideoFrameFactory::OverlayMode::kDontRequestPromotionHints,
                    init_cb.Get());
  base::RunLoop().RunUntilIdle();

  // TODO(liberato): for testing, we could just skip calling the gpu init cb,
  // since |impl_| doesn't know or care if it's called.  that way, we don't need
  // to mock out making the callback work.  would be nice, though.
}

TEST_F(VideoFrameFactoryImplTest,
       SetSurfaceBundleForwardsToMaybeRenderEarlyManager) {
  // Sending a non-null CodecSurfaceBundle should forward it to |mre_manager|.
  // Also provide a non-null TextureOwner to it.
  scoped_refptr<CodecSurfaceBundle> surface_bundle =
      base::MakeRefCounted<CodecSurfaceBundle>(
          base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(0, nullptr,
                                                                nullptr, true),
          features::NeedThreadSafeAndroidMedia()
              ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
              : nullptr);
  EXPECT_CALL(*mre_manager_raw_, SetSurfaceBundle(surface_bundle));
  impl_->SetSurfaceBundle(surface_bundle);
  base::RunLoop().RunUntilIdle();
}

TEST_F(VideoFrameFactoryImplTest, CreateVideoFrameFailsIfUnsupportedFormat) {
  // Sending an unsupported format should cause an early failure, without a
  // thread hop.
  gfx::Size coded_size(limits::kMaxDimension + 1, limits::kMaxDimension + 1);
  gfx::Rect visible_rect(coded_size);
  gfx::Size natural_size(0, 0);
  auto output_buffer = CodecOutputBuffer::CreateForTesting(
      0, coded_size, gfx::ColorSpace(), std::nullopt);
  ASSERT_FALSE(VideoFrame::IsValidConfig(PIXEL_FORMAT_ARGB,
                                         VideoFrame::STORAGE_OPAQUE, coded_size,
                                         visible_rect, natural_size));

  // We should get a call to the output callback, but no calls to the provider.
  base::MockCallback<VideoFrameFactory::OnceOutputCB> output_cb;
  EXPECT_CALL(output_cb, Run(scoped_refptr<VideoFrame>(nullptr)));
  EXPECT_CALL(*image_provider_raw_, MockRequestImage()).Times(0);

  impl_->CreateVideoFrame(std::move(output_buffer), base::TimeDelta(),
                          natural_size, base::NullCallback(), output_cb.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(VideoFrameFactoryImplTest, CreateVideoFrameSucceeds) {
  // Creating a video frame calls through to the image provider, and forwards a
  // VideoFrame to the output cb.
  //
  // TODO(liberato): Consider testing the metadata values.
  RequestVideoFrame();

  // Call the ImageReadyCB.
  scoped_refptr<VideoFrame> frame;
  EXPECT_CALL(output_cb_, Run(_)).WillOnce(SaveArg<0>(&frame));
  bool release_cb_called_flag = false;
  auto record = MakeImageRecord(&release_cb_called_flag);
  scoped_refptr<CodecImage> codec_image(
      record.codec_image_holder->codec_image_raw());
  image_provider_raw_->ProvideOneRequestedImage(&record);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(frame, nullptr);

  // Make sure that it set the output buffer properly.
  EXPECT_EQ(codec_image->get_codec_output_buffer_for_testing(),
            output_buffer_raw_);

  EXPECT_EQ(frame->coded_size(), video_frame_params_.coded_size);
  EXPECT_EQ(frame->natural_size(), video_frame_params_.natural_size);
  EXPECT_EQ(frame->visible_rect(), video_frame_params_.visible_rect);
  EXPECT_EQ(frame->ColorSpace(), video_frame_params_.color_space);

  // Destroy the VideoFrame, and verify that our release cb is called.
  EXPECT_FALSE(release_cb_called_flag);
  frame = nullptr;
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(release_cb_called_flag);
}

TEST_F(VideoFrameFactoryImplTest,
       DestroyingFactoryDuringVideoFrameCreationDoesntCrash) {
  // We should be able to destroy |impl_| while a VideoFrame is pending, and
  // nothing bad should happen.
  RequestVideoFrame();
  impl_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(VideoFrameFactoryImplTest, IsStalled) {
  EXPECT_CALL(*info_helper_raw_, IsStalled()).WillOnce(Return(false));
  EXPECT_FALSE(impl_->IsStalled());
  EXPECT_CALL(*info_helper_raw_, IsStalled()).WillOnce(Return(true));
  EXPECT_TRUE(impl_->IsStalled());
}

}  // namespace media
