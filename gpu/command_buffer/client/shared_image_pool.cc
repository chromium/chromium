// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/shared_image_pool.h"

#include "gpu/command_buffer/client/shared_image_interface.h"

namespace {

gpu::ImageInfo GetImageInfo(scoped_refptr<gpu::ClientImage> image) {
  auto shared_image = image->GetSharedImage();
  return gpu::ImageInfo(shared_image->size(), shared_image->format(),
                        shared_image->usage(), shared_image->color_space(),
                        shared_image->surface_origin(),
                        shared_image->alpha_type());
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

SharedImagePoolBase::SharedImagePoolBase(
    const ImageInfo& image_info,
    const scoped_refptr<SharedImageInterface> sii,
    std::optional<uint8_t> max_pool_size)
    : image_info_(image_info),
      sii_(std::move(sii)),
      max_pool_size_(std::move(max_pool_size)) {}

SharedImagePoolBase::~SharedImagePoolBase() {
  ClearInternal();
}

size_t SharedImagePoolBase::GetPoolSizeForTesting() const {
  return image_pool_.size();
}

scoped_refptr<ClientSharedImage>
SharedImagePoolBase::CreateSharedImageInternal() {
  CHECK(sii_);
  return sii_->CreateSharedImage(
      {image_info_.format, image_info_.size, image_info_.color_space,
       image_info_.surface_origin, image_info_.alpha_type, image_info_.usage,
       "SharedImagePoolBase"},
      gpu::kNullSurfaceHandle);
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
  // Ensure that there is only one reference which the current |image| and
  // clients are not accidentally keeping more references alive while releasing
  // this |image|.
  CHECK(image->HasOneRef());

  // Recycle the image into the pool only if the pool is not full or if
  // |max_pool_size_| is not specified. Otherwise the last ref of |image| here
  // will get destroyed automatically.
  if (!max_pool_size_.has_value() ||
      image_pool_.size() < max_pool_size_.value()) {
    image_pool_.push_back(std::move(image));
  }
}

void SharedImagePoolBase::ClearInternal() {
  // Explicitly clear the pool and delete all images.
  for (auto& image : image_pool_) {
    auto shared_image = image->GetSharedImage();
    CHECK(shared_image);

    // Note that |sync_token_| has to be waited upon before the image
    // is re-used or deleted to ensure that previous user has finished using it.
    shared_image->UpdateDestructionSyncToken(image->sync_token_);
  }
  image_pool_.clear();
}

void SharedImagePoolBase::ReconfigureInternal(const ImageInfo& image_info) {
  if (image_info_ == image_info) {
    return;
  }
  // If ImageInfo does not matches, we clear the existing images.
  ClearInternal();
  image_info_ = image_info;
}

}  // namespace gpu
