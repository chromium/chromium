/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "third_party/blink/renderer/core/layout/layout_br.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"

namespace blink {

static String NewlineString() {
  DEFINE_STATIC_LOCAL(const String, string, ("\n"));
  return string;
}

LayoutBR::LayoutBR(HTMLBRElement& node) : LayoutText(&node, NewlineString()) {}

LayoutBR::~LayoutBR() = default;

int LayoutBR::CaretMinOffset() const {
  NOT_DESTROYED();
  return 0;
}

int LayoutBR::CaretMaxOffset() const {
  NOT_DESTROYED();
  return 1;
}

unsigned LayoutBR::NonCollapsedCaretMaxOffset() const {
  NOT_DESTROYED();
  return 1;
}

PositionWithAffinity LayoutBR::PositionForPoint(const PhysicalOffset&) const {
  NOT_DESTROYED();
  // NG codepath requires |kPrePaintClean|.
  // |SelectionModifier| calls this only in legacy codepath.
  DCHECK(!IsInLayoutNGInlineFormattingContext() ||
         GetDocument().Lifecycle().GetState() >=
             DocumentLifecycle::kPrePaintClean);
  return PositionBeforeThis();
}

Position LayoutBR::PositionForCaretOffset(unsigned offset) const {
  NOT_DESTROYED();
  DCHECK_LE(offset, 1u);
  DCHECK(GetNode());
  return offset ? Position::AfterNode(*GetNode())
                : Position::BeforeNode(*GetNode());
}

std::optional<unsigned> LayoutBR::CaretOffsetForPosition(
    const Position& position) const {
  NOT_DESTROYED();
  if (position.IsNull() || position.AnchorNode() != GetNode())
    return std::nullopt;
  DCHECK(position.IsBeforeAnchor() || position.IsAfterAnchor()) << position;
  return position.IsBeforeAnchor() ? 0 : 1;
}

}  // namespace blink
