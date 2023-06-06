/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_OBSERVER_H_

#include "third_party/blink/renderer/core/css/css_property_source_data.h"
#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"

namespace blink {

// This is only for the inspector and shouldn't be used elsewhere.
class CSSParserObserver {
 public:
  virtual void StartRuleHeader(StyleRule::RuleType, unsigned offset) = 0;
  virtual void EndRuleHeader(unsigned offset) = 0;
  virtual void ObserveSelector(unsigned start_offset, unsigned end_offset) = 0;
  virtual void StartRuleBody(unsigned offset) = 0;
  virtual void EndRuleBody(unsigned offset) = 0;
  virtual void ObserveProperty(unsigned start_offset,
                               unsigned end_offset,
                               bool is_important,
                               bool is_parsed) = 0;
  virtual void ObserveComment(unsigned start_offset, unsigned end_offset) = 0;
  virtual void ObserveErroneousAtRule(unsigned start_offset,
                                      CSSAtRuleID id) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_OBSERVER_H_
