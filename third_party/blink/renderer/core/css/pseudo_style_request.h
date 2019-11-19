/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PSEUDO_STYLE_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PSEUDO_STYLE_REQUEST_H_

#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class ComputedStyle;

class PseudoElementStyleRequest {
  DISALLOW_NEW();

 public:
  enum RequestType { kForRenderer, kForComputedStyle };

  PseudoElementStyleRequest(PseudoId pseudo_id,
                            CustomScrollbar* scrollbar = nullptr,
                            ScrollbarPart scrollbar_part = kNoPart)
      : pseudo_id(pseudo_id),
        type(kForRenderer),
        scrollbar_part(scrollbar_part),
        scrollbar(scrollbar) {}

  PseudoElementStyleRequest(PseudoId pseudo_id, RequestType request_type)
      : pseudo_id(pseudo_id),
        type(request_type),
        scrollbar_part(kNoPart),
        scrollbar(nullptr) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(scrollbar); }

  // The spec disallows inheritance for ::backdrop.
  bool AllowsInheritance(const ComputedStyle* parent_style) const {
    return parent_style && pseudo_id != kPseudoIdBackdrop;
  }

  PseudoId pseudo_id;
  RequestType type;
  ScrollbarPart scrollbar_part;
  Member<CustomScrollbar> scrollbar;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PSEUDO_STYLE_REQUEST_H_
