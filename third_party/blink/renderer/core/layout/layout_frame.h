/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_FRAME_H_

#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_frame_set.h"

namespace blink {

class HTMLFrameElement;

class LayoutFrame final : public LayoutEmbeddedContent {
 public:
  explicit LayoutFrame(HTMLFrameElement*);

  FrameEdgeInfo EdgeInfo() const;

  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) override;

  const char* GetName() const override { return "LayoutFrame"; }

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectFrame || LayoutEmbeddedContent::IsOfType(type);
  }

  void UpdateFromElement() override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutFrame, IsFrame());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_FRAME_H_
