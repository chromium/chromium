// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/link_condition.h"

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/css/navigation_query.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

LinkCondition::LinkCondition(
    NavigationLocation* location,
    const ConditionalExpNode* navigation_param_root_exp)
    : navigation_location_(location),
      navigation_param_root_exp_(navigation_param_root_exp) {}

void LinkCondition::Trace(Visitor* visitor) const {
  visitor->Trace(navigation_location_);
  visitor->Trace(navigation_param_root_exp_);
}

bool LinkCondition::Evaluate(const Element& element) const {
  const auto* anchor = DynamicTo<HTMLAnchorElement>(&element);
  if (!anchor) {
    return false;
  }

  Document& document = element.GetDocument();
  const Route* route = navigation_location_->FindOrCreateRoute(document);
  KURL href = anchor->Href();
  if (!route || !route->MatchesUrl(href)) {
    return false;
  }

  if (!navigation_param_root_exp_) {
    return true;
  }

  const RouteMap* route_map = RouteMap::Get(&document);
  CHECK(route_map);

  class Visitor : public ConditionalExpNodeVisitor {
    STACK_ALLOCATED();

   public:
    Visitor(const RouteMap& route_map, const Route& route, const KURL& href)
        : route_map_(route_map), route_(route), href_(href) {}

   private:
    bool Match(const NavigationParamExpNode& exp_node) const {
      if (!exp_node.Value()) {
        // This is navigation-param(param).
        return route_.FromOrToMatchesParamInHref(route_map_.GetFromURL(),
                                                 route_map_.GetToURL(),
                                                 exp_node.Param(), href_);
      }
      // This is param: value.
      const AtomicString& param = exp_node.Param();
      const AtomicString& value = exp_node.Value();
      return route_.HrefMatchesParam(href_, param, value);
    }

    KleeneValue EvaluateNavigationParamExpNode(
        const NavigationParamExpNode& exp_node) final {
      return Match(exp_node) ? KleeneValue::kTrue : KleeneValue::kFalse;
    }

    const RouteMap& route_map_;
    const Route& route_;
    const KURL& href_;
  };

  Visitor visitor(*route_map, *route, href);
  return navigation_param_root_exp_->Evaluate(visitor) == KleeneValue::kTrue;
}

void LinkCondition::SerializeTo(StringBuilder& builder) const {
  navigation_location_->SerializeTo(builder);
  if (navigation_param_root_exp_) {
    builder.Append(" with ");
    navigation_param_root_exp_->SerializeTo(builder);
  }
}

KleeneValue NavigationParamExpNode::Evaluate(
    ConditionalExpNodeVisitor& visitor) const {
  return visitor.EvaluateNavigationParamExpNode(*this);
}

void NavigationParamExpNode::SerializeTo(StringBuilder& builder) const {
  if (value_) {
    builder.Append("\"");
    builder.Append(param_);
    builder.Append("\": \"");
    builder.Append(value_);
    builder.Append("\"");
  } else {
    builder.Append("navigation-param(\"");
    builder.Append(param_);
    builder.Append("\")");
  }
}

}  // namespace blink
