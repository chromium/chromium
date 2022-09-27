// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_directive.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_text_directive_options.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_finder.h"

namespace blink {

// static
TextDirective* TextDirective::Create(const String& directive_value) {
  auto selector = TextFragmentSelector::FromTextDirective(directive_value);
  if (selector.Type() == TextFragmentSelector::kInvalid)
    return nullptr;

  return MakeGarbageCollected<TextDirective>(selector);
}

TextDirective::TextDirective(const TextFragmentSelector& selector)
    : SelectorDirective(Directive::kText), selector_(selector) {}

TextDirective::~TextDirective() = default;

TextDirective* TextDirective::Create(TextDirectiveOptions* options) {
  String prefix;
  String textStart;
  String textEnd;
  String suffix;

  if (options) {
    if (options->hasPrefix())
      prefix = options->prefix();

    if (options->hasTextStart())
      textStart = options->textStart();

    if (options->hasTextEnd())
      textEnd = options->textEnd();

    if (options->hasSuffix())
      suffix = options->suffix();
  }

  TextFragmentSelector::SelectorType type = TextFragmentSelector::kInvalid;

  if (!textStart.empty()) {
    if (!textEnd.empty())
      type = TextFragmentSelector::kRange;
    else
      type = TextFragmentSelector::kExact;
  }

  return MakeGarbageCollected<TextDirective>(
      TextFragmentSelector(type, textStart, textEnd, prefix, suffix));
}

const String TextDirective::prefix() const {
  return selector_.Prefix();
}

const String TextDirective::textStart() const {
  return selector_.Start();
}

const String TextDirective::textEnd() const {
  return selector_.End();
}

const String TextDirective::suffix() const {
  return selector_.Suffix();
}

void TextDirective::Trace(Visitor* visitor) const {
  SelectorDirective::Trace(visitor);
}

String TextDirective::ToStringImpl() const {
  return type() + "=" + selector_.ToString();
}

}  // namespace blink
