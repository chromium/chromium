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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_LAYOUT_ENGINE_SPACING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_LAYOUT_ENGINE_SPACING_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

class Font;

// Helper class used by SVGTextLayoutEngine to handle 'letter-spacing' and
// 'word-spacing'.
class SVGTextLayoutEngineSpacing {
  STACK_ALLOCATED();

 public:
  SVGTextLayoutEngineSpacing(const Font&, float effective_zoom);

  float CalculateCSSSpacing(UChar current_character);

 private:
  const Font& font_;
  UChar last_character_;
  float effective_zoom_;
  DISALLOW_COPY_AND_ASSIGN(SVGTextLayoutEngineSpacing);
};

}  // namespace blink

#endif
