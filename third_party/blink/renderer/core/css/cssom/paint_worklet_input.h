// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_INPUT_H_

#include <memory>
#include <utility>

#include "cc/paint/paint_worklet_input.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"

namespace blink {

// PaintWorkletInput contains information that is shared by the native and the
// CSS paint worklet.
class CORE_EXPORT PaintWorkletInput : public cc::PaintWorkletInput {
 public:
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

 protected:
  PaintWorkletInput(const FloatSize& container_size,
                    int worklet_id,
                    cc::PaintWorkletInput::PropertyKeys property_keys)
      : container_size_(container_size),
        worklet_id_(worklet_id),
        property_keys_(std::move(property_keys)) {}

  ~PaintWorkletInput() override = default;

 private:
  const FloatSize container_size_;
  const int worklet_id_;

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
