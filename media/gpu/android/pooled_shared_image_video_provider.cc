// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/pooled_shared_image_video_provider.h"

#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace media {

// static
std::unique_ptr<PooledSharedImageVideoProvider>
PooledSharedImageVideoProvider::Create(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCB get_stub_cb,
    std::unique_ptr<SharedImageVideoProvider> provider,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  return base::WrapUnique(new PooledSharedImageVideoProvider(
      base::SequenceBound<GpuHelperImpl>(std::move(gpu_task_runner),
                                         std::move(get_stub_cb)),
      std::move(provider), std::move(drdc_lock)));
}

PooledSharedImageVideoProvider::PooledImage::PooledImage(const ImageSpec& spec,
                                                         ImageRecord record)
    : spec(spec), record(std::move(record)) {}

PooledSharedImageVideoProvider::PooledImage::~PooledImage() = default;

PooledSharedImageVideoProvider::PendingRequest::PendingRequest(
    const ImageSpec& spec,
    ImageReadyCB cb)
    : spec(spec), cb(std::move(cb)) {}

PooledSharedImageVideoProvider::PendingRequest::~PendingRequest() = default;

PooledSharedImageVideoProvider::PooledSharedImageVideoProvider(
    base::SequenceBound<GpuHelper> gpu_helper,
    std::unique_ptr<SharedImageVideoProvider> provider,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
      provider_(std::move(provider)),
      gpu_helper_(std::move(gpu_helper)),
      weak_factory_(this) {}

// Note that this will drop everything in |pool_|, which will call all the
// release callbacks for the underlying byffer.
PooledSharedImageVideoProvider::~PooledSharedImageVideoProvider() = default;

// SharedImageVideoProvider
void PooledSharedImageVideoProvider::Initialize(GpuInitCB gpu_init_cb) {
  provider_->Initialize(std::move(gpu_init_cb));
}

void PooledSharedImageVideoProvider::RequestImage(ImageReadyCB cb,
                                                  const ImageSpec& spec) {
  // See if the pool matches the requested spec.
  if (pool_spec_ != spec) {
    // Nope -- mark any outstanding images for destruction and start a new pool.
    // Note that this calls all the release callbacks.
    pool_.clear();

    // Any images added to the pool should match |spec|.
    pool_spec_ = spec;
  }

  // Push this onto the pending requests.
  // IMPORTANT BUT SUBTLE NOTE: |spec| doesn't mention the TextureOwner, but it
  // is sent to the provider so it must also match the one that was used with
  // |spec|.  We assume that the generation id will be updated by our caller
  // whenever the TextureOwner changes.  While this is fragile, it's also just
  // a temporary thing.  Keeping a strong ref to |texture_owner| would probably
  // work, but it's not good to keep refs to those around longer than needed.
  // It might be okay to do that directly, since the request (if any) that's
  // pending for it would have the strong ref, so maybe we could just add it
  // here too.
  pending_requests_.emplace_back(spec, std::move(cb));

  // Are there any free images in the pool?  If so, then pop one and use it to
  // process the request we just pushed, assuming that it's the most recent.  We
  // could optimize this call out if |pending_requensts_| wasn't empty before,
  // since we know it doesn't match the pool spec if the pool's not empty.  As
  // it is, it will just pop and re-push the pooled buffer in the (rare) case
  // that the pool doesn't match.
  if (!pool_.empty()) {
    auto front = std::move(pool_.front());
    pool_.pop_front();
    ProcessFreePooledImage(front);
    // TODO(liberato): See if skipping the return if |pool_| is now empty is
    // helpful, especially during start-up.  Alternatively, just request some
    // constant number of images (~5) when the pool spec changes, then add them
    // one at a time if needed.
    return;
  }

  // Request a new image, since we don't have enough.  There might be some
  // outstanding that will be returned, but we'd like to have enough not to wait
  // on them.  This has the nice property that everything in |pending_requests_|
  // will have an image delivered in order for it.  Note that we might not
  // exactly match up returned (new) images to the requests; there might be
  // intervening returns of existing images from the client that happen to match
  // if we switch from spec A => spec B => spec A, but that's okay.  We can be
  // sure that there are at least as many that will arrive as we need.
  auto ready_cb =
      base::BindOnce(&PooledSharedImageVideoProvider::OnImageCreated,
                     weak_factory_.GetWeakPtr(), spec);
  provider_->RequestImage(std::move(ready_cb), spec);
}

void PooledSharedImageVideoProvider::OnImageCreated(ImageSpec spec,
                                                    ImageRecord record) {
  // Wrap |record| up for the pool, and process it.
  scoped_refptr<PooledImage> pooled_image =
      base::MakeRefCounted<PooledImage>(std::move(spec), std::move(record));
  ProcessFreePooledImage(pooled_image);
}

void PooledSharedImageVideoProvider::OnImageReturned(
    scoped_refptr<PooledImage> pooled_image,
    const gpu::SyncToken& sync_token) {
  // An image has been returned to us.  Wait for |sync_token| and then send it
  // to ProcessFreePooledImage to re-use / pool / delete.
  gpu_helper_.AsyncCall(&GpuHelper::OnImageReturned)
      .WithArgs(sync_token, pooled_image->record.codec_image_holder,
                base::BindPostTaskToCurrentDefault(base::BindOnce(
                    &PooledSharedImageVideoProvider::ProcessFreePooledImage,
                    weak_factory_.GetWeakPtr(), pooled_image)),
                GetDrDcLock());
}

void PooledSharedImageVideoProvider::ProcessFreePooledImage(
    scoped_refptr<PooledImage> pooled_image) {
  // Are there any requests pending?
  if (pending_requests_.size()) {
    // See if |record| matches the top request.  If so, fulfill it, else drop
    // |record| since we don't need it.  Note that it's possible to have pending
    // requests that don't match the pool spec; the pool spec is the most recent
    // request.  There might be other ones that were made before that which we
    // didn't fulfill yet.
    auto& front = pending_requests_.front();
    if (pooled_image->spec == front.spec) {
      // Construct a record that notifies us when the image is released.
      // TODO(liberato): Don't copy fields this way.
      ImageRecord record;
      record.shared_image = pooled_image->record.shared_image;
      record.is_vulkan = pooled_image->record.is_vulkan;
      record.codec_image_holder = pooled_image->record.codec_image_holder;
      // The release CB notifies us instead of |provider_|.
      record.release_cb = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&PooledSharedImageVideoProvider::OnImageReturned,
                         weak_factory_.GetWeakPtr(), std::move(pooled_image)),
          gpu::SyncToken());

      // Save the callback and remove the request, in case |cb| calls us back.
      auto cb = std::move(front.cb);
      pending_requests_.pop_front();

      std::move(cb).Run(std::move(record));
      return;
    }

    // Can't fulfill the topmost request.  Discard |pooled_image|, even if it
    // matches the pool.  The reason is that any pending requests will have
    // images created for them, which we'll use when they arrive.  It would be
    // okay to store |pooled_image| in the pool if it matches, but then we'd
    // have more pooled images than we expect.
    return;
  }

  // There are no outstanding image requests, or the top one doesn't match
  // |pooled_image|.  If this image is compatible with the pool, then pool it.
  // Otherwise, discard it.

  // See if |record| matches |pool_spec_|.  If not, then drop it.  Otherwise,
  // pool it for later.  Note that we don't explicitly call the release cb,
  // since dropping the image is sufficient to notify |provider_|.  Note that
  // we've already waited for any sync token at this point, so it's okay if we
  // don't provide one to the underlying release cb.
  if (pool_spec_ != pooled_image->spec)
    return;

  // Add it to the pool.
  pool_.push_front(std::move(pooled_image));
}

