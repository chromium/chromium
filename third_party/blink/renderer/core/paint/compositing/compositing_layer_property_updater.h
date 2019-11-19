// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_LAYER_PROPERTY_UPDATER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_LAYER_PROPERTY_UPDATER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;

class CompositingLayerPropertyUpdater {
  STATIC_ONLY(CompositingLayerPropertyUpdater);

 public:
  static void Update(const LayoutObject&);
};

}  // namespace blink

#endif  // PaintPropertyTreeBuilder_h
