// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/shared_image_pool.h"

#include "gpu/command_buffer/client/shared_image_interface.h"

namespace {

gpu::ImageInfo GetImageInfo(scoped_refptr<gpu::ClientImage> image) {
  auto shared_image = image->GetSharedImage();
  return gpu::ImageInfo(
      shared_image->size(), shared_image->format(), shared_image->usage(),
      shared_image->color_space(), shared_image->surface_origin(),
      shared_image->alpha_type(), shared_image->buffer_usage());
}

}  // namespace

namespace gpu {

// Implementation of the ClientImage class.
ClientImage::ClientImage(scoped_refptr<ClientSharedImage> shared_image)
    : shared_image_(std::move(shared_image)) {
  CHECK(shared_image_);
  sync_token_ = shared_image_->creation_sync_token();
}

ClientImage::~ClientImage() {
  CHECK(shared_image_);
  shared_image_->UpdateDestructionSyncToken(std::move(sync_token_));
}

const scoped_refptr<ClientSharedImage>& ClientImage::GetSharedImage() const {
  return shared_image_;
}

const SyncToken& ClientImage::GetSyncToken() const {
  return sync_token_;
}

void ClientImage::SetReleaseSyncToken(SyncToken release_sync_token) {
  sync_token_ = std::move(release_sync_token);
}

const SharedImagePoolId& ClientImage::GetPoolIdForTesting() const {
  return pool_id_;
}

SharedImagePoolBase::SharedImagePoolBase(
    const SharedImagePoolId& pool_id,
    const ImageInfo& image_info,
    std::string_view debug_label,
    const scoped_refptr<SharedImageInterface> sii,
    std::optional<uint8_t> max_pool_size,
    std::optional<base::TimeDelta> unused_resource_expiration_time)
    : pool_id_(pool_id),
      image_info_(image_info),
      debug_label_(debug_label),
      sii_(std::move(sii)),
      max_pool_size_(std::move(max_pool_size)),
      unused_resource_expiration_time_(
          std::move(unused_resource_expiration_time)) {}

SharedImagePoolBase::~SharedImagePoolBase() {
  ClearInternal();
}

size_t SharedImagePoolBase::GetPoolSizeForTesting() const {
  return image_pool_.size();
}

bool SharedImagePoolBase::IsReclaimTimerRunningForTesting() const {
  return unused_resources_reclaim_timer_.IsRunning();
}

scoped_refptr<ClientSharedImage>
SharedImagePoolBase::CreateSharedImageInternal() {
  CHECK(sii_);
  if (image_info_.buffer_usage.has_value()) {
    // Creates a Mappable shared image. Note that eventually when shared image
    // usage is merged with buffer usage, there will be only one method to
    // create both mappable and non-mappable shared image. These 2 paths will be
    // merged after that.
    return sii_->CreateSharedImage(
        {image_info_.format, image_info_.size, image_info_.color_space,
         image_info_.surface_origin, image_info_.alpha_type, image_info_.usage,
         debug_label_ + "Mappable"},
        gpu::kNullSurfaceHandle, image_info_.buffer_usage.value());
  } else {
    return sii_->CreateSharedImage(
        {image_info_.format, image_info_.size, image_info_.color_space,
         image_info_.surface_origin, image_info_.alpha_type, image_info_.usage,
         debug_label_},
        gpu::kNullSurfaceHandle);
  }
}

scoped_refptr<ClientImage> SharedImagePoolBase::GetImageFromPoolInternal() {
  if (!image_pool_.empty()) {
    auto image = image_pool_.back();
    image_pool_.pop_back();
    return image;
  }
  return nullptr;
}

void SharedImagePoolBase::ReleaseImageInternal(
    scoped_refptr<ClientImage> image) {
  if (!image || (GetImageInfo(image) != image_info_)) {
    return;
  }

  // Ensure that the |image| belongs to |this| pool.
  CHECK_EQ(image->pool_id_.ToString(), pool_id_.ToString());

  // Ensure that there is only one reference which the current |image| and
  // clients are not accidentally keeping more references alive while releasing
  // this |image|.
  CHECK(image->HasOneRef());

  // Recycle the image into the pool only if the pool is not full or if
  // |max_pool_size_| is not specified. Otherwise the last ref of |image| here
  // will get destroyed automatically.
  if (!max_pool_size_.has_value() ||
      image_pool_.size() < max_pool_size_.value()) {
    // Update the last used time.
    image->last_used_time_ = base::TimeTicks::Now();
    image_pool_.push_back(std::move(image));
    MaybePostUnusedResourcesReclaimTask();
  }
}

void SharedImagePoolBase::ClearInternal() {
  image_pool_.clear();
  CHECK(sii_);
  // ClientSharedImage destructor calls DestroySharedImage which in turn ensures
  // that the deferred destroy request is flushed. Thus, clients don't need to
  // call SharedImageInterface::Flush explicitly.
}

void SharedImagePoolBase::ReconfigureInternal(const ImageInfo& image_info) {
  if (image_info_ == image_info) {
    return;
  }
  // If ImageInfo does not matches, we clear the existing images.
  ClearInternal();
  image_info_ = image_info;
}

void SharedImagePoolBase::MaybePostUnusedResourcesReclaimTask() {
  if (!unused_resources_reclaim_timer_.IsRunning() && !image_pool_.empty() &&
      unused_resource_expiration_time_.has_value()) {
    unused_resources_reclaim_timer_.Start(
        FROM_HERE, unused_resource_expiration_time_.value(),
        base::BindOnce(&SharedImagePoolBase::ClearOldUnusedResources,
                       base::Unretained(this)));
  }
}

void SharedImagePoolBase::ClearOldUnusedResources() {
  CHECK(unused_resource_expiration_time_.has_value());

  // Get the current time.
  auto now = base::TimeTicks::Now();

  // Clear the resources that have expired.
  // Remove elements that satisfy the predicate by using std::remove_if and
  // erase.
  auto new_end =
      std::remove_if(image_pool_.begin(), image_pool_.end(),
                     [this, now](const scoped_refptr<ClientImage>& resource) {
                       return now - resource->last_used_time_ >=
                              unused_resource_expiration_time_.value();
                     });

  // Erase the "removed" elements from the vector.
  image_pool_.erase(new_end, image_pool_.end());
  // ClientSharedImage destructor calls DestroySharedImage which in turn ensures
  // that the deferred destroy request is flushed. Thus, clients don't need to
  // call SharedImageInterface::Flush explicitly.

  // Reclaim unused resource again.
  MaybePostUnusedResourcesReclaimTask();
}

}  // namespace gpu
