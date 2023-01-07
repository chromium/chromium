// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/core/css/cssom/css_paint_worklet_input.h"

namespace blink {

CSSPaintWorkletInput::CSSPaintWorkletInput(
    const String& name,
    const gfx::SizeF& container_size,
    float effective_zoom,
    int worklet_id,
    PaintWorkletStylePropertyMap::CrossThreadData data,
    Vector<std::unique_ptr<CrossThreadStyleValue>> parsed_input_arguments,
    cc::PaintWorkletInput::PropertyKeys property_keys)
    : PaintWorkletInput(container_size, worklet_id, property_keys),
      name_(name),
      effective_zoom_(effective_zoom),
      style_map_data_(std::move(data)),
      parsed_input_arguments_(std::move(parsed_input_arguments)) {}

}  // namespace blink
