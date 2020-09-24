/*
 * Copyright (C) 2011 Robert Hogan <robert@roberthogan.net>
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
 */

#include "third_party/blink/renderer/core/layout/layout_table_caption.h"

#include "third_party/blink/renderer/core/layout/layout_table.h"

namespace blink {

LayoutTableCaption::LayoutTableCaption(Element* element)
    : LayoutBlockFlow(element) {}

LayoutTableCaption::~LayoutTableCaption() = default;

LayoutUnit LayoutTableCaption::ContainingBlockLogicalWidthForContent() const {
  LayoutBlock* cb = ContainingBlock();
  return cb->LogicalWidth();
}

void LayoutTableCaption::InsertedIntoTree() {
  LayoutBlockFlow::InsertedIntoTree();

  Table()->AddCaption(this);
}

void LayoutTableCaption::WillBeRemovedFromTree() {
  LayoutBlockFlow::WillBeRemovedFromTree();

  Table()->RemoveCaption(this);
}

LayoutTable* LayoutTableCaption::Table() const {
  return To<LayoutTable>(Parent());
}

}  // namespace blink
