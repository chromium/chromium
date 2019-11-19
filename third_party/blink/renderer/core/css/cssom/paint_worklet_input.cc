// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"

namespace blink {

PaintWorkletInput::PaintWorkletInput(
    const String& name,
    const FloatSize& container_size,
    float effective_zoom,
    float device_scale_factor,
    int worklet_id,
    PaintWorkletStylePropertyMap::CrossThreadData data,
    Vector<std::unique_ptr<CrossThreadStyleValue>> parsed_input_arguments,
    cc::PaintWorkletInput::PropertyKeys property_keys)
    : name_(name.IsolatedCopy()),
      container_size_(container_size),
      effective_zoom_(effective_zoom),
      device_scale_factor_(device_scale_factor),
      worklet_id_(worklet_id),
      style_map_data_(std::move(data)),
      parsed_input_arguments_(std::move(parsed_input_arguments)),
      property_keys_(std::move(property_keys)) {}

}  // namespace blink
