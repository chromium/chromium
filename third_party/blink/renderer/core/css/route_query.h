// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ROUTE_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ROUTE_QUERY_H_

#include "third_party/blink/renderer/core/css/conditional_exp_node.h"
#include "third_party/blink/renderer/core/route_matching/route_preposition.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Document;
class URLPattern;

// <route-test>
//
// https://wicg.github.io/declarative-partial-updates/css-route-matching/#at-route
class RouteTest : public GarbageCollected<RouteTest> {
 public:
  RouteTest(const AtomicString& route_name, RoutePreposition);
  RouteTest(URLPattern*,
            const AtomicString& original_url_pattern_string,
            RoutePreposition);

  void Trace(Visitor*) const;

  RoutePreposition GetPreposition() const { return preposition_; }

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

  bool Matches(Document&) const;

 private:
  Member<URLPattern> url_pattern_;

  // Route name, or, if `url_pattern_` is set, the original URLPattern
  // string. The reason for storing the original string is for
  // serialization. The URLPattern API deliberately doesn't support
  // serialization.
  AtomicString string_;

  RoutePreposition preposition_;
};

class RouteQueryExpNode : public ConditionalExpNode {
 public:
  explicit RouteQueryExpNode(RouteTest& route_test)
      : route_test_(&route_test) {}

  void Trace(Visitor*) const override;

  const RouteTest& GetRouteTest() const { return *route_test_; }

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;

 private:
  Member<RouteTest> route_test_;
};

class RouteQuery : public GarbageCollected<RouteQuery> {
 public:
  explicit RouteQuery(const ConditionalExpNode& root_exp)
      : root_exp_(&root_exp) {}

  void Trace(Visitor*) const;

  const ConditionalExpNode* GetRootExp() const { return root_exp_; }
  bool Evaluate(Document*) const;

 private:
  Member<const ConditionalExpNode> root_exp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ROUTE_QUERY_H_
