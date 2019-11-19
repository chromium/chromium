/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2007, 2012 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_rule.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"

namespace blink {

struct SameSizeAsCSSRule : public GarbageCollected<SameSizeAsCSSRule>,
                           public ScriptWrappable {
  ~SameSizeAsCSSRule() override;
  unsigned char bitfields;
  void* pointer_union;
};

static_assert(sizeof(CSSRule) == sizeof(SameSizeAsCSSRule),
              "CSSRule should stay small");

const CSSParserContext* CSSRule::ParserContext(
    SecureContextMode secure_context_mode) const {
  CSSStyleSheet* style_sheet = parentStyleSheet();
  return style_sheet ? style_sheet->Contents()->ParserContext()
                     : StrictCSSParserContext(secure_context_mode);
}

void CSSRule::SetParentStyleSheet(CSSStyleSheet* style_sheet) {
  parent_is_rule_ = false;
  parent_style_sheet_ = style_sheet;
  MarkingVisitor::WriteBarrier(parent_style_sheet_);
}

void CSSRule::SetParentRule(CSSRule* rule) {
  parent_is_rule_ = true;
  parent_rule_ = rule;
  MarkingVisitor::WriteBarrier(parent_rule_);
}

void CSSRule::Trace(blink::Visitor* visitor) {
  // This makes the parent link strong, which is different from the
  // pre-oilpan world, where the parent link is mysteriously zeroed under
  // some circumstances.
  if (parent_is_rule_)
    visitor->Trace(parent_rule_);
  else
    visitor->Trace(parent_style_sheet_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
