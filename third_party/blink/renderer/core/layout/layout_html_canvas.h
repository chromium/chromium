/*
 * Copyright (C) 2004, 2006, 2007, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_HTML_CANVAS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_HTML_CANVAS_H_

#include "third_party/blink/renderer/core/layout/layout_replaced.h"

namespace blink {

class HTMLCanvasElement;

class CORE_EXPORT LayoutHTMLCanvas final : public LayoutReplaced {
 public:
  explicit LayoutHTMLCanvas(HTMLCanvasElement*);

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectCanvas || LayoutReplaced::IsOfType(type);
  }
  PaintLayerType LayerTypeRequired() const override;

  void InvalidatePaint(const PaintInvalidatorContext&) const final;

  void CanvasSizeChanged();

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  const char* GetName() const override { return "LayoutHTMLCanvas"; }

  void WillBeDestroyed() override;

 private:
  void PaintReplaced(const PaintInfo&,
                     const PhysicalOffset& paint_offset) const override;
  void IntrinsicSizeChanged() override { CanvasSizeChanged(); }

  bool CanHaveAdditionalCompositingReasons() const override { return true; }
  CompositingReasons AdditionalCompositingReasons() const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutHTMLCanvas, IsCanvas());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_HTML_CANVAS_H_
