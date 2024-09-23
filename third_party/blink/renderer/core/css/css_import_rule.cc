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

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSImportRule::CSSImportRule(StyleRuleImport* import_rule,
                             CSSStyleSheet* parent)
    : CSSRule(parent), import_rule_(import_rule) {}

CSSImportRule::~CSSImportRule() = default;

String CSSImportRule::href() const {
  return import_rule_->Href();
}

MediaList* CSSImportRule::media() {
  if (!media_cssom_wrapper_) {
    media_cssom_wrapper_ = MakeGarbageCollected<MediaList>(this);
  }
  return media_cssom_wrapper_.Get();
}

String CSSImportRule::cssText() const {
  StringBuilder result;
  result.Append("@import ");
  result.Append(SerializeURI(import_rule_->Href()));

  if (import_rule_->IsLayered()) {
    result.Append(" layer");
    String layer_name = layerName();
    if (layer_name.length()) {
      result.Append("(");
      result.Append(layer_name);
      result.Append(")");
    }
  }

  if (String supports = import_rule_->GetSupportsString();
      supports != g_null_atom) {
    result.Append(" supports(");
    result.Append(supports);
    result.Append(")");
  }

  if (import_rule_->MediaQueries()) {
    String media_text = import_rule_->MediaQueries()->MediaText();
    if (!media_text.empty()) {
      result.Append(' ');
      result.Append(media_text);
    }
  }
  result.Append(';');

  return result.ReleaseString();
}

CSSStyleSheet* CSSImportRule::styleSheet() const {
  // TODO(yukishiino): CSSImportRule.styleSheet attribute is not nullable,
  // thus this function must not return nullptr.
  if (!import_rule_->GetStyleSheet()) {
    return nullptr;
  }

  if (!style_sheet_cssom_wrapper_) {
    style_sheet_cssom_wrapper_ = MakeGarbageCollected<CSSStyleSheet>(
        import_rule_->GetStyleSheet(), const_cast<CSSImportRule*>(this));
  }
  return style_sheet_cssom_wrapper_.Get();
}

String CSSImportRule::layerName() const {
  if (!import_rule_->IsLayered()) {
    return g_null_atom;
  }
  return import_rule_->GetLayerNameAsString();
}

String CSSImportRule::supportsText() const {
  return import_rule_->GetSupportsString();
}

void CSSImportRule::Reattach(StyleRuleBase*) {
  // FIXME: Implement when enabling caching for stylesheets with import rules.
  NOTREACHED_IN_MIGRATION();
}

const MediaQuerySet* CSSImportRule::MediaQueries() const {
  return import_rule_->MediaQueries();
}

void CSSImportRule::SetMediaQueries(const MediaQuerySet* media_queries) {
  import_rule_->SetMediaQueries(media_queries);
}

void CSSImportRule::Trace(Visitor* visitor) const {
  visitor->Trace(import_rule_);
  visitor->Trace(media_cssom_wrapper_);
  visitor->Trace(style_sheet_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
