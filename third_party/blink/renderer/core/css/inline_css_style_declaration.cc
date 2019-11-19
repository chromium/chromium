/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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

#include "third_party/blink/renderer/core/css/inline_css_style_declaration.h"

#include "third_party/blink/renderer/core/css/style_attribute_mutation_scope.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

MutableCSSPropertyValueSet& InlineCSSStyleDeclaration::PropertySet() const {
  return parent_element_->EnsureMutableInlineStyle();
}

void InlineCSSStyleDeclaration::DidMutate(MutationType type) {
  if (type == kNoChanges)
    return;

  if (!parent_element_)
    return;

  parent_element_->ClearMutableInlineStyleIfEmpty();
  parent_element_->SetNeedsStyleRecalc(
      kLocalStyleChange, StyleChangeReasonForTracing::Create(
                             style_change_reason::kInlineCSSStyleMutated));
  parent_element_->InvalidateStyleAttribute();
  StyleAttributeMutationScope(this).DidInvalidateStyleAttr();
}

CSSStyleSheet* InlineCSSStyleDeclaration::ParentStyleSheet() const {
  return parent_element_ ? &parent_element_->GetDocument().ElementSheet()
                         : nullptr;
}

void InlineCSSStyleDeclaration::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_element_);
  AbstractPropertySetCSSStyleDeclaration::Trace(visitor);
}

}  // namespace blink
