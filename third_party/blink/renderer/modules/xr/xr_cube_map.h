// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CUBE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CUBE_MAP_H_

#include "base/types/pass_key.h"
#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"

typedef unsigned int GLenum;

namespace blink {

class WebGLRenderingContextBase;
class WebGLTexture;

// Internal-only helper class for storing and managing cube map data;
class XRCubeMap {
 public:
  explicit XRCubeMap(const device::mojom::blink::XRCubeMap& cube_map);

  WebGLTexture* updateWebGLEnvironmentCube(WebGLRenderingContextBase* context,
                                           WebGLTexture* texture,
                                           GLenum internal_format,
                                           GLenum format,
                                           GLenum type) const;

 private:
  uint32_t width_and_height_ = 0;
  Vector<uint16_t> positive_x_;
  Vector<uint16_t> negative_x_;
  Vector<uint16_t> positive_y_;
  Vector<uint16_t> negative_y_;
  Vector<uint16_t> positive_z_;
  Vector<uint16_t> negative_z_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CUBE_MAP_H_
