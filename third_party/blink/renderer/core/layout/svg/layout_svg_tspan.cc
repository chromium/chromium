/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_tspan.h"

#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"

namespace blink {

LayoutSVGTSpan::LayoutSVGTSpan(Element* element) : LayoutSVGInline(element) {}

bool LayoutSVGTSpan::IsChildAllowed(LayoutObject* child,
                                    const ComputedStyle&) const {
  NOT_DESTROYED();
  // Always allow text (except empty textnodes and <br>).
  if (child->IsText())
    return SVGLayoutSupport::IsLayoutableTextNode(child);

  return child->IsSVGInline() && !child->IsSVGTextPath();
}

}  // namespace blink
