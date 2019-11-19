// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_IMAGE_LIST_PROPERTY_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_IMAGE_LIST_PROPERTY_FUNCTIONS_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

using StyleImageList = HeapVector<Member<StyleImage>, 1>;

class ImageListPropertyFunctions {
 public:
  static void GetInitialImageList(const CSSProperty&, StyleImageList* result) {
    result->clear();
  }

  static void GetImageList(const CSSProperty& property,
                           const ComputedStyle& style,
                           StyleImageList* result) {
    const FillLayer* fill_layer = nullptr;
    switch (property.PropertyID()) {
      case CSSPropertyID::kBackgroundImage:
        fill_layer = &style.BackgroundLayers();
        break;
      case CSSPropertyID::kWebkitMaskImage:
        fill_layer = &style.MaskLayers();
        break;
      default:
        NOTREACHED();
        return;
    }

    result->clear();
    while (fill_layer) {
      result->push_back(fill_layer->GetImage());
      fill_layer = fill_layer->Next();
    }
  }

  static void SetImageList(const CSSProperty& property,
                           ComputedStyle& style,
                           const StyleImageList* image_list) {
    FillLayer* fill_layer = nullptr;
    switch (property.PropertyID()) {
      case CSSPropertyID::kBackgroundImage:
        fill_layer = &style.AccessBackgroundLayers();
        break;
      case CSSPropertyID::kWebkitMaskImage:
        fill_layer = &style.AccessMaskLayers();
        break;
      default:
        NOTREACHED();
        return;
    }

    FillLayer* prev = nullptr;
    for (wtf_size_t i = 0; i < image_list->size(); i++) {
      if (!fill_layer)
        fill_layer = prev->EnsureNext();
      fill_layer->SetImage(image_list->at(i));
      prev = fill_layer;
      fill_layer = fill_layer->Next();
    }
    while (fill_layer) {
      fill_layer->ClearImage();
      fill_layer = fill_layer->Next();
    }
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_IMAGE_LIST_PROPERTY_FUNCTIONS_H_
