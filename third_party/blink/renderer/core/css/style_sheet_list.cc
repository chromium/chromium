/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/style_sheet_list.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

StyleSheetList::StyleSheetList(TreeScope* tree_scope)
    : tree_scope_(tree_scope) {
  CHECK(tree_scope);
}

inline const HeapVector<Member<StyleSheet>>& StyleSheetList::StyleSheets()
    const {
  return GetDocument()->GetStyleEngine().StyleSheetsForStyleSheetList(
      *tree_scope_);
}

unsigned StyleSheetList::length() {
  if (!tree_scope_)
    return style_sheet_vector_.size();
  return StyleSheets().size();
}

StyleSheet* StyleSheetList::item(unsigned index) {
  if (!tree_scope_) {
    return index < style_sheet_vector_.size() ? style_sheet_vector_[index].Get()
                                              : nullptr;
  }
  const HeapVector<Member<StyleSheet>>& sheets = StyleSheets();
  return index < sheets.size() ? sheets[index].Get() : nullptr;
}

HTMLStyleElement* StyleSheetList::GetNamedItem(const AtomicString& name) const {
  if (!tree_scope_)
    return nullptr;

  // IE also supports retrieving a stylesheet by name, using the name/id of the
  // <style> tag (this is consistent with all the other collections) ### Bad
  // implementation because returns a single element (are IDs always unique?)
  // and doesn't look for name attribute. But unicity of stylesheet ids is good
  // practice anyway ;)
  // FIXME: We should figure out if we should change this or fix the spec.
  Element* element = tree_scope_->getElementById(name);
  return DynamicTo<HTMLStyleElement>(element);
}

CSSStyleSheet* StyleSheetList::AnonymousNamedGetter(const AtomicString& name) {
  if (GetDocument()) {
    UseCounter::Count(*GetDocument(),
                      WebFeature::kStyleSheetListAnonymousNamedGetter);
  }
  HTMLStyleElement* item = GetNamedItem(name);
  if (!item)
    return nullptr;
  CSSStyleSheet* sheet = item->sheet();
  if (sheet) {
    UseCounter::Count(*GetDocument(),
                      WebFeature::kStyleSheetListNonNullAnonymousNamedGetter);
  }
  return sheet;
}

void StyleSheetList::Trace(blink::Visitor* visitor) {
  visitor->Trace(tree_scope_);
  visitor->Trace(style_sheet_vector_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
