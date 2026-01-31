// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_LINK_CONDITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_LINK_CONDITION_H_

#include "third_party/blink/renderer/core/css/conditional_exp_node.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Element;
class NavigationLocation;
class StringBuilder;

// A link condition, i.e. a :link-to() selector. A :link-to() selector takes a
// route name (or url-pattern() directly), and an optional set of url-pattern()
// parameter match criteria.
//
// See https://drafts.csswg.org/css-navigation-1/#link-navigation-pseudo-classes
class LinkCondition : public GarbageCollected<LinkCondition> {
 public:
  LinkCondition(NavigationLocation*,
                const ConditionalExpNode* navigation_param_root_exp);

  void Trace(Visitor*) const;

  bool Evaluate(const Element&) const;
  void SerializeTo(StringBuilder&) const;

  const NavigationLocation& GetNavigationLocation() const {
    return *navigation_location_;
  }

 private:
  Member<NavigationLocation> navigation_location_;
  Member<const ConditionalExpNode> navigation_param_root_exp_;
};

class NavigationParamExpNode : public ConditionalExpNode {
 public:
  explicit NavigationParamExpNode(const AtomicString& param) : param_(param) {}
  NavigationParamExpNode(const AtomicString& param, const AtomicString& value)
      : param_(param), value_(value) {}

  const AtomicString& Param() const { return param_; }
  const AtomicString& Value() const { return value_; }

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const final;
  void SerializeTo(StringBuilder&) const final;

 private:
  AtomicString param_;
  AtomicString value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_LINK_CONDITION_H_
