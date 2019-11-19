// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_variant_numeric.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static const char* kUnknownNumericString = "Unknown";

String FontVariantNumeric::ToString(NumericFigure figure) {
  switch (figure) {
    case kNormalFigure:
      return "NormalFigure";
    case kLiningNums:
      return "LiningNums";
    case kOldstyleNums:
      return "OldstyleNums";
  }
  return kUnknownNumericString;
}

String FontVariantNumeric::ToString(NumericSpacing spacing) {
  switch (spacing) {
    case kNormalSpacing:
      return "NormalSpacing";
    case kProportionalNums:
      return "ProportionalNums";
    case kTabularNums:
      return "TabularNums";
  }
  return kUnknownNumericString;
}

String FontVariantNumeric::ToString(NumericFraction fraction) {
  switch (fraction) {
    case kNormalFraction:
      return "Normal";
    case kDiagonalFractions:
      return "Diagonal";
    case kStackedFractions:
      return "Stacked";
  }
  return kUnknownNumericString;
}

String FontVariantNumeric::ToString(Ordinal ordinal) {
  switch (ordinal) {
    case kOrdinalOff:
      return "Off";
    case kOrdinalOn:
      return "On";
  }
  return kUnknownNumericString;
}

String FontVariantNumeric::ToString(SlashedZero slashed) {
  switch (slashed) {
    case kSlashedZeroOff:
      return "Off";
    case kSlashedZeroOn:
      return "On";
  }
  return kUnknownNumericString;
}

String FontVariantNumeric::ToString() const {
  return String::Format(
      "numeric_figure=%s, numeric_spacing=%s, numeric_fraction=%s, ordinal=%s, "
      "slashed_zero=%s",
      ToString(NumericFigureValue()).Ascii().c_str(),
      ToString(NumericSpacingValue()).Ascii().c_str(),
      ToString(NumericFractionValue()).Ascii().c_str(),
      ToString(OrdinalValue()).Ascii().c_str(),
      ToString(SlashedZeroValue()).Ascii().c_str());
}

}  // namespace blink
