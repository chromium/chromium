// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_route.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_url_pattern_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/route_matching/route_map.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"

namespace blink {

StyleRuleRoute::StyleRuleRoute(const String& name, CSSPropertyValueSet* values)
    : StyleRuleBase(kRoute),
      name_(name),
      pattern_(DynamicTo<CSSURLPatternValue>(
          values->GetPropertyCSSValue(CSSPropertyID::kPattern))),
      protocol_(DynamicTo<CSSStringValue>(
          values->GetPropertyCSSValue(CSSPropertyID::kProtocol))),
      hostname_(DynamicTo<CSSStringValue>(
          values->GetPropertyCSSValue(CSSPropertyID::kHostname))),
      port_(DynamicTo<CSSStringValue>(
          values->GetPropertyCSSValue(CSSPropertyID::kPort))),
      pathname_(DynamicTo<CSSStringValue>(
          values->GetPropertyCSSValue(CSSPropertyID::kPathname))),
      search_(DynamicTo<CSSStringValue>(
          values->GetPropertyCSSValue(CSSPropertyID::kSearch))),
      hash_(DynamicTo<CSSStringValue>(
          values->GetPropertyCSSValue(CSSPropertyID::kHash))),
      base_url_(DynamicTo<CSSStringValue>(
          values->GetPropertyCSSValue(CSSPropertyID::kBaseUrl))) {
  DCHECK(name.StartsWith("--"));
}

StyleRuleRoute::StyleRuleRoute(const StyleRuleRoute& other)
    : StyleRuleBase(other), name_(other.name_) {}

void StyleRuleRoute::TraceAfterDispatch(Visitor* v) const {
  v->Trace(pattern_);
  v->Trace(protocol_);
  v->Trace(hostname_);
  v->Trace(port_);
  v->Trace(pathname_);
  v->Trace(search_);
  v->Trace(hash_);
  v->Trace(base_url_);
  StyleRuleBase::TraceAfterDispatch(v);
}

void StyleRuleRoute::CreateRouteIfNeeded(Document* document) const {
  if (!document) {
    return;
  }
  auto& route_map = RouteMap::Ensure(*document);
  URLPattern* url_pattern;
  if (pattern_) {
    const AtomicString& str = pattern_->UrlString();
    auto* url_pattern_input = MakeGarbageCollected<V8URLPatternInput>(str);
    url_pattern = URLPattern::Create(
        document->GetExecutionContext()->GetIsolate(), url_pattern_input,
        document->Url(), IGNORE_EXCEPTION);
  } else {
    URLPatternInit* init = URLPatternInit::Create();
    if (protocol_) {
      init->setProtocol(protocol_->Value());
    }
    if (hostname_) {
      init->setHostname(hostname_->Value());
    }
    if (port_) {
      init->setPort(port_->Value());
    }
    if (pathname_) {
      init->setPathname(pathname_->Value());
    }
    if (search_) {
      init->setSearch(search_->Value());
    }
    if (hash_) {
      init->setHash(hash_->Value());
    }
    if (base_url_) {
      init->setBaseURL(base_url_->Value());
    } else {
      init->setBaseURL(document->BaseURL());
    }
    auto* url_pattern_input = MakeGarbageCollected<V8URLPatternInput>(init);
    url_pattern =
        URLPattern::Create(document->GetExecutionContext()->GetIsolate(),
                           url_pattern_input, IGNORE_EXCEPTION);
  }

  if (!url_pattern) {
    return;
  }
  route_map.AddRouteFromRule(name_, url_pattern);
}

}  // namespace blink
