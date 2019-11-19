/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_QUERY_H_

#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;

class SVGTextQuery {
  STACK_ALLOCATED();

 public:
  SVGTextQuery(LayoutObject* layout_object)
      : query_root_layout_object_(layout_object) {}

  unsigned NumberOfCharacters() const;
  float TextLength() const;
  float SubStringLength(unsigned start_position, unsigned length) const;
  FloatPoint StartPositionOfCharacter(unsigned position) const;
  FloatPoint EndPositionOfCharacter(unsigned position) const;
  float RotationOfCharacter(unsigned position) const;
  FloatRect ExtentOfCharacter(unsigned position) const;
  int CharacterNumberAtPosition(const FloatPoint&) const;

 private:
  LayoutObject* query_root_layout_object_;
};

}  // namespace blink

#endif
