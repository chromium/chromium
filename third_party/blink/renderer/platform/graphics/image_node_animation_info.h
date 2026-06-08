// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_NODE_ANIMATION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_NODE_ANIMATION_INFO_H_

#include <optional>

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"

namespace blink {

class ElementImageAnimationData;
class ImageResourceContent;

enum class ImageAnimationEnum : uint8_t {
  kNormal,
  kRunning,
  kPaused,
  kStopped,
  kMaxEnumValue = kStopped
};

struct ImageNodeAnimationInfo {
  STACK_ALLOCATED();

 public:
  ImageNodeAnimationInfo() = default;
  ImageNodeAnimationInfo(DOMNodeId node_id,
                         ImageAnimationEnum image_animation,
                         ElementImageAnimationData* animation_data,
                         ImageResourceContent* image)
      : node_id(node_id),
        image_animation(image_animation),
        animation_data(animation_data),
        image(image) {}
  DOMNodeId node_id = kInvalidDOMNodeId;
  ImageAnimationEnum image_animation;
  ElementImageAnimationData* animation_data = nullptr;
  ImageResourceContent* image = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_NODE_ANIMATION_INFO_H_
