// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_NATIVE_PAINT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_NATIVE_PAINT_DEFINITION_H_

#include "third_party/blink/renderer/modules/csspaint/paint_definition.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class MODULES_EXPORT NativePaintDefinition : public PaintDefinition {
 public:
  ~NativePaintDefinition() override = default;
  void Trace(Visitor* visitor) const override {
    PaintDefinition::Trace(visitor);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVEPAINT_NATIVE_PAINT_DEFINITION_H_
