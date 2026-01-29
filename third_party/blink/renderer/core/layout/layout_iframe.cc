/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_iframe.h"

#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"

namespace blink {

LayoutIFrame::LayoutIFrame(HTMLFrameOwnerElement* element)
    : LayoutEmbeddedContent(element) {}

void LayoutIFrame::UpdateAfterLayout() {
  NOT_DESTROYED();
  LayoutEmbeddedContent::UpdateAfterLayout();

  const ComputedStyle& style = StyleRef();
  if (!style.IsResponsivelySized()) {
    return;
  }
  DCHECK(RuntimeEnabledFeatures::ResponsiveIframesEnabled());
  if (!GetEmbeddedContentView() && GetFrameView()) {
    GetFrameView()->AddPartToUpdate(*this);
  }
}

PhysicalNaturalSizingInfo LayoutIFrame::GetNaturalDimensions() const {
  NOT_DESTROYED();
  const ComputedStyle& style = StyleRef();
  if (style.IsResponsivelySized()) {
    DCHECK(RuntimeEnabledFeatures::ResponsiveIframesEnabled());
    if (FrameView* frame_view = ChildFrameView()) {
      // Use the natural size received from the child frame if it exists.
      if (std::optional<NaturalSizingInfo> sizing_info =
              frame_view->GetNaturalDimensions()) {
        // Use the block-size only.
        // TODO(crbug.com/418397278): `writing-mode` isn't supported yet.
        sizing_info->has_width = false;

        // Scale based on our zoom as the embedded document doesn't have that
        // info.
        sizing_info->size.Scale(style.EffectiveZoom());
        return PhysicalNaturalSizingInfo::FromSizingInfo(*sizing_info);
      }
    }
  }

  return LayoutEmbeddedContent::GetNaturalDimensions();
}

}  // namespace blink
