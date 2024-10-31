// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_layer_shared_image_manager.h"

#include "third_party/blink/renderer/modules/xr/xr_layer.h"

namespace blink {

void XRLayerSharedImageManager::Reset() {
  layer_shared_images_.clear();
}

void XRLayerSharedImageManager::SetLayerSharedImages(
    XRLayer* layer,
    const scoped_refptr<gpu::ClientSharedImage>& color_shared_image,
    const gpu::SyncToken& color_sync_token,
    const scoped_refptr<gpu::ClientSharedImage>& camera_image_shared_image,
    const gpu::SyncToken& camera_image_sync_token) {
  XRLayerSharedImages shared_images = {
      {color_shared_image, color_sync_token},
      {camera_image_shared_image, camera_image_sync_token}};
  layer_shared_images_.Set(layer->layer_id(), shared_images);
}

const XRLayerSharedImages& XRLayerSharedImageManager::GetLayerSharedImages(
    const XRLayer* layer) const {
  auto shared_images = layer_shared_images_.find(layer->layer_id());
  if (shared_images == layer_shared_images_.end()) {
    return empty_shared_images_;
  }

  return shared_images->value;
}

}  // namespace blink
