// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/text_fit.h"

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TextFitMethod TextFit::Method() const {
  return RuntimeEnabledFeatures::CssTextFitReshapingEnabled()
             ? TextFitMethod::kFontSize
             : TextFitMethod::kScale;
}

String TextFit::ToString() const {
  StringView type;
  switch (Type()) {
    case TextFitType::kNone:
      type = "none";
      break;
    case TextFitType::kGrow:
      type = "grow";
      break;
    case TextFitType::kShrink:
      type = "shrink";
      break;
  }

  StringView target;
  switch (Target()) {
    case TextFitTarget::kConsistent:
      target = "consistent";
      break;
    case TextFitTarget::kPerLine:
      target = "per-line";
      break;
    case TextFitTarget::kPerLineAll:
      target = "per-line-all";
      break;
  }

  String limit;
  if (ScaleFactorLimit()) {
    limit = String::Number(*ScaleFactorLimit());
  }
  return StrCat({"TextFit {type:", type, ", target:", target,
                 limit.empty() ? "" : ", scale-factor-limit:", limit, "}"});
}

}  // namespace blink
