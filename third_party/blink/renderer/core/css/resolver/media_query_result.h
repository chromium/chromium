/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MEDIA_QUERY_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MEDIA_QUERY_RESULT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

class CORE_EXPORT MediaQueryResult {
  DISALLOW_NEW();

 public:
  MediaQueryResult(const MediaQueryFeatureExpNode& feature, bool result)
      : feature_(&feature), result_(result) {}
  void Trace(Visitor* visitor) const { visitor->Trace(feature_); }

  bool operator==(const MediaQueryResult& other) const {
    return feature_ == other.feature_ && result_ == other.result_;
  }
  bool operator!=(const MediaQueryResult& other) const {
    return !(*this == other);
  }

  const MediaQueryFeatureExpNode& Feature() const { return *feature_; }

  bool Result() const { return result_; }

 private:
  Member<const MediaQueryFeatureExpNode> feature_;
  bool result_;
};

class MediaQuerySetResult {
  DISALLOW_NEW();

 public:
  MediaQuerySetResult(const MediaQuerySet& media_queries, bool result)
      : media_queries_(&media_queries), result_(result) {}
  void Trace(Visitor* visitor) const { visitor->Trace(media_queries_); }

  const MediaQuerySet& MediaQueries() const { return *media_queries_; }

  bool Result() const { return result_; }

 private:
  Member<const MediaQuerySet> media_queries_;
  bool result_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MediaQueryResult)
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MediaQuerySetResult)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MEDIA_QUERY_RESULT_H_
