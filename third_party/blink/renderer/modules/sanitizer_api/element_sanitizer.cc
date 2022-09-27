// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "element_sanitizer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_element_set_html_options.h"
#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer.h"

namespace blink {

void ElementSanitizer::setHTML(ScriptState* script_state,
                               Element& element,
                               const String& markup,
                               ElementSetHTMLOptions* options,
                               ExceptionState& exception_state) {
  options->getSanitizerOr(Sanitizer::getDefaultInstance())
      ->ElementSetHTML(script_state, element, markup, exception_state);
}

}  // namespace blink
