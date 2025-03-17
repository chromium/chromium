// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_unsafe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizer_sanitizerconfig_sanitizerpresets.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

// Note: SanitizerSafeInternal and SanitizerUnsafeInternal are mostly identical.
//   But because SetHTMLOptions and SetHTMLUnsafeOptions are unrelated types (as
//   far as C++ is concerned) they cannot easily be merged.

void SanitizerAPI::SanitizeSafeInternal(ContainerNode* element,
                                        SetHTMLOptions* options,
                                        ExceptionState& exception_state) {
  if (exception_state.HadException()) {
    element->setTextContent("");
    return;
  }

  if (element->IsElementNode()) {
    const Element* real_element = To<Element>(element);
    if (real_element->TagQName() == html_names::kScriptTag ||
        real_element->TagQName() == svg_names::kScriptTag) {
      element->setTextContent("");
      return;
    }
  }

  const Sanitizer* sanitizer = nullptr;
  if (!options || !options->hasSanitizer()) {
    // Default case: No dictionary, or dictionary without 'sanitizer' member.
    sanitizer = Sanitizer::Create(nullptr, /*safe*/ true, exception_state);
  } else {
    if (options->sanitizer()->IsSanitizer()) {
      // We already got a sanitizer.
      sanitizer = options->sanitizer()->GetAsSanitizer();
    } else if (options->sanitizer()->IsSanitizerConfig()) {
      // We need to create a Sanitizer from a given config.
      sanitizer =
          Sanitizer::Create(options->sanitizer()->GetAsSanitizerConfig(),
                            /*safe*/ true, exception_state);
    } else if (options->sanitizer()->IsSanitizerPresets()) {
      // Create a Sanitizer from a "preset" string.
      sanitizer = Sanitizer::Create(
          options->sanitizer()->GetAsSanitizerPresets().AsEnum(),
          exception_state);
    } else {
      // Default case: Dictionary with 'sanitizer' member but no (valid) value.
      sanitizer = Sanitizer::Create(nullptr, /*safe*/ true, exception_state);
    }
  }

  if (exception_state.HadException()) {
    return;
  }

  CHECK(sanitizer);
  sanitizer->SanitizeSafe(element);
}

void SanitizerAPI::SanitizeUnsafeInternal(ContainerNode* element,
                                          SetHTMLUnsafeOptions* options,
                                          ExceptionState& exception_state) {
  if (exception_state.HadException()) {
    element->setTextContent("");
    return;
  }

  const Sanitizer* sanitizer = nullptr;
  if (!options || !options->hasSanitizer()) {
    // Default case: No dictionary, or dictionary without 'sanitizer' member.
    sanitizer = Sanitizer::Create(nullptr, /*safe*/ false, exception_state);
  } else {
    if (options->sanitizer()->IsSanitizer()) {
      // We already got a sanitizer.
      sanitizer = options->sanitizer()->GetAsSanitizer();
    } else if (options->sanitizer()->IsSanitizerConfig()) {
      // We need to create a Sanitizer from a given config.
      sanitizer =
          Sanitizer::Create(options->sanitizer()->GetAsSanitizerConfig(),
                            /*safe*/ false, exception_state);
    } else if (options->sanitizer()->IsSanitizerPresets()) {
      // Create a Sanitizer from a "preset" string.
      sanitizer = Sanitizer::Create(
          options->sanitizer()->GetAsSanitizerPresets().AsEnum(),
          exception_state);
    } else {
      // Default case: Dictionary with 'sanitizer' member but not (valid) value.
      sanitizer = Sanitizer::Create(nullptr, /*safe*/ false, exception_state);
    }
  }

  if (exception_state.HadException()) {
    return;
  }

  CHECK(sanitizer);
  sanitizer->SanitizeUnsafe(element);
}

}  // namespace blink
