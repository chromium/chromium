/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SLIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SLIDER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_mock_object.h"

namespace blink {

class AXObjectCacheImpl;
class HTMLInputElement;

class AXSlider : public AXLayoutObject {
 public:
  AXSlider(LayoutObject*, AXObjectCacheImpl&);
  ~AXSlider() override = default;

 private:
  HTMLInputElement* GetInputElement() const;
  AXObject* ElementAccessibilityHitTest(const IntPoint&) const final;

  ax::mojom::Role DetermineAccessibilityRole() final;
  bool IsSlider() const final { return true; }
  bool IsControl() const final { return true; }

  void AddChildren() final;

  bool OnNativeSetValueAction(const String&) final;
  AccessibilityOrientation Orientation() const final;

  DISALLOW_COPY_AND_ASSIGN(AXSlider);
};

class AXSliderThumb final : public AXMockObject {
 public:
  explicit AXSliderThumb(AXObjectCacheImpl&);
  ~AXSliderThumb() override = default;

  ax::mojom::Role RoleValue() const override {
    return ax::mojom::Role::kSliderThumb;
  }

 private:
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  LayoutObject* LayoutObjectForRelativeBounds() const override;

  DISALLOW_COPY_AND_ASSIGN(AXSliderThumb);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SLIDER_H_
