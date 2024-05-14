/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/style_sheet_candidate.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"

namespace blink {

AtomicString StyleSheetCandidate::Title() const {
  return IsElement()
             ? To<Element>(GetNode()).FastGetAttribute(html_names::kTitleAttr)
             : g_null_atom;
}

bool StyleSheetCandidate::IsXSL() const {
  return !IsA<HTMLDocument>(GetNode().GetDocument()) && type_ == kPi &&
         To<ProcessingInstruction>(GetNode()).IsXSL();
}

bool StyleSheetCandidate::IsCSSStyle() const {
  return type_ == kHTMLStyle || type_ == kSVGStyle;
}

bool StyleSheetCandidate::IsEnabledViaScript() const {
  auto* html_link_element = DynamicTo<HTMLLinkElement>(GetNode());
  return html_link_element && html_link_element->IsEnabledViaScript();
}

bool StyleSheetCandidate::IsEnabledAndLoading() const {
  auto* html_link_element = DynamicTo<HTMLLinkElement>(GetNode());
  return html_link_element && !html_link_element->IsDisabled() &&
         html_link_element->StyleSheetIsLoading();
}

bool StyleSheetCandidate::CanBeActivated(
    const String& current_preferrable_name) const {
  StyleSheet* sheet = Sheet();
  auto* css_style_sheet = DynamicTo<CSSStyleSheet>(sheet);
  if (!css_style_sheet || sheet->disabled()) {
    return false;
  }
  return css_style_sheet->CanBeActivated(current_preferrable_name);
}

StyleSheetCandidate::Type StyleSheetCandidate::TypeOf(Node& node) {
  if (node.getNodeType() == Node::kProcessingInstructionNode) {
    return kPi;
  }

  if (node.IsHTMLElement()) {
    if (IsA<HTMLLinkElement>(node)) {
      return kHTMLLink;
    }
    if (IsA<HTMLStyleElement>(node)) {
      return kHTMLStyle;
    }

    NOTREACHED_IN_MIGRATION();
    return kInvalid;
  }

  if (IsA<SVGStyleElement>(node)) {
    return kSVGStyle;
  }

  NOTREACHED_IN_MIGRATION();
  return kInvalid;
}

StyleSheet* StyleSheetCandidate::Sheet() const {
  switch (type_) {
    case kHTMLLink:
      return To<HTMLLinkElement>(GetNode()).sheet();
    case kHTMLStyle:
      return To<HTMLStyleElement>(GetNode()).sheet();
    case kSVGStyle:
      return To<SVGStyleElement>(GetNode()).sheet();
    case kPi:
      return To<ProcessingInstruction>(GetNode()).sheet();
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

}  // namespace blink
