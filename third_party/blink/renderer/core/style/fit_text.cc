// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/fit_text.h"

#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String FitText::ToString() const {
  StringView target;
  switch (Target()) {
    case FitTextTarget::kNone:
      target = "none";
      break;
    case FitTextTarget::kPerLine:
      target = "per-line";
      break;
    case FitTextTarget::kConsistent:
      target = "consistent";
      break;
  }

  StringView method;
  if (method_) {
    switch (*method_) {
      case FitTextMethod::kScale:
        method = "scale";
        break;
      case FitTextMethod::kFontSize:
        method = "font-size";
        break;
      case FitTextMethod::kScaleInline:
        method = "scale-inline";
        break;
      case FitTextMethod::kLetterSpacing:
        method = "letter-spacing";
        break;
    }
  }

  String size_limit;
  if (SizeLimit()) {
    size_limit = String::Number(*SizeLimit());
  }
  return StrCat({"FitText {target:", target,
                 method.empty() ? "" : ", method:", method,
                 size_limit.empty() ? "" : ", size-limit:", size_limit, "}"});
}

}  // namespace blink
