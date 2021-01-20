// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_DIRECT_SHARED_IMAGE_VIDEO_PROVIDER_H_
#define MEDIA_GPU_ANDROID_DIRECT_SHARED_IMAGE_VIDEO_PROVIDER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/maybe_render_early_manager.h"
#include "media/gpu/android/shared_image_video_provider.h"
#include "media/gpu/android/video_frame_factory.h"
#include "media/gpu/gles2_decoder_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/gl_bindings.h"

namespace media {
class GpuSharedImageVideoFactory;

// SharedImageVideoProvider implementation that lives on the thread that it's
// created on, but hops to the GPU thread to create new shared images on demand.
class MEDIA_GPU_EXPORT DirectSharedImageVideoProvider
    : public SharedImageVideoProvider {
 public:
  DirectSharedImageVideoProvider(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetStubCB get_stub_cb);
  ~DirectSharedImageVideoProvider() override;

  // SharedImageVideoProvider
  void Initialize(GpuInitCB get_stub_cb) override;
  void RequestImage(ImageReadyCB cb,
                    const ImageSpec& spec,
                    scoped_refptr<gpu::TextureOwner> texture_owner) override;

 private:
  base::SequenceBound<GpuSharedImageVideoFactory> gpu_factory_;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DirectSharedImageVideoProvider);
};

// GpuSharedImageVideoFactory creates SharedImageVideo objects.  It must be run
// on the gpu main thread.
//
// GpuSharedImageVideoFactory is an implementation detail of
// DirectSharedImageVideoProvider.  It's here since we'll likely re-use it for
// the pool.
class GpuSharedImageVideoFactory
    : public gpu::CommandBufferStub::DestructionObserver {
 public:
  explicit GpuSharedImageVideoFactory(
      SharedImageVideoProvider::GetStubCB get_stub_cb);
  ~GpuSharedImageVideoFactory() override;

  // Will run |init_cb| with the shared context current.  |init_cb| should not
  // post, else the context won't be current.
  void Initialize(SharedImageVideoProvider::GpuInitCB init_cb);

  // Similar to SharedImageVideoProvider::ImageReadyCB, but provides additional
  // details for the provider that's using us.
  using FactoryImageReadyCB = SharedImageVideoProvider::ImageReadyCB;

  // Creates a SharedImage for |spec|, and returns it via the callback.
  // TODO(liberato): |texture_owner| is only needed to get the service id, to
  // create the per-frame texture.  All of that is only needed for legacy
  // mailbox support, where we have to have one texture per CodecImage.
  void CreateImage(FactoryImageReadyCB cb,
                   const SharedImageVideoProvider::ImageSpec& spec,
                   scoped_refptr<gpu::TextureOwner> texture_owner);

 private:
  // Creates a SharedImage for |mailbox|, and returns success or failure.
  bool CreateImageInternal(const SharedImageVideoProvider::ImageSpec& spec,
                           scoped_refptr<gpu::TextureOwner> texture_owner,
                           gpu::Mailbox mailbox,
                           scoped_refptr<CodecImage> image);

  void OnWillDestroyStub(bool have_context) override;

  gpu::CommandBufferStub* stub_ = nullptr;

  // A helper for creating textures. Only valid while |stub_| is valid.
  std::unique_ptr<GLES2DecoderHelper> decoder_helper_;

  bool is_vulkan_ = false;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<GpuSharedImageVideoFactory> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuSharedImageVideoFactory);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_DIRECT_SHARED_IMAGE_VIDEO_PROVIDER_H_
