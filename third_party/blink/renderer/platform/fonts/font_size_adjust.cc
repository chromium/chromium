// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_size_adjust.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

unsigned FontSizeAdjust::GetHash() const {
  unsigned computed_hash = 0;
  // Normalize negative zero.
  WTF::AddFloatToHash(computed_hash, value_ == 0.0 ? 0.0 : value_);
  WTF::AddIntToHash(computed_hash, static_cast<const unsigned>(metric_));
  WTF::AddIntToHash(computed_hash, static_cast<const unsigned>(type_));
  return computed_hash;
}

String FontSizeAdjust::ToString(Metric metric) const {
  switch (metric) {
    case Metric::kCapHeight:
      return "cap-height";
    case Metric::kChWidth:
      return "ch-width";
    case Metric::kIcWidth:
      return "ic-width";
    case Metric::kIcHeight:
      return "ic-height";
    case Metric::kExHeight:
      return "ex-height";
  }
  NOTREACHED_IN_MIGRATION();
}

String FontSizeAdjust::ToString() const {
  if (value_ == kFontSizeAdjustNone) {
    return "none";
  }

  if (metric_ == Metric::kExHeight) {
    return IsFromFont()
               ? "from-font"
               : String::Format("%s", String::Number(value_).Ascii().c_str());
  }

  return IsFromFont()
             ? String::Format("%s from-font", ToString(metric_).Ascii().c_str())
             : String::Format("%s %s", ToString(metric_).Ascii().c_str(),
                              String::Number(value_).Ascii().c_str());
}

}  // namespace blink