PooledSharedImageVideoProvider::GpuHelperImpl::GpuHelperImpl(
    GetStubCB get_stub_cb)
    : weak_factory_(this) {
  gpu::CommandBufferStub* stub = get_stub_cb.Run();
  if (stub) {
    command_buffer_helper_ = CommandBufferHelper::Create(stub);
  }
}

PooledSharedImageVideoProvider::GpuHelperImpl::~GpuHelperImpl() = default;

void PooledSharedImageVideoProvider::GpuHelperImpl::OnImageReturned(
    const gpu::SyncToken& sync_token,
    scoped_refptr<CodecImageHolder> codec_image_holder,
    base::OnceClosure cb,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  auto on_sync_token_cleared_cb = base::BindOnce(
      &GpuHelperImpl::OnSyncTokenCleared, weak_factory_.GetWeakPtr(),
      std::move(codec_image_holder), std::move(cb), std::move(drdc_lock));
  command_buffer_helper_->WaitForSyncToken(sync_token,
                                           std::move(on_sync_token_cleared_cb));
}

void PooledSharedImageVideoProvider::GpuHelperImpl::OnSyncTokenCleared(
    scoped_refptr<CodecImageHolder> codec_image_holder,
    base::OnceClosure cb,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  {
    base::AutoLockMaybe auto_lock(drdc_lock ? drdc_lock->GetDrDcLockPtr()
                                            : nullptr);
    codec_image_holder->codec_image_raw()->NotifyUnused();
  }

  // Do this last, since |cb| might post to some other thread.
  std::move(cb).Run();
}

}  // namespace media
