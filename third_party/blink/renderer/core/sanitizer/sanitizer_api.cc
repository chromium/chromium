// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizer_sanitizerconfig.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

void SanitizerAPI::SanitizeSafeInternal(ContainerNode* element,
                                        SetHTMLOptions* options,
                                        ExceptionState& exception_state) {
  if (element->IsElementNode()) {
    const Element* real_element = To<Element>(element);
    if (real_element->TagQName() == html_names::kScriptTag ||
        real_element->TagQName() == svg_names::kScriptTag) {
      return;
    }
  }

  const Sanitizer* sanitizer = nullptr;
  if (options && options->hasSanitizer()) {
    sanitizer =
        options->sanitizer()->IsSanitizer()
            ? options->sanitizer()->GetAsSanitizer()
            : Sanitizer::Create(options->sanitizer()->GetAsSanitizerConfig(),
                                exception_state);
  }
  if (!sanitizer) {
    sanitizer = SanitizerBuiltins::GetDefaultSafe();
  }
  sanitizer->SanitizeSafe(element);
}

void SanitizerAPI::SanitizeUnsafeInternal(ContainerNode* element,
                                          SetHTMLOptions* options,
                                          ExceptionState& exception_state) {
  const Sanitizer* sanitizer = nullptr;
  if (options && options->hasSanitizer()) {
    sanitizer =
        options->sanitizer()->IsSanitizer()
            ? options->sanitizer()->GetAsSanitizer()
            : Sanitizer::Create(options->sanitizer()->GetAsSanitizerConfig(),
                                exception_state);
  }
  if (!sanitizer) {
    sanitizer = SanitizerBuiltins::GetDefaultUnsafe();
  }
  sanitizer->SanitizeUnsafe(element);
}

}  // namespace blink
