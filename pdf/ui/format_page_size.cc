// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ui/format_page_size.h"

#include <string>

#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/strings/grit/components_strings.h"
#include "printing/units.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"

using printing::kMicronsPerInch;
using printing::kPointsPerInch;

namespace chrome_pdf {

namespace {

// Scales `length_points` to be in inches instead of points.
constexpr float ConvertPointsToInches(int length_points) {
  constexpr float kInchesPerPoint = 1.0f / kPointsPerInch;
  return length_points * kInchesPerPoint;
}

// Scales `length_points` to be in millimeters instead of points.
constexpr float ConvertPointsToMillimeters(int length_points) {
  constexpr float kMillimetersPerInch = 25.4f;
  constexpr float kMillimetersPerPoint = kMillimetersPerInch / kPointsPerInch;
  return length_points * kMillimetersPerPoint;
}

// Formats a length given in points. The formatted length is in inches and
// contains two fractional digits.
base::string16 FormatLengthInInches(int length_points) {
  return base::FormatDouble(ConvertPointsToInches(length_points),
                            /*fractional_digits=*/2);
}

// Formats a length given in points. The formatted length is in millimeters and
// contains no fractional digits.
base::string16 FormatLengthInMillimeters(int length_points) {
  return base::FormatDouble(ConvertPointsToMillimeters(length_points),
                            /*fractional_digits=*/0);
}

// Returns the localized string for the orientation.
base::string16 GetOrientation(const gfx::Size& size) {
  // TODO(crbug.com/1184345): Add a string for square sizes such that they are
  // not displayed as "portrait".
  return l10n_util::GetStringUTF16(
      size.height() >= size.width() ? IDS_PDF_PROPERTIES_PAGE_SIZE_PORTRAIT
                                    : IDS_PDF_PROPERTIES_PAGE_SIZE_LANDSCAPE);
}

bool ShowInches() {
  UErrorCode error_code = U_ZERO_ERROR;
  UMeasurementSystem system = ulocdata_getMeasurementSystem(
      base::i18n::GetConfiguredLocale().c_str(), &error_code);

  // On error, assume the units are SI.
  return U_SUCCESS(error_code) && system == UMS_US;
}

}  // namespace

base::string16 FormatPageSize(const base::Optional<gfx::Size>& size_points) {
  if (!size_points.has_value())
    return l10n_util::GetStringUTF16(IDS_PDF_PROPERTIES_PAGE_SIZE_VARIABLE);

  // TODO(dhoss): Consider using `icu::number::NumberFormatter`.
  if (ShowInches()) {
    return l10n_util::GetStringFUTF16(
        IDS_PDF_PROPERTIES_PAGE_SIZE_VALUE_INCH,
        FormatLengthInInches(size_points.value().width()),
        FormatLengthInInches(size_points.value().height()),
        GetOrientation(size_points.value()));
  }

  return l10n_util::GetStringFUTF16(
      IDS_PDF_PROPERTIES_PAGE_SIZE_VALUE_MM,
      FormatLengthInMillimeters(size_points.value().width()),
      FormatLengthInMillimeters(size_points.value().height()),
      GetOrientation(size_points.value()));
}

}  // namespace chrome_pdf
