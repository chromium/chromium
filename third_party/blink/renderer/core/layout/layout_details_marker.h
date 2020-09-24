/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DETAILS_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DETAILS_MARKER_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

class LayoutDetailsMarker final : public LayoutBlockFlow {
 public:
  LayoutDetailsMarker(Element*);

  enum Orientation { kUp, kDown, kLeft, kRight };

  Orientation GetOrientation() const;

  bool CreatesNewFormattingContext() const override { return true; }

  const char* GetName() const override { return "LayoutDetailsMarker"; }

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectDetailsMarker ||
           LayoutBlockFlow::IsOfType(type);
  }
  void Paint(const PaintInfo&) const override;

  bool IsOpen() const;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutDetailsMarker, IsDetailsMarker());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DETAILS_MARKER_H_
