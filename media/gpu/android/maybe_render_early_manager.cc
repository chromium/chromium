// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/maybe_render_early_manager.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "media/gpu/android/codec_image_group.h"
#include "media/gpu/android/codec_surface_bundle.h"

namespace media {

// GPU-thread side of the default MaybeRenderEarlyManager.  This handles doing
// the actual rendering.
class GpuMaybeRenderEarlyImpl {
 public:
  GpuMaybeRenderEarlyImpl() {}

  GpuMaybeRenderEarlyImpl(const GpuMaybeRenderEarlyImpl&) = delete;
  GpuMaybeRenderEarlyImpl& operator=(const GpuMaybeRenderEarlyImpl&) = delete;

  ~GpuMaybeRenderEarlyImpl() = default;

  void SetCodecImageGroup(scoped_refptr<CodecImageGroup> image_group) {
    image_group_ = std::move(image_group);
  }

  void AddCodecImage(scoped_refptr<CodecImageHolder> codec_image_holder) {
    // Register to find out when this CodecImage is unused, so that we can try
    // to render a new image early.
    codec_image_holder->codec_image_raw()->AddUnusedCB(base::BindOnce(
        &GpuMaybeRenderEarlyImpl::OnImageUnused, weak_factory_.GetWeakPtr()));

    DCHECK(!base::Contains(images_, codec_image_holder->codec_image_raw()));
    images_.push_back(codec_image_holder->codec_image_raw());

    // Add |image| to our current image group.  This makes sure that any overlay
    // lasts as long as the images.  For TextureOwner, it doesn't do much.
    image_group_->AddCodecImage(codec_image_holder->codec_image_raw());
  }

  void MaybeRenderEarly(scoped_refptr<gpu::RefCountedLock> drdc_lock) {
    base::AutoLockMaybe auto_lock(drdc_lock ? drdc_lock->GetDrDcLockPtr()
                                            : nullptr);
    internal::MaybeRenderEarly(&images_);
  }

 private:
  void OnImageUnused(CodecImage* image) {
    // |image| is no longer used, so try to render a new image speculatively.
    DCHECK(base::Contains(images_, image));
    // Remember that |image_group_| might not be the same one that |image|
    // belongs to.
    std::erase(images_, image);
    internal::MaybeRenderEarly(&images_);
  }

  // Outstanding images that should be considered for early rendering.
  std::vector<raw_ptr<CodecImage, VectorExperimental>> images_;

  // Current image group to which new images (frames) will be added.  We'll
  // replace this when SetImageGroup() is called.
  scoped_refptr<CodecImageGroup> image_group_;

  base::WeakPtrFactory<GpuMaybeRenderEarlyImpl> weak_factory_{this};
};

// Default implementation of MaybeRenderEarlyManager.  Lives on whatever thread
// you like, but will hop to the gpu thread to do real work.
class MaybeRenderEarlyManagerImpl : public MaybeRenderEarlyManager,
                                    public gpu::RefCountedLockHelperDrDc {
 public:
  MaybeRenderEarlyManagerImpl(
      scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
      scoped_refptr<gpu::RefCountedLock> drdc_lock)
      : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
        gpu_task_runner_(gpu_task_runner),
        gpu_impl_(std::move(gpu_task_runner)) {}

  MaybeRenderEarlyManagerImpl(const MaybeRenderEarlyManagerImpl&) = delete;
  MaybeRenderEarlyManagerImpl& operator=(const MaybeRenderEarlyManagerImpl&) =
      delete;

  ~MaybeRenderEarlyManagerImpl() override = default;

  void SetSurfaceBundle(
      scoped_refptr<CodecSurfaceBundle> surface_bundle) override {
    // Start a new image group.  Note that there's no reason that we can't have
    // more than one group per surface bundle; it's okay if we're called
    // multiple times with the same surface bundle.  It just helps to combine
    // the callbacks if we don't, especially since AndroidOverlay doesn't know
    // how to remove destruction callbacks.  That's one reason why we don't just
    // make the CodecImage register itself.  The other is that the threading is
    // easier if we do it this way, since the image group is constructed on the
    // proper thread to talk to the overlay.
    auto image_group = base::MakeRefCounted<CodecImageGroup>(
        gpu_task_runner_, std::move(surface_bundle), GetDrDcLock());

    // Give the image group to |gpu_impl_|.  Note that we don't drop our ref to
    // |image_group| on this thread.  It can only be constructed here.
    gpu_impl_.AsyncCall(&GpuMaybeRenderEarlyImpl::SetCodecImageGroup)
        .WithArgs(std::move(image_group));
  }

  void AddCodecImage(
      scoped_refptr<CodecImageHolder> codec_image_holder) override {
    gpu_impl_.AsyncCall(&GpuMaybeRenderEarlyImpl::AddCodecImage)
        .WithArgs(std::move(codec_image_holder));
  }

  void MaybeRenderEarly() override {
    gpu_impl_.AsyncCall(&GpuMaybeRenderEarlyImpl::MaybeRenderEarly)
        .WithArgs(GetDrDcLock());
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> gpu_task_runner_;

  // Gpu-side.
  base::SequenceBound<GpuMaybeRenderEarlyImpl> gpu_impl_;
};

// static
std::unique_ptr<MaybeRenderEarlyManager> MaybeRenderEarlyManager::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<gpu::RefCountedLock> lock) {
  return std::make_unique<MaybeRenderEarlyManagerImpl>(std::move(task_runner),
                                                       std::move(lock));
}

}  // namespace media
