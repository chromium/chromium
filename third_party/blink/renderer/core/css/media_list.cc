/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2010, 2012 Apple Inc. All rights reserved.
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
#include "third_party/blink/renderer/core/css/media_list.h"

#include <memory>
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/media_query_set_owner.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

/* MediaList is used to store 3 types of media related entities which mean the
 * same:
 *
 * Media Queries, Media Types and Media Descriptors.
 *
 * Media queries, as described in the Media Queries Level 3 specification, build
 * on the mechanism outlined in HTML4. The syntax of media queries fit into the
 * media type syntax reserved in HTML4. The media attribute of HTML4 also exists
 * in XHTML and generic XML. The same syntax can also be used inside the @media
 * and @import rules of CSS.
 *
 * However, the parsing rules for media queries are incompatible with those of
 * HTML4 and are consistent with those of media queries used in CSS.
 *
 * HTML5 (at the moment of writing still work in progress) references the Media
 * Queries specification directly and thus updates the rules for HTML.
 *
 * CSS 2.1 Spec (http://www.w3.org/TR/CSS21/media.html)
 * CSS 3 Media Queries Spec (http://www.w3.org/TR/css3-mediaqueries/)
 */

MediaQuerySet::MediaQuerySet() = default;

MediaQuerySet::MediaQuerySet(const MediaQuerySet&) = default;

MediaQuerySet::MediaQuerySet(HeapVector<Member<const MediaQuery>> queries)
    : queries_(std::move(queries)) {}

MediaQuerySet* MediaQuerySet::Create(
    const String& media_string,
    const ExecutionContext* execution_context) {
  if (media_string.empty()) {
    return MediaQuerySet::Create();
  }

  return MediaQueryParser::ParseMediaQuerySet(media_string, execution_context);
}

void MediaQuerySet::Trace(Visitor* visitor) const {
  visitor->Trace(queries_);
}

const MediaQuerySet* MediaQuerySet::CopyAndAdd(
    const String& query_string,
    const ExecutionContext* execution_context) const {
  // To "parse a media query" for a given string means to follow "the parse
  // a media query list" steps and return "null" if more than one media query
  // is returned, or else the returned media query.
  MediaQuerySet* result = Create(query_string, execution_context);

  // Only continue if exactly one media query is found, as described above.
  if (result->queries_.size() != 1) {
    return nullptr;
  }

  const MediaQuery* new_query = result->queries_[0].Get();
  DCHECK(new_query);

  // If comparing with any of the media queries in the collection of media
  // queries returns true terminate these steps.
  for (wtf_size_t i = 0; i < queries_.size(); ++i) {
    const MediaQuery& query = *queries_[i];
    if (query == *new_query) {
      return nullptr;
    }
  }

  HeapVector<Member<const MediaQuery>> new_queries = queries_;
  new_queries.push_back(new_query);

  return MakeGarbageCollected<MediaQuerySet>(std::move(new_queries));
}

const MediaQuerySet* MediaQuerySet::CopyAndRemove(
    const String& query_string_to_remove,
    const ExecutionContext* execution_context) const {
  // To "parse a media query" for a given string means to follow "the parse
  // a media query list" steps and return "null" if more than one media query
  // is returned, or else the returned media query.
  MediaQuerySet* result = Create(query_string_to_remove, execution_context);

  // Only continue if exactly one media query is found, as described above.
  if (result->queries_.size() != 1) {
    return this;
  }

  const MediaQuery* new_query = result->queries_[0];
  DCHECK(new_query);

  HeapVector<Member<const MediaQuery>> new_queries = queries_;

  // Remove any media query from the collection of media queries for which
  // comparing with the media query returns true.
  bool found = false;
  for (wtf_size_t i = 0; i < new_queries.size(); ++i) {
    const MediaQuery& query = *new_queries[i];
    if (query == *new_query) {
      new_queries.EraseAt(i);
      --i;
      found = true;
    }
  }

  if (!found) {
    return nullptr;
  }

  return MakeGarbageCollected<MediaQuerySet>(std::move(new_queries));
}

String MediaQuerySet::MediaText() const {
  StringBuilder text;

  bool first = true;
  for (wtf_size_t i = 0; i < queries_.size(); ++i) {
    if (!first) {
      text.Append(", ");
    } else {
      first = false;
    }
    text.Append(queries_[i]->CssText());
  }
  return text.ReleaseString();
}

MediaList::MediaList(CSSStyleSheet* parent_sheet)
    : parent_style_sheet_(parent_sheet), parent_rule_(nullptr) {
  DCHECK(Owner());
}

MediaList::MediaList(CSSRule* parent_rule)
    : parent_style_sheet_(nullptr), parent_rule_(parent_rule) {
  DCHECK(Owner());
}

String MediaList::mediaText(ExecutionContext* execution_context) const {
  return MediaTextInternal();
}

void MediaList::setMediaText(const ExecutionContext* execution_context,
                             const String& value) {
  CSSStyleSheet::RuleMutationScope mutation_scope(parent_rule_);

  Owner()->SetMediaQueries(MediaQuerySet::Create(value, execution_context));

  NotifyMutation();
}

String MediaList::item(unsigned index) const {
  const HeapVector<Member<const MediaQuery>>& queries =
      Queries()->QueryVector();
  if (index < queries.size()) {
    return queries[index]->CssText();
  }
  return String();
}

void MediaList::deleteMedium(const ExecutionContext* execution_context,
                             const String& medium,
                             ExceptionState& exception_state) {
  CSSStyleSheet::RuleMutationScope mutation_scope(parent_rule_);

  const MediaQuerySet* new_media_queries =
      Queries()->CopyAndRemove(medium, execution_context);
  if (!new_media_queries) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "Failed to delete '" + medium + "'.");
    return;
  }
  Owner()->SetMediaQueries(new_media_queries);

  NotifyMutation();
}

void MediaList::appendMedium(const ExecutionContext* execution_context,
                             const String& medium) {
  CSSStyleSheet::RuleMutationScope mutation_scope(parent_rule_);

  const MediaQuerySet* new_media_queries =
      Queries()->CopyAndAdd(medium, execution_context);
  if (!new_media_queries) {
    return;
  }
  Owner()->SetMediaQueries(new_media_queries);

  NotifyMutation();
}

const MediaQuerySet* MediaList::Queries() const {
  return Owner()->MediaQueries();
}

void MediaList::Trace(Visitor* visitor) const {
  visitor->Trace(parent_style_sheet_);
  visitor->Trace(parent_rule_);
  ScriptWrappable::Trace(visitor);
}

MediaQuerySetOwner* MediaList::Owner() const {
  return parent_rule_ ? parent_rule_->GetMediaQuerySetOwner()
                      : parent_style_sheet_.Get();
}

void MediaList::NotifyMutation() {
  if (parent_rule_ && parent_rule_->parentStyleSheet()) {
    StyleSheetContents* parent_contents =
        parent_rule_->parentStyleSheet()->Contents();
    if (parent_rule_->GetType() == CSSRule::kStyleRule) {
      parent_contents->NotifyRuleChanged(
          static_cast<CSSStyleRule*>(parent_rule_.Get())->GetStyleRule());
    } else {
      parent_contents->NotifyDiffUnrepresentable();
    }
  }
  if (parent_style_sheet_) {
    parent_style_sheet_->DidMutate(CSSStyleSheet::Mutation::kSheet);
  }
}

}  // namespace blink
