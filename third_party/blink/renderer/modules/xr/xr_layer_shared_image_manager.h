// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_SHARED_IMAGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_SHARED_IMAGE_MANAGER_H_

#include <optional>

#include "gpu/command_buffer/client/client_shared_image.h"
#include "third_party/blink/renderer/modules/xr/xr_id_hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class XRLayer;

enum class XRSharedImageSource {
  kInvalid = 0,
  kCamera = 1,
  kBaseLayer = 2,
  kCompositionLayer = 3,
};

// TODO(crbug.com/40286368): Remove |sync_token_| once the sync token is
// incorporated into |gpu::ClientSharedImage|.
struct XRSharedImageData {
  XRSharedImageSource source = XRSharedImageSource::kInvalid;
  device::LayerId layer_id = device::kInvalidLayerId;
  scoped_refptr<gpu::ClientSharedImage> shared_image;
  gpu::SyncToken sync_token;
};

class XRLayerSharedImageManager {
 public:
  XRLayerSharedImageManager() = default;
  ~XRLayerSharedImageManager() = default;

  void Reset();
  void SetSharedImages(XRLayer* base_layer,
                       Vector<XRSharedImageData> shared_images);
  const XRSharedImageData& CameraSharedImage() const;
  const XRSharedImageData& LayerSharedImage(device::LayerId layer_id) const;
  bool HasLayerSharedImage(device::LayerId layer_id) const;

 private:
  // keep all shared images in a vector
  // including camera's shared image
  Vector<XRSharedImageData> shared_images_;
  XRSharedImageData empty_ = {};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_SHARED_IMAGE_MANAGER_H_
