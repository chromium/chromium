// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_size_adjust.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

StringView ToString(FontSizeAdjust::Metric metric) {
  switch (metric) {
    case FontSizeAdjust::Metric::kCapHeight:
      return "cap-height";
    case FontSizeAdjust::Metric::kChWidth:
      return "ch-width";
    case FontSizeAdjust::Metric::kIcWidth:
      return "ic-width";
    case FontSizeAdjust::Metric::kIcHeight:
      return "ic-height";
    case FontSizeAdjust::Metric::kExHeight:
      return "ex-height";
  }
  NOTREACHED();
}

}  // namespace

unsigned FontSizeAdjust::GetHash() const {
  unsigned computed_hash = 0;
  AddFloatToHash(computed_hash, value_);
  AddIntToHash(computed_hash, static_cast<const unsigned>(metric_));
  AddIntToHash(computed_hash, static_cast<const unsigned>(type_));
  return computed_hash;
}

String FontSizeAdjust::ToString() const {
  if (value_ == kFontSizeAdjustNone) {
    return "none";
  }
  String adjustment = IsFromFont() ? "from-font" : String::Number(value_);
  if (metric_ == Metric::kExHeight) {
    return adjustment;
  }
  return StrCat({::blink::ToString(metric_), " ", adjustment});
}

}  // namespace blink
