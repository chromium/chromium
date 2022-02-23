// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_PAINT_WORKLET_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_PAINT_WORKLET_INPUT_H_

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"

namespace blink {

// CSSPaintWorkletInput encapsulates the necessary information to run a CSS
// Paint instance (a 'PaintWorklet') for a given target (e.g. the
// 'background-image' property of a particular element). It is used to enable
// Off-Thread PaintWorklet, allowing us to defer the actual JavaScript calls
// until the cc-Raster phase (and even then run the JavaScript on a separate
// worklet thread).
//
// This object is passed cross-thread, but contains thread-unsafe objects (the
// WTF::Strings for |name_| and the WTF::Strings stored in |style_map_|). As
// such CSSPaintWorkletInput must be treated carefully. In essence, it 'belongs'
// to the PaintWorklet thread for the purposes of the WTF::String members. None
// of the WTF::String accessors should be accessed on any thread apart from the
// PaintWorklet thread, where an IsolatedCopy should still be taken.
//
// An IsolatedCopy is still needed on the PaintWorklet thread because
// cc::PaintWorkletInput is thread-safe ref-counted (it is shared between Blink,
// cc-impl, and the cc-raster thread pool), so we *do not know* on what thread
// this object will die - and thus on what thread the WTF::Strings that it
// contains will die.
class CORE_EXPORT CSSPaintWorkletInput : public PaintWorkletInput {
 public:
  CSSPaintWorkletInput(
      const String& name,
      const gfx::SizeF& container_size,
      float effective_zoom,
      float device_scale_factor,
      int worklet_id,
      PaintWorkletStylePropertyMap::CrossThreadData values,
      Vector<std::unique_ptr<CrossThreadStyleValue>> parsed_input_args,
      cc::PaintWorkletInput::PropertyKeys property_keys);

  ~CSSPaintWorkletInput() override = default;

  // These accessors are safe on any thread.
  float EffectiveZoom() const { return effective_zoom_; }
  float DeviceScaleFactor() const { return device_scale_factor_; }
  const Vector<std::unique_ptr<CrossThreadStyleValue>>& ParsedInputArguments()
      const {
    return parsed_input_arguments_;
  }

  // These should only be accessed on the PaintWorklet thread.
  String NameCopy() const { return name_.IsolatedCopy(); }
  PaintWorkletStylePropertyMap::CrossThreadData StyleMapData() const {
    return PaintWorkletStylePropertyMap::CopyCrossThreadData(style_map_data_);
  }

  PaintWorkletInputType GetType() const override {
    return PaintWorkletInputType::kCSS;
  }

 private:
  const String name_;
  const float effective_zoom_;
  const float device_scale_factor_;
  PaintWorkletStylePropertyMap::CrossThreadData style_map_data_;
  Vector<std::unique_ptr<CrossThreadStyleValue>> parsed_input_arguments_;
};

template <>
struct DowncastTraits<CSSPaintWorkletInput> {
  static bool AllowFrom(const cc::PaintWorkletInput& worklet_input) {
    auto* input = DynamicTo<PaintWorkletInput>(worklet_input);
    return input && AllowFrom(*input);
  }

  static bool AllowFrom(const PaintWorkletInput& worklet_input) {
    return worklet_input.GetType() ==
           PaintWorkletInput::PaintWorkletInputType::kCSS;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_PAINT_WORKLET_INPUT_H_
