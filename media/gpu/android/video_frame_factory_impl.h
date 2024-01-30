// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_H_
#define MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_buffer_wait_coordinator.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/frame_info_helper.h"
#include "media/gpu/android/maybe_render_early_manager.h"
#include "media/gpu/android/shared_image_video_provider.h"
#include "media/gpu/android/video_frame_factory.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/gl_bindings.h"

namespace media {
class CodecImageGroup;
class MaybeRenderEarlyManager;

// VideoFrameFactoryImpl creates CodecOutputBuffer backed VideoFrames and tries
// to eagerly render them to their surface to release the buffers back to the
// decoder as soon as possible. It's not thread safe; it should be created, used
// and destructed on a single sequence. It's implemented by proxying calls
// to a helper class hosted on the gpu thread.
class MEDIA_GPU_EXPORT VideoFrameFactoryImpl
    : public VideoFrameFactory,
      public gpu::RefCountedLockHelperDrDc {
 public:
  // Callback used to return a mailbox and release callback for an image. The
  // release callback may be dropped without being run, and the image will be
  // cleaned up properly. The release callback may be called from any thread.
  using ImageReadyCB =
      base::OnceCallback<void(gpu::Mailbox mailbox,
                              VideoFrame::ReleaseMailboxCB release_cb)>;

  using ImageWithInfoReadyCB =
      base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                              FrameInfoHelper::FrameInfo,
                              SharedImageVideoProvider::ImageRecord)>;

  // |get_stub_cb| will be run on |gpu_task_runner|.
  VideoFrameFactoryImpl(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      const gpu::GpuPreferences& gpu_preferences,
      std::unique_ptr<SharedImageVideoProvider> image_provider,
      std::unique_ptr<MaybeRenderEarlyManager> mre_manager,
      std::unique_ptr<FrameInfoHelper> frame_info_helper,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  VideoFrameFactoryImpl(const VideoFrameFactoryImpl&) = delete;
  VideoFrameFactoryImpl& operator=(const VideoFrameFactoryImpl&) = delete;

  ~VideoFrameFactoryImpl() override;

  void Initialize(OverlayMode overlay_mode, InitCB init_cb) override;
  void SetSurfaceBundle(
      scoped_refptr<CodecSurfaceBundle> surface_bundle) override;
  void CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      OnceOutputCB output_cb) override;
  void RunAfterPendingVideoFrames(base::OnceClosure closure) override;
  bool IsStalled() const override;

  // This should be only used for testing.
  void SetCodecBufferWaitCorrdinatorForTesting(
      scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator) {
    codec_buffer_wait_coordinator_ = std::move(codec_buffer_wait_coordinator);
  }

 private:
  void RequestImage(std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
                    ImageWithInfoReadyCB image_ready_cb);
  // ImageReadyCB that will construct a VideoFrame, and forward it to
  // |output_cb| if construction succeeds.  This is static for two reasons.
  // First, we want to snapshot the state of the world when the request is made,
  // in case things like the texture owner change before it's returned.  While
  // it's unclear that MCVD would actually do this (it drains output buffers
  // before switching anything, which guarantees that the VideoFrame has been
  // created and sent to the renderer), it's still much simpler to think about
  // if this uses the same state as the CreateVideoFrame call.
  //
  // Second, this way we don't care about the lifetime of |this|; |output_cb|
  // can worry about it.
  static void CreateVideoFrame_OnImageReady(
      base::WeakPtr<VideoFrameFactoryImpl> thiz,
      OnceOutputCB output_cb,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      bool is_texture_owner_backed,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      VideoPixelFormat pixel_format,
      OverlayMode overlay_mode,
      bool video_frame_copy_required,
      scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
      std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
      FrameInfoHelper::FrameInfo frame_info,
      SharedImageVideoProvider::ImageRecord record);

  void CreateVideoFrame_OnFrameInfoReady(
      ImageWithInfoReadyCB image_ready_cb,
      std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
      FrameInfoHelper::FrameInfo frame_info);

  MaybeRenderEarlyManager* mre_manager() const { return mre_manager_.get(); }

  std::unique_ptr<SharedImageVideoProvider> image_provider_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // The CodecBufferWaitCoordintor that video frames should use, or nullptr.
  scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator_;

  OverlayMode overlay_mode_ = OverlayMode::kDontRequestPromotionHints;

  // Is the video frame copy required?
  bool video_frame_copy_required_ = false;

  // Current group that new CodecImages should belong to.  Do not use this on
  // our thread; everything must be posted to the gpu main thread, including
  // destruction of it.
  scoped_refptr<CodecImageGroup> image_group_;

  std::unique_ptr<MaybeRenderEarlyManager> mre_manager_;

  // Helper to get coded_size and optional Vulkan YCbCrInfo.
  std::unique_ptr<FrameInfoHelper> frame_info_helper_;

  // The current image spec that we'll use to request images.
  SharedImageVideoProvider::ImageSpec image_spec_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoFrameFactoryImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_H_
