// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/inner_html_builder.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"

namespace blink {

// static
String InnerHtmlBuilder::Build(LocalFrame& frame) {
  auto* body = frame.GetDocument()->body();
  if (!body) {
    return String();
  }
  InnerHtmlBuilder builder(*frame.GetDocument());
  return builder.Build(*body);
}

InnerHtmlBuilder::InnerHtmlBuilder(Document& d)
    : MarkupAccumulator(kDoNotResolveURLs,
                        IsA<HTMLDocument>(d) ? SerializationType::kHTML
                                             : SerializationType::kXML,
                        ShadowRootInclusion()) {}

String InnerHtmlBuilder::Build(HTMLElement& body) {
  return SerializeNodes<EditingStrategy>(body, kIncludeNode);
}

MarkupAccumulator::EmitChoice InnerHtmlBuilder::WillProcessElement(
    const Element& e) {
  if (e.IsScriptElement()) {
    return EmitChoice::kIgnore;
  }
  return MarkupAccumulator::WillProcessElement(e);
}

}  // namespace blink
