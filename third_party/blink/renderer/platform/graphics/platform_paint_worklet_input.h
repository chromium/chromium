// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PLATFORM_PAINT_WORKLET_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PLATFORM_PAINT_WORKLET_INPUT_H_

#include "cc/trees/layer_tree_painter.h"

namespace blink {

class PLATFORM_EXPORT PlatformPaintWorkletInput : public cc::PaintWorkletInput {
 public:
  PlatformPaintWorkletInput(const String& name,
                            const FloatSize& container_size,
                            float effective_zoom)
      : name_(name),
        container_size_(container_size),
        effective_zoom_(effective_zoom) {}

  ~PlatformPaintWorkletInput() override = default;

  const String& Name() const { return name_; }
  const FloatSize& ContainerSize() const { return container_size_; }
  float EffectiveZoom() const { return effective_zoom_; }

 private:
  String name_;
  FloatSize container_size_;
  float effective_zoom_;
  // TODO(crbug.com/895579): add a cross thread style map.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHIC_PLATFORM_PAINT_WORKLET_INPUT_H_
