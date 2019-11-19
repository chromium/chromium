/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2012 Apple Computer, Inc.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
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

#include "third_party/blink/renderer/core/css/css_media_rule.h"

#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSMediaRule::CSSMediaRule(StyleRuleMedia* media_rule, CSSStyleSheet* parent)
    : CSSConditionRule(media_rule, parent) {}

CSSMediaRule::~CSSMediaRule() = default;

scoped_refptr<MediaQuerySet> CSSMediaRule::MediaQueries() const {
  return To<StyleRuleMedia>(group_rule_.Get())->MediaQueries();
}

String CSSMediaRule::cssText() const {
  StringBuilder result;
  result.Append("@media ");
  if (MediaQueries()) {
    result.Append(MediaQueries()->MediaText());
    result.Append(' ');
  }
  result.Append("{\n");
  AppendCSSTextForItems(result);
  result.Append('}');
  return result.ToString();
}

String CSSMediaRule::conditionText() const {
  if (!MediaQueries())
    return String();
  return MediaQueries()->MediaText();
}

MediaList* CSSMediaRule::media() const {
  if (!MediaQueries())
    return nullptr;
  if (!media_cssom_wrapper_) {
    media_cssom_wrapper_ = MakeGarbageCollected<MediaList>(
        MediaQueries(), const_cast<CSSMediaRule*>(this));
  }
  return media_cssom_wrapper_.Get();
}

void CSSMediaRule::Reattach(StyleRuleBase* rule) {
  CSSConditionRule::Reattach(rule);
  if (media_cssom_wrapper_ && MediaQueries())
    media_cssom_wrapper_->Reattach(MediaQueries());
}

void CSSMediaRule::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_cssom_wrapper_);
  CSSConditionRule::Trace(visitor);
}
}  // namespace blink
