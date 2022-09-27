// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_NATIVE_CSS_PAINT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_NATIVE_CSS_PAINT_DEFINITION_H_

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/modules/csspaint/nativepaint/native_paint_definition.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class LocalFrame;
class CSSProperty;
class Element;

class MODULES_EXPORT NativeCssPaintDefinition : public NativePaintDefinition {
 public:
  ~NativeCssPaintDefinition() override = default;

  // Validation function for determining if a value / interpolable_value is
  // supported on the compositor.
  using ValueFilter = bool (*)(const Element* element,
                               const CSSValue* value,
                               const InterpolableValue* interpolable_value);

  static Animation* GetAnimationForProperty(
      const Element* element,
      const CSSProperty& property,
      ValueFilter filter = DefaultValueFilter);

  static bool CanGetValueFromKeyframe(const Element* element,
                                      const PropertySpecificKeyframe* frame,
                                      const KeyframeEffectModelBase* model,
                                      ValueFilter filter);

  // Default validator for a keyframe value, which accepts any non-null value
  // as being supported. Replace with a property specific validator as needed.
  static bool DefaultValueFilter(const Element* element,
                                 const CSSValue* value,
                                 const InterpolableValue* interpolable_value);

 protected:
  NativeCssPaintDefinition(LocalFrame*,
                           PaintWorkletInput::PaintWorkletInputType);
  NativeCssPaintDefinition() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_NATIVE_CSS_PAINT_DEFINITION_H_
