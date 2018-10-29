/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_VIDEO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_VIDEO_H_

#include "third_party/blink/renderer/core/layout/layout_media.h"

namespace blink {

class HTMLVideoElement;

class LayoutVideo final : public LayoutMedia {
 public:
  LayoutVideo(HTMLVideoElement*);
  ~LayoutVideo() override;

  static LayoutSize DefaultSize();

  LayoutRect ReplacedContentRect() const final;

  bool SupportsAcceleratedRendering() const;

  bool ShouldDisplayVideo() const;
  HTMLVideoElement* VideoElement() const;

  const char* GetName() const override { return "LayoutVideo"; }

  void IntrinsicSizeChanged() override;

  void UpdateAfterLayout() override;

 private:
  void UpdateFromElement() override;

  LayoutSize CalculateIntrinsicSize();
  void UpdateIntrinsicSize();

  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) override;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectVideo || LayoutMedia::IsOfType(type);
  }

  void PaintReplaced(const PaintInfo&,
                     const LayoutPoint& paint_offset) const override;

  void UpdateLayout() override;

  LayoutUnit ComputeReplacedLogicalWidth(
      ShouldComputePreferred = kComputeActual) const override;
  LayoutUnit ComputeReplacedLogicalHeight(
      LayoutUnit estimated_used_width = LayoutUnit()) const override;
  LayoutUnit MinimumReplacedHeight() const override;

  CompositingReasons AdditionalCompositingReasons() const override;

  void UpdatePlayer();

  LayoutSize cached_image_size_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutVideo, IsVideo());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_VIDEO_H_
