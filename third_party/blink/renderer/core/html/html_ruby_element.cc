// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_ruby_element.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_rt_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_ruby.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_as_block.h"

namespace blink {

HTMLRubyElement::HTMLRubyElement(Document& document)
    : HTMLElement(html_names::kRubyTag, document) {}

LayoutObject* HTMLRubyElement::CreateLayoutObject(const ComputedStyle& style) {
  if (RuntimeEnabledFeatures::CssDisplayRubyEnabled()) {
    if (style.Display() == EDisplay::kBlock &&
        RuntimeEnabledFeatures::BlockRubyConsoleMessageEnabled()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kRubyElementWithDisplayBlock);
      if (Traversal<HTMLRTElement>::FirstChild(*this)) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kRubyElementWithDisplayBlockAndRt);
      }
      GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              ConsoleMessage::Source::kRendering, ConsoleMessage::Level::kInfo,
              "A <ruby> with `display: block` doesn't render its ruby "
              "annotation correctly. A workaround compatible with older "
              "browsers and newer browsers is to specify `display: block; "
              "display: block ruby;`."),
          /* discard_duplicates */ true);
    }
    return LayoutObject::CreateObject(this, style);
  }
  if (style.Display() == EDisplay::kInline)
    return MakeGarbageCollected<LayoutRubyAsInline>(this);
  if (style.Display() == EDisplay::kBlock) {
    UseCounter::Count(GetDocument(), WebFeature::kRubyElementWithDisplayBlock);
    if (Traversal<HTMLRTElement>::FirstChild(*this)) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kRubyElementWithDisplayBlockAndRt);
    }
    if (RuntimeEnabledFeatures::BlockRubyConsoleMessageEnabled()) {
      GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              ConsoleMessage::Source::kRendering, ConsoleMessage::Level::kInfo,
              "A <ruby> with `display: block` won't render its ruby annotation "
              "correctly with future versions of the browser. A workaround is "
              "to specify `display: block; display: block ruby;`."),
          /* discard_duplicates */ true);
    }
    return MakeGarbageCollected<LayoutRubyAsBlock>(this);
  }
  return LayoutObject::CreateObject(this, style);
}

}  // namespace blink
