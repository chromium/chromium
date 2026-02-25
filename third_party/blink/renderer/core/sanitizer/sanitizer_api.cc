// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_unsafe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizer_sanitizerconfig_sanitizerpresets.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/parser/fragment_parser_options.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"
#include "third_party/blink/renderer/core/sanitizer/streaming_sanitizer.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

namespace {

const Sanitizer* SanitizerFromOptions(const FragmentParserOptions& options,
                                      Sanitizer::Mode mode,
                                      ExceptionState& exception_state) {
  if (!options.sanitizer_init()) {
    return Sanitizer::Create(nullptr, mode, exception_state);
  }

  switch (options.sanitizer_init()->GetContentType()) {
    case V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets::ContentType::
        kSanitizer:
      return options.sanitizer_init()->GetAsSanitizer();
    case V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets::ContentType::
        kSanitizerConfig:
      return Sanitizer::Create(options.sanitizer_init()->GetAsSanitizerConfig(),
                               mode, exception_state);
    case V8UnionSanitizerOrSanitizerConfigOrSanitizerPresets::ContentType::
        kSanitizerPresets:
      return Sanitizer::Create(
          options.sanitizer_init()->GetAsSanitizerPresets().AsEnum(),
          exception_state);
  }
}
}  // namespace

void SanitizerAPI::SanitizeInternal(Sanitizer::Mode mode,
                                    const ContainerNode* context_element,
                                    ContainerNode* root_element,
                                    FragmentParserOptions options,
                                    ExceptionState& exception_state) {
  // Per spec, we need to parse & sanitize into an inert (non-active) document.
  CHECK(!root_element->GetDocument().IsActive());

  if (exception_state.HadException()) {
    root_element->setTextContent("");
    return;
  }

  if (mode == Sanitizer::Mode::kSafe && context_element->IsElementNode()) {
    const Element* real_element = To<Element>(context_element);
    if (real_element->TagQName() == html_names::kScriptTag ||
        real_element->TagQName() == svg_names::kScriptTag) {
      root_element->setTextContent("");
      return;
    }
  }

  const Sanitizer* sanitizer =
      SanitizerFromOptions(options, mode, exception_state);

  if (exception_state.HadException()) {
    return;
  }

  CHECK(sanitizer);
  switch (mode) {
    case Sanitizer::Mode::kSafe:
      sanitizer->SanitizeSafe(root_element);
      break;
    case Sanitizer::Mode::kUnsafe:
      sanitizer->SanitizeUnsafe(root_element);
      break;
  }
}

StreamingSanitizer* SanitizerAPI::CreateStreamingSanitizerInternal(
    FragmentParserOptions options,
    const ContainerNode* context,
    ExceptionState& exception_state) {
  const Sanitizer* sanitizer =
      SanitizerFromOptions(options, Sanitizer::Mode::kUnsafe, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  CHECK(sanitizer);
  return StreamingSanitizer::CreateUnsafe(sanitizer, context);
}

}  // namespace blink
