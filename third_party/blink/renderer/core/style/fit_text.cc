// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/fit_text.h"

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

FitTextMethod FitText::Method() const {
  return RuntimeEnabledFeatures::CssFitWidthTextReshapingEnabled()
             ? FitTextMethod::kFontSize
             : FitTextMethod::kScale;
}

String FitText::ToString() const {
  StringView type;
  switch (Type()) {
    case FitTextType::kNone:
      type = "none";
      break;
    case FitTextType::kGrow:
      type = "grow";
      break;
    case FitTextType::kShrink:
      type = "shrink";
      break;
  }

  StringView target;
  switch (Target()) {
    case FitTextTarget::kConsistent:
      target = "consistent";
      break;
    case FitTextTarget::kPerLine:
      target = "per-line";
      break;
    case FitTextTarget::kPerLineAll:
      target = "per-line-all";
      break;
  }

  String limit;
  if (ScaleFactorLimit()) {
    limit = String::Number(*ScaleFactorLimit());
  }
  return StrCat({"FitText {type:", type, ", target:", target,
                 limit.empty() ? "" : ", scale-factor-limit:", limit, "}"});
}

}  // namespace blink
