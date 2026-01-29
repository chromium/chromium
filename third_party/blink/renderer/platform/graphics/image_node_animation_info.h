// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_NODE_ANIMATION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_NODE_ANIMATION_INFO_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"

namespace blink {

enum class ImageAnimationEnum : uint8_t {
  kNormal,
  kRunning,
  kPaused,
  kMaxEnumValue = kPaused
};

struct ImageNodeAnimationInfo {
 public:
  ImageNodeAnimationInfo() = default;
  ImageNodeAnimationInfo(DOMNodeId node_id, ImageAnimationEnum image_animation)
      : node_id(node_id), image_animation(image_animation) {}
  DOMNodeId node_id = kInvalidDOMNodeId;
  ImageAnimationEnum image_animation;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_NODE_ANIMATION_INFO_H_
