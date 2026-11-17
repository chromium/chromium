// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/rel_list.h"

#include "base/no_destructor.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/resource/link_dictionary_resource.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

RelList::RelList(Element* element)
    : DOMTokenList(*element, html_names::kRelAttr) {}

RelList::RelList(Element* element, const QualifiedName& attr)
    : DOMTokenList(*element, attr) {}

static const HashSet<AtomicString>& SupportedTokensLink() {
  // "monetization" is gated in ValidateTokenValue() instead, because this set
  // is built once per process. See https://crbug.com/1031476
  static const base::NoDestructor<HashSet<AtomicString>> tokens({
      AtomicString::FromLiteral<"preload">(),
      AtomicString::FromLiteral<"preconnect">(),
      AtomicString::FromLiteral<"dns-prefetch">(),
      AtomicString::FromLiteral<"stylesheet">(),
      AtomicString::FromLiteral<"icon">(),
      AtomicString::FromLiteral<"alternate">(),
      AtomicString::FromLiteral<"prefetch">(),
      AtomicString::FromLiteral<"prerender">(),
      AtomicString::FromLiteral<"next">(),
      AtomicString::FromLiteral<"manifest">(),
      AtomicString::FromLiteral<"apple-touch-icon">(),
      AtomicString::FromLiteral<"apple-touch-icon-precomposed">(),
      AtomicString::FromLiteral<"canonical">(),
      AtomicString::FromLiteral<"modulepreload">(),
      AtomicString::FromLiteral<"allowed-alt-sxg">(),
      AtomicString::FromLiteral<"compression-dictionary">(),
  });

  return *tokens;
}

static const HashSet<AtomicString>& SupportedTokensAnchorAndAreaAndForm() {
  static const base::NoDestructor<HashSet<AtomicString>> tokens({
      AtomicString::FromLiteral<"noreferrer">(),
      AtomicString::FromLiteral<"noopener">(),
      AtomicString::FromLiteral<"opener">(),
  });

  return *tokens;
}

bool RelList::ValidateTokenValue(const AtomicString& token_value,
                                 ExceptionState& state) const {
  //  https://html.spec.whatwg.org/C/#linkTypes
  if (GetElement().HasTagName(html_names::kLinkTag) &&
      (SupportedTokensLink().Contains(token_value) ||
       (token_value == "monetization" &&
        RuntimeEnabledFeatures::MonetizationEnabled(
            GetElement().GetExecutionContext())))) {
    return true;
  } else if ((GetElement().HasTagName(html_names::kATag) ||
              GetElement().HasTagName(html_names::kAreaTag) ||
              GetElement().HasTagName(html_names::kFormTag) ||
              GetElement().HasTagName(svg_names::kATag) ||
              GetElement().HasTagName(mathml_names::kATag)) &&
             SupportedTokensAnchorAndAreaAndForm().Contains(token_value)) {
    return true;
  }
  return false;
}

}  // namespace blink
