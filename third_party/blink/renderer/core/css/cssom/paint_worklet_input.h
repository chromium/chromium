// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_INPUT_H_

#include <memory>
#include <utility>

#include "cc/paint/paint_worklet_input.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"

namespace blink {

// PaintWorkletInput encapsulates the necessary information to run a CSS Paint
// instance (a 'PaintWorklet') for a given target (e.g. the 'background-image'
// property of a particular element). It is used to enable Off-Thread
// PaintWorklet, allowing us to defer the actual JavaScript calls until the
// cc-Raster phase (and even then run the JavaScript on a separate worklet
// thread).
//
// This object is passed cross-thread, but contains thread-unsafe objects (the
// WTF::Strings for |name_| and the WTF::Strings stored in |style_map_|). As
// such PaintWorkletInput must be treated carefully. In essence, it 'belongs' to
// the PaintWorklet thread for the purposes of the WTF::String members. None of
// the WTF::String accessors should be accessed on any thread apart from the
// PaintWorklet thread, where an IsolatedCopy should still be taken.
//
// An IsolatedCopy is still needed on the PaintWorklet thread because
// cc::PaintWorkletInput is thread-safe ref-counted (it is shared between Blink,
// cc-impl, and the cc-raster thread pool), so we *do not know* on what thread
// this object will die - and thus on what thread the WTF::Strings that it
// contains will die.
class CORE_EXPORT PaintWorkletInput : public cc::PaintWorkletInput {
 public:
  PaintWorkletInput(
      const String& name,
      const FloatSize& container_size,
      float effective_zoom,
      float device_scale_factor,
      int worklet_id,
      PaintWorkletStylePropertyMap::CrossThreadData values,
      Vector<std::unique_ptr<CrossThreadStyleValue>> parsed_input_args,
      cc::PaintWorkletInput::PropertyKeys property_keys);

  ~PaintWorkletInput() override = default;

  // PaintWorkletInput implementation
  gfx::SizeF GetSize() const override {
    return gfx::SizeF(container_size_.Width(), container_size_.Height());
  }
  int WorkletId() const override { return worklet_id_; }
  const cc::PaintWorkletInput::PropertyKeys& GetPropertyKeys() const override {
    return property_keys_;
  }

  // These accessors are safe on any thread.
  const FloatSize& ContainerSize() const { return container_size_; }
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

 private:
  const String name_;
  const FloatSize container_size_;
  const float effective_zoom_;
  const float device_scale_factor_;
  const int worklet_id_;
  PaintWorkletStylePropertyMap::CrossThreadData style_map_data_;
  Vector<std::unique_ptr<CrossThreadStyleValue>> parsed_input_arguments_;

  // List of properties associated with this PaintWorkletInput.
  // Kept and initialized here, but used in CC, so using C++ std library types.
  // TODO(xidachen): make this structure account for native property.
  // Instead of pair<string, CompositorElementId>, define
  // struct PropertyKey {
  //   std::string custom_property_name;
  //   enum native_property_type;
  //   CompositorElementId element_id;
  // }
  // PropId uniquely identifies a property value, potentially being animated by
  // the compositor, used by this PaintWorklet as an input at paint time. The
  // worklet provides a list of the properties that it uses as inputs.
  cc::PaintWorkletInput::PropertyKeys property_keys_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_INPUT_H_
