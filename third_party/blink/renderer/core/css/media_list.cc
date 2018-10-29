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
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
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

MediaQuerySet::MediaQuerySet(const MediaQuerySet& o)
    : queries_(o.queries_.size()) {
  for (unsigned i = 0; i < queries_.size(); ++i)
    queries_[i] = o.queries_[i]->Copy();
}

scoped_refptr<MediaQuerySet> MediaQuerySet::Create(const String& media_string) {
  if (media_string.IsEmpty())
    return MediaQuerySet::Create();

  return MediaQueryParser::ParseMediaQuerySet(media_string);
}

bool MediaQuerySet::Set(const String& media_string) {
  scoped_refptr<MediaQuerySet> result = Create(media_string);
  // TODO(keishi) Changed DCHECK to CHECK for crbug.com/699269 diagnosis
  for (const auto& query : result->queries_) {
    CHECK(query);
  }
  queries_.swap(result->queries_);
  return true;
}

bool MediaQuerySet::Add(const String& query_string) {
  // To "parse a media query" for a given string means to follow "the parse
  // a media query list" steps and return "null" if more than one media query
  // is returned, or else the returned media query.
  scoped_refptr<MediaQuerySet> result = Create(query_string);

  // Only continue if exactly one media query is found, as described above.
  if (result->queries_.size() != 1)
    return false;

  std::unique_ptr<MediaQuery> new_query = std::move(result->queries_[0]);
  // TODO(keishi) Changed DCHECK to CHECK for crbug.com/699269 diagnosis
  CHECK(new_query);

  // If comparing with any of the media queries in the collection of media
  // queries returns true terminate these steps.
  for (wtf_size_t i = 0; i < queries_.size(); ++i) {
    MediaQuery& query = *queries_[i];
    if (query == *new_query)
      return false;
  }

  queries_.push_back(std::move(new_query));
  return true;
}

bool MediaQuerySet::Remove(const String& query_string_to_remove) {
  // To "parse a media query" for a given string means to follow "the parse
  // a media query list" steps and return "null" if more than one media query
  // is returned, or else the returned media query.
  scoped_refptr<MediaQuerySet> result = Create(query_string_to_remove);

  // Only continue if exactly one media query is found, as described above.
  if (result->queries_.size() != 1)
    return true;

  std::unique_ptr<MediaQuery> new_query = std::move(result->queries_[0]);
  // TODO(keishi) Changed DCHECK to CHECK for crbug.com/699269 diagnosis
  CHECK(new_query);

  // Remove any media query from the collection of media queries for which
  // comparing with the media query returns true.
  bool found = false;
  for (wtf_size_t i = 0; i < queries_.size(); ++i) {
    MediaQuery& query = *queries_[i];
    if (query == *new_query) {
      queries_.EraseAt(i);
      --i;
      found = true;
    }
  }

  return found;
}

void MediaQuerySet::AddMediaQuery(std::unique_ptr<MediaQuery> media_query) {
  // TODO(keishi) Changed DCHECK to CHECK for crbug.com/699269 diagnosis
  CHECK(media_query);
  queries_.push_back(std::move(media_query));
}

String MediaQuerySet::MediaText() const {
  StringBuilder text;

  bool first = true;
  for (wtf_size_t i = 0; i < queries_.size(); ++i) {
    if (!first)
      text.Append(", ");
    else
      first = false;
    text.Append(queries_[i]->CssText());
  }
  return text.ToString();
}

MediaList::MediaList(scoped_refptr<MediaQuerySet> media_queries,
                     CSSStyleSheet* parent_sheet)
    : media_queries_(media_queries),
      parent_style_sheet_(parent_sheet),
      parent_rule_(nullptr) {}

MediaList::MediaList(scoped_refptr<MediaQuerySet> media_queries,
                     CSSRule* parent_rule)
    : media_queries_(media_queries),
      parent_style_sheet_(nullptr),
      parent_rule_(parent_rule) {}

void MediaList::setMediaText(const String& value) {
  CSSStyleSheet::RuleMutationScope mutation_scope(parent_rule_);

  media_queries_->Set(value);

  if (parent_style_sheet_)
    parent_style_sheet_->DidMutate();
}

String MediaList::item(unsigned index) const {
  const Vector<std::unique_ptr<MediaQuery>>& queries =
      media_queries_->QueryVector();
  if (index < queries.size())
    return queries[index]->CssText();
  return String();
}

void MediaList::deleteMedium(const String& medium,
                             ExceptionState& exception_state) {
  CSSStyleSheet::RuleMutationScope mutation_scope(parent_rule_);

  bool success = media_queries_->Remove(medium);
  if (!success) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "Failed to delete '" + medium + "'.");
    return;
  }
  if (parent_style_sheet_)
    parent_style_sheet_->DidMutate();
}

void MediaList::appendMedium(const String& medium) {
  CSSStyleSheet::RuleMutationScope mutation_scope(parent_rule_);

  bool added = media_queries_->Add(medium);
  if (!added)
    return;

  if (parent_style_sheet_)
    parent_style_sheet_->DidMutate();
}

void MediaList::Reattach(scoped_refptr<MediaQuerySet> media_queries) {
  // TODO(keishi) Changed DCHECK to CHECK for crbug.com/699269 diagnosis
  CHECK(media_queries);
  for (const auto& query : media_queries->QueryVector()) {
    CHECK(query);
  }
  media_queries_ = media_queries;
}

void MediaList::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_style_sheet_);
  visitor->Trace(parent_rule_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
