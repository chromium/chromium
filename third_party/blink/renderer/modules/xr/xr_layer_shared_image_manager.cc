// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_layer_shared_image_manager.h"
#include "third_party/blink/renderer/modules/xr/xr_layer.h"

namespace blink {

void XRLayerSharedImageManager::Reset() {
  shared_images_.clear();
}

void XRLayerSharedImageManager::SetSharedImages(
    XRLayer* base_layer,
    Vector<XRSharedImageData> shared_images) {
  shared_images_ = std::move(shared_images);
  // assign layer id for the base layer
  if (base_layer) {
    auto it = std::find_if(shared_images_.begin(), shared_images_.end(),
                           [](const XRSharedImageData& shared_image) {
                             return shared_image.source ==
                                    XRSharedImageSource::kBaseLayer;
                           });
    if (it != shared_images_.end()) {
      it->layer_id = base_layer->layer_id();
    }
  }
}

const XRSharedImageData& XRLayerSharedImageManager::CameraSharedImage() const {
  auto it =
      std::find_if(shared_images_.begin(), shared_images_.end(),
                   [](const XRSharedImageData& shared_image) {
                     return shared_image.source == XRSharedImageSource::kCamera;
                   });
  if (it != shared_images_.end()) {
    return *it;
  }

  return empty_;
}

const XRSharedImageData& XRLayerSharedImageManager::LayerSharedImage(
    device::LayerId layer_id) const {
  auto it = std::find_if(shared_images_.begin(), shared_images_.end(),
                         [layer_id](const XRSharedImageData& shared_image) {
                           return shared_image.layer_id == layer_id;
                         });

  if (it != shared_images_.end()) {
    return *it;
  }

  return empty_;
}

bool XRLayerSharedImageManager::HasLayerSharedImage(
    device::LayerId layer_id) const {
  return std::any_of(shared_images_.begin(), shared_images_.end(),
                     [layer_id](const XRSharedImageData& shared_image) {
                       return shared_image.layer_id == layer_id;
                     });
}

}  // namespace blink
