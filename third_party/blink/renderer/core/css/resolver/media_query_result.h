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

#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"

namespace blink {

class MediaQueryResult {
  DISALLOW_NEW();

 public:
  MediaQueryResult(const MediaQueryExp& expr, bool result)
      : expression_(expr), result_(result) {}

  const MediaQueryExp& Expression() const { return expression_; }

  bool Result() const { return result_; }

 private:
  const MediaQueryExp expression_;
  bool result_;
};

}  // namespace blink

#endif
