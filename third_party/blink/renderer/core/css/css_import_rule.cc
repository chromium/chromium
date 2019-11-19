/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/core/css/css_import_rule.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSImportRule::CSSImportRule(StyleRuleImport* import_rule,
                             CSSStyleSheet* parent)
    : CSSRule(parent), import_rule_(import_rule) {}

CSSImportRule::~CSSImportRule() = default;

String CSSImportRule::href() const {
  return import_rule_->Href();
}

MediaList* CSSImportRule::media() const {
  if (!media_cssom_wrapper_) {
    media_cssom_wrapper_ = MakeGarbageCollected<MediaList>(
        import_rule_->MediaQueries(), const_cast<CSSImportRule*>(this));
  }
  return media_cssom_wrapper_.Get();
}

String CSSImportRule::cssText() const {
  StringBuilder result;
  result.Append("@import url(\"");
  result.Append(import_rule_->Href());
  result.Append("\")");

  if (import_rule_->MediaQueries()) {
    String media_text = import_rule_->MediaQueries()->MediaText();
    if (!media_text.IsEmpty()) {
      result.Append(' ');
      result.Append(media_text);
    }
  }
  result.Append(';');

  return result.ToString();
}

CSSStyleSheet* CSSImportRule::styleSheet() const {
  // TODO(yukishiino): CSSImportRule.styleSheet attribute is not nullable,
  // thus this function must not return nullptr.
  if (!import_rule_->GetStyleSheet())
    return nullptr;

  if (!style_sheet_cssom_wrapper_)
    style_sheet_cssom_wrapper_ = MakeGarbageCollected<CSSStyleSheet>(
        import_rule_->GetStyleSheet(), const_cast<CSSImportRule*>(this));
  return style_sheet_cssom_wrapper_.Get();
}

void CSSImportRule::Reattach(StyleRuleBase*) {
  // FIXME: Implement when enabling caching for stylesheets with import rules.
  NOTREACHED();
}

void CSSImportRule::Trace(blink::Visitor* visitor) {
  visitor->Trace(import_rule_);
  visitor->Trace(media_cssom_wrapper_);
  visitor->Trace(style_sheet_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
