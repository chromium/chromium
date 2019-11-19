// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_POOLED_SHARED_IMAGE_VIDEO_PROVIDER_H_
#define MEDIA_GPU_ANDROID_POOLED_SHARED_IMAGE_VIDEO_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/gpu/android/shared_image_video_provider.h"
#include "media/gpu/command_buffer_helper.h"

namespace media {

class PooledSharedImageVideoProviderTest;

// Provider class for shared images.
class MEDIA_GPU_EXPORT PooledSharedImageVideoProvider
    : public SharedImageVideoProvider {
 public:
  // Helper class that processes image returns on the gpu thread.
  class GpuHelper {
   public:
    GpuHelper() = default;
    virtual ~GpuHelper() = default;

    // Called (on the gpu thread) to handle image return.
    virtual void OnImageReturned(
        const gpu::SyncToken& sync_token,
        scoped_refptr<CodecImageHolder> codec_image_holder,
        base::OnceClosure cb) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(GpuHelper);
  };

  // Create a default implementation.  |provider| is the underlying provider to
  // create shared images.
  static std::unique_ptr<PooledSharedImageVideoProvider> Create(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetStubCB get_stub_cb,
      std::unique_ptr<SharedImageVideoProvider> provider);

  ~PooledSharedImageVideoProvider() override;

  // SharedImageVideoProvider
  void Initialize(GpuInitCB gpu_init_cb) override;
  void RequestImage(ImageReadyCB cb,
                    const ImageSpec& spec,
                    scoped_refptr<gpu::TextureOwner> texture_owner) override;

 private:
  friend class PooledSharedImageVideoProviderTest;

  PooledSharedImageVideoProvider(
      base::SequenceBound<GpuHelper> gpu_helper,
      std::unique_ptr<SharedImageVideoProvider> provider);

  class GpuHelperImpl : public GpuHelper {
   public:
    GpuHelperImpl(GetStubCB get_stub_cb);
    ~GpuHelperImpl() override;

    // GpuHelper
    void OnImageReturned(const gpu::SyncToken& sync_token,
                         scoped_refptr<CodecImageHolder> codec_image_holder,
                         base::OnceClosure cb) override;

   private:
    void OnSyncTokenCleared(scoped_refptr<CodecImageHolder> codec_image_holder,
                            base::OnceClosure cb);

    scoped_refptr<CommandBufferHelper> command_buffer_helper_;
    base::WeakPtrFactory<GpuHelperImpl> weak_factory_;
  };

  // Record of on image from |provider|.
  class PooledImage : public base::RefCounted<PooledImage> {
   public:
    PooledImage(const ImageSpec& spec, ImageRecord record);

    ImageSpec spec;
    // The original record, including the original reuse callback.
    ImageRecord record;

   private:
    virtual ~PooledImage();

    friend class base::RefCounted<PooledImage>;
  };

  // One request from the client that's pending an image.
  class PendingRequest {
   public:
    PendingRequest(const ImageSpec& spec, ImageReadyCB cb);
    ~PendingRequest();
    ImageSpec spec;
    ImageReadyCB cb;
    std::unique_ptr<CodecOutputBuffer> output_buffer;
    scoped_refptr<gpu::TextureOwner> texture_owner;
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb;
  };

  // Called by |provider_| when a new image is created.
  void OnImageCreated(ImageSpec spec, ImageRecord record);

  // Called by our client when it runs the release cb, to notify us that the
  // image is no longer in use.
  void OnImageReturned(scoped_refptr<PooledImage> pooled_image,
                       const gpu::SyncToken& sync_token);

  // Given a free image |pooled_image| that is not in our pool, use it to either
  // fulfill a pending request, add it to the pool, or discard it.
  void ProcessFreePooledImage(scoped_refptr<PooledImage> pooled_image);

  // Underlying provider that we use to construct images.
  std::unique_ptr<SharedImageVideoProvider> provider_;

  // All currently unused images.
  std::list<scoped_refptr<PooledImage>> pool_;

  // Spec for all images in |pool_|.
  ImageSpec pool_spec_;

  std::list<PendingRequest> pending_requests_;

  base::SequenceBound<GpuHelper> gpu_helper_;

  base::WeakPtrFactory<PooledSharedImageVideoProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PooledSharedImageVideoProvider);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_POOLED_SHARED_IMAGE_VIDEO_PROVIDER_H_
