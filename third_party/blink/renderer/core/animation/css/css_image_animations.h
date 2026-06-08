// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_IMAGE_ANIMATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_IMAGE_ANIMATIONS_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/platform/graphics/css_image_animation_data_interface.h"
#include "third_party/blink/renderer/platform/graphics/image_node_animation_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Node;

class CORE_EXPORT CSSImageAnimations final : public ElementImageAnimationData {
  DISALLOW_NEW();

 public:
  CSSImageAnimations() = default;
  CSSImageAnimations(const CSSImageAnimations&) = delete;
  CSSImageAnimations& operator=(const CSSImageAnimations&) = delete;

  void Clear() { image_animation_data_.clear(); }

  std::optional<ImageAnimationData> GetImageAnimationData(
      ImageResourceContent*) const override;
  void SetImageAnimationData(ImageResourceContent*,
                             ImageAnimationData) override;
  void EraseImageAnimationData(ImageResourceContent*) override;

  static ImageNodeAnimationInfo CreateImageNodeAnimationInfo(
      Node*,
      ImageResourceContent*,
      ImageAnimationEnum);

  void Trace(Visitor*) const;

 private:
  // CSS Image Animation state for each animated ImageResourceContent by this
  // element.
  // See BitmapImage::PaintImageForCurrentFrameWithInfo() for details.
  HeapHashMap<WeakMember<ImageResourceContent>, ImageAnimationData>
      image_animation_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_IMAGE_ANIMATIONS_H_
