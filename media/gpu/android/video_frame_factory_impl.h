// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_
#define MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_

#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/surface_texture_gl_owner.h"
#include "media/gpu/android/video_frame_factory.h"
#include "media/gpu/gles2_decoder_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/gl_bindings.h"

namespace media {
class CodecImageGroup;
class GpuVideoFrameFactory;
class TexturePool;

// VideoFrameFactoryImpl creates CodecOutputBuffer backed VideoFrames and tries
// to eagerly render them to their surface to release the buffers back to the
// decoder as soon as possible. It's not thread safe; it should be created, used
// and destructed on a single sequence. It's implemented by proxying calls
// to a helper class hosted on the gpu thread.
class MEDIA_GPU_EXPORT VideoFrameFactoryImpl : public VideoFrameFactory {
 public:
  // |get_stub_cb| will be run on |gpu_task_runner|.
  VideoFrameFactoryImpl(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetStubCb get_stub_cb);
  ~VideoFrameFactoryImpl() override;

  void Initialize(bool wants_promotion_hint,
                  bool use_texture_owner_as_overlays,
                  InitCb init_cb) override;
  void SetSurfaceBundle(
      scoped_refptr<AVDASurfaceBundle> surface_bundle) override;
  void CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      OnceOutputCb output_cb) override;
  void RunAfterPendingVideoFrames(base::OnceClosure closure) override;

 private:
  // The gpu thread side of the implementation.
  std::unique_ptr<GpuVideoFrameFactory> gpu_video_frame_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  GetStubCb get_stub_cb_;

  // The texture owner that video frames should use, or nullptr.
  scoped_refptr<TextureOwner> texture_owner_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(VideoFrameFactoryImpl);
};

// GpuVideoFrameFactory is an implementation detail of VideoFrameFactoryImpl. It
// may be created on any thread but only accessed on the gpu thread thereafter.
class GpuVideoFrameFactory
    : public gpu::CommandBufferStub::DestructionObserver {
 public:
  GpuVideoFrameFactory();
  ~GpuVideoFrameFactory() override;

  scoped_refptr<TextureOwner> Initialize(
      bool wants_promotion_hint,
      bool use_texture_owner_as_overlays,
      VideoFrameFactory::GetStubCb get_stub_cb);

  // Creates and returns a VideoFrame with its ReleaseMailboxCB.
  void CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<TextureOwner> texture_owner,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      VideoFrameFactory::OnceOutputCb output_cb,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Set our image group.  Must be called before the first call to
  // CreateVideoFrame occurs.
  void SetImageGroup(scoped_refptr<CodecImageGroup> image_group);

 private:
  // Creates an AbstractTexture and VideoFrame.
  void CreateVideoFrameInternal(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<TextureOwner> texture_owner,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      scoped_refptr<VideoFrame>* video_frame_out,
      std::unique_ptr<gpu::gles2::AbstractTexture>* texture_out,
      CodecImage** codec_image_out);

  void OnWillDestroyStub(bool have_context) override;

  // Removes |image| from |images_|.
  void OnImageDestructed(CodecImage* image);

  // Outstanding images that should be considered for early rendering.
  std::vector<CodecImage*> images_;

  gpu::CommandBufferStub* stub_;

  // Callback to notify us that an image has been destroyed.
  CodecImage::DestructionCb destruction_cb_;

  // Do we want promotion hints from the compositor?
  bool wants_promotion_hint_ = false;

  // Indicates whether texture owner can be promoted to an overlay.
  bool use_texture_owner_as_overlays_ = false;

  // A helper for creating textures. Only valid while |stub_| is valid.
  std::unique_ptr<GLES2DecoderHelper> decoder_helper_;

  // Current image group to which new images (frames) will be added.  We'll
  // replace this when SetImageGroup() is called.
  scoped_refptr<CodecImageGroup> image_group_;

  // Pool which owns all the textures that we create.
  scoped_refptr<TexturePool> texture_pool_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<GpuVideoFrameFactory> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(GpuVideoFrameFactory);
};

namespace internal {

// Tries to render CodecImages to their backing surfaces when it's valid to do
// so. This lets us release codec buffers back to their codecs as soon as
// possible so that decoding can progress smoothly.
// Templated on the image type for testing.
template <typename Image>
void MEDIA_GPU_EXPORT MaybeRenderEarly(std::vector<Image*>* image_vector_ptr) {
  auto& images = *image_vector_ptr;
  if (images.empty())
    return;

  // Find the latest image rendered to the front buffer (if any).
  base::Optional<size_t> front_buffer_index;
  for (int i = images.size() - 1; i >= 0; --i) {
    if (images[i]->was_rendered_to_front_buffer()) {
      front_buffer_index = i;
      break;
    }
  }

  // If there's no image in the front buffer we can safely render one.
  if (!front_buffer_index) {
    // Iterate until we successfully render one to skip over invalidated images.
    for (size_t i = 0; i < images.size(); ++i) {
      if (images[i]->RenderToFrontBuffer()) {
        front_buffer_index = i;
        break;
      }
    }
    // If we couldn't render anything there's nothing more to do.
    if (!front_buffer_index)
      return;
  }

  // Try to render the image following the front buffer to the back buffer.
  size_t back_buffer_index = *front_buffer_index + 1;
  if (back_buffer_index < images.size() &&
      images[back_buffer_index]->is_texture_owner_backed()) {
    images[back_buffer_index]->RenderToTextureOwnerBackBuffer();
  }
}

}  // namespace internal

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_
