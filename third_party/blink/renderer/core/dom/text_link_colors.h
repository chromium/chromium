/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TEXT_LINK_COLORS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TEXT_LINK_COLORS_H_

#include "third_party/blink/public/common/css/color_scheme.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CSSValue;

class TextLinkColors {
  DISALLOW_NEW();

 public:
  TextLinkColors();
  TextLinkColors(const TextLinkColors&) = delete;
  TextLinkColors& operator=(const TextLinkColors&) = delete;

  void SetTextColor(const Color& color) { text_color_ = color; }
  Color TextColor() const { return text_color_; }

  const Color& LinkColor() const { return link_color_; }
  const Color& VisitedLinkColor() const { return visited_link_color_; }
  const Color& ActiveLinkColor() const { return active_link_color_; }
  void SetLinkColor(const Color& color) { link_color_ = color; }
  void SetVisitedLinkColor(const Color& color) { visited_link_color_ = color; }
  void SetActiveLinkColor(const Color& color) { active_link_color_ = color; }
  void ResetLinkColor();
  void ResetVisitedLinkColor();
  void ResetActiveLinkColor();
  Color ColorFromCSSValue(const CSSValue&,
                          Color current_color,
                          ColorScheme color_scheme,
                          bool for_visited_link = false) const;

 private:
  Color text_color_;
  Color link_color_;
  Color visited_link_color_;
  Color active_link_color_;
};

}  // namespace blink

#endif
