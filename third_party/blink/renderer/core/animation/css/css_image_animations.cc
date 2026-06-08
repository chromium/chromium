// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_image_animations.h"

#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

std::optional<ImageAnimationData> CSSImageAnimations::GetImageAnimationData(
    ImageResourceContent* image) const {
  DCHECK(image);
  auto it = image_animation_data_.find(image);
  if (it == image_animation_data_.end()) {
    return std::nullopt;
  }
  return it->value;
}

void CSSImageAnimations::SetImageAnimationData(ImageResourceContent* image,
                                               ImageAnimationData entry) {
  DCHECK(image);
  image_animation_data_.Set(image, entry);
}

void CSSImageAnimations::EraseImageAnimationData(ImageResourceContent* image) {
  DCHECK(image);
  image_animation_data_.erase(image);
}

// static
ImageNodeAnimationInfo CSSImageAnimations::CreateImageNodeAnimationInfo(
    Node* node,
    ImageResourceContent* image,
    ImageAnimationEnum image_animation) {
  if (!RuntimeEnabledFeatures::CSSImageAnimationEnabled() || !image ||
      !image->GetImage() || !image->GetImage()->MaybeAnimated() || !node) {
    return ImageNodeAnimationInfo();
  }
  Element* element = node->IsDocumentNode()
                         ? To<Document>(node)->documentElement()
                         : DynamicTo<Element>(node);
  DCHECK(element);
  CSSImageAnimations* animation_data =
      &element->EnsureElementAnimations().CssImageAnimations();
  return ImageNodeAnimationInfo(node->GetDomNodeId(), image_animation,
                                animation_data, image);
}

void CSSImageAnimations::Trace(Visitor* visitor) const {
  visitor->Trace(image_animation_data_);
}

}  // namespace blink
