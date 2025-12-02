// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_NAVIGATION_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_NAVIGATION_QUERY_H_

#include "third_party/blink/renderer/core/css/conditional_exp_node.h"
#include "third_party/blink/renderer/core/route_matching/navigation_preposition.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Document;
class Route;
class URLPattern;

// <navigation-location>
//
// https://drafts.csswg.org/css-navigation-1/#typedef-navigation-location
class NavigationLocation : public GarbageCollected<NavigationLocation> {
 public:
  explicit NavigationLocation(const AtomicString& navigation_name)
      : string_(navigation_name) {}
  NavigationLocation(URLPattern* url_pattern,
                     const AtomicString& original_url_pattern_string)
      : url_pattern_(url_pattern), string_(original_url_pattern_string) {}

  void Trace(Visitor*) const;

  URLPattern* GetURLPattern() const { return url_pattern_; }

  const AtomicString& OriginalURLPatternString() const {
    if (url_pattern_) {
      return string_;
    }
    return g_null_atom;
  }

  const AtomicString& GetRouteName() const {
    if (url_pattern_) {
      return g_null_atom;
    }
    return string_;
  }

  // Look for a `Route` entry in the route map. Additionally, if this
  // <route-location> is a URLPattern, an entry will be inserted if it's
  // missing.
  const Route* FindOrCreateRoute(Document&) const;

  void SerializeTo(StringBuilder&) const;

 private:
  Member<URLPattern> url_pattern_;

  // Route name, or, if `url_pattern_` is set, the original URLPattern
  // string. The reason for storing the original string is for
  // serialization. The URLPattern API deliberately doesn't support
  // serialization.
  AtomicString string_;
};

// <navigation-test>
//
// https://drafts.csswg.org/css-navigation-1/#typedef-navigation-test
class NavigationTestExpression
    : public GarbageCollected<NavigationTestExpression> {
 public:
  NavigationTestExpression(NavigationLocation& location,
                           NavigationPreposition preposition)
      : navigation_location_(&location), preposition_(preposition) {}

  void Trace(Visitor* v) const { v->Trace(navigation_location_); }

  NavigationLocation& GetLocation() const { return *navigation_location_; }
  NavigationPreposition GetPreposition() const { return preposition_; }

  bool Matches(Document&) const;

  void SerializeTo(StringBuilder&) const;

 private:
  Member<NavigationLocation> navigation_location_;
  NavigationPreposition preposition_;
};

class NavigationExpNode : public ConditionalExpNode {
 public:
  explicit NavigationExpNode(NavigationTestExpression& test)
      : navigation_test_(&test) {}

  void Trace(Visitor*) const override;

  const NavigationTestExpression& NavigationTest() const {
    return *navigation_test_;
  }

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;

 private:
  Member<NavigationTestExpression> navigation_test_;
};

class NavigationQuery : public GarbageCollected<NavigationQuery> {
 public:
  explicit NavigationQuery(const ConditionalExpNode& root_exp)
      : root_exp_(&root_exp) {}

  void Trace(Visitor*) const;

  const ConditionalExpNode* GetRootExp() const { return root_exp_; }
  bool Evaluate(Document*) const;

 private:
  Member<const ConditionalExpNode> root_exp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_NAVIGATION_QUERY_H_
