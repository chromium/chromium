// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/rel_list.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/resource/link_dictionary_resource.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

RelList::RelList(Element* element)
    : DOMTokenList(*element, html_names::kRelAttr) {}

static HashSet<AtomicString>& SupportedTokensLink() {
  // There is a use counter for <link rel="monetization"> but the feature is
  // actually not implemented yet, so "monetization" is not included in the
  // list below. See https://crbug.com/1031476
  // clang-format off
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tokens,
                      ({
                          AtomicString("preload"),
                          AtomicString("preconnect"),
                          AtomicString("dns-prefetch"),
                          AtomicString("stylesheet"),
                          AtomicString("icon"),
                          AtomicString("alternate"),
                          AtomicString("prefetch"),
                          AtomicString("prerender"),
                          AtomicString("next"),
                          AtomicString("manifest"),
                          AtomicString("apple-touch-icon"),
                          AtomicString("apple-touch-icon-precomposed"),
                          AtomicString("canonical"),
                          AtomicString("modulepreload"),
                          AtomicString("allowed-alt-sxg"),
                      }));
  // clang-format on

  return tokens;
}

static HashSet<AtomicString>& SupportedTokensAnchorAndAreaAndForm() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tokens,
                      ({
                          AtomicString("noreferrer"),
                          AtomicString("noopener"),
                          AtomicString("opener"),
                      }));

  return tokens;
}

bool RelList::ValidateTokenValue(const AtomicString& token_value,
                                 ExceptionState& state) const {
  //  https://html.spec.whatwg.org/C/#linkTypes
  ExecutionContext* execution_context =
      GetElement().GetDocument().GetExecutionContext();
  if (GetElement().HasTagName(html_names::kLinkTag)) {
    if (SupportedTokensLink().Contains(token_value)) {
      return true;
    } else if (CompressionDictionaryTransportFullyEnabled(execution_context) &&
               token_value == "compression-dictionary") {
      return true;
    }
  } else if ((GetElement().HasTagName(html_names::kATag) ||
              GetElement().HasTagName(html_names::kAreaTag) ||
              GetElement().HasTagName(html_names::kFormTag)) &&
             SupportedTokensAnchorAndAreaAndForm().Contains(token_value)) {
    return true;
  }
  return false;
}

}  // namespace blink
