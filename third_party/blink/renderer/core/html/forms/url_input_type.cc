/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#include "third_party/blink/renderer/core/html/forms/url_input_type.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

void URLInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeURL);
}

bool URLInputType::TypeMismatchFor(const String& value) const {
  return !value.empty() && !KURL(NullURL(), value).IsValid();
}

bool URLInputType::TypeMismatch() const {
  return TypeMismatchFor(GetElement().Value());
}

String URLInputType::TypeMismatchText() const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_TYPE_MISMATCH_URL);
}

String URLInputType::SanitizeValue(const String& proposed_value) const {
  return BaseTextInputType::SanitizeValue(
      StripLeadingAndTrailingHTMLSpaces(proposed_value));
}

String URLInputType::SanitizeUserInputValue(
    const String& proposed_value) const {
  // Do not call URLInputType::sanitizeValue.
  return BaseTextInputType::SanitizeValue(proposed_value);
}

}  // namespace blink
