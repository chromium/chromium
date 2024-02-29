// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ui/document_properties.h"

#include <optional>
#include <string>

#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "pdf/document_metadata.h"
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
std::u16string FormatLengthInInches(int length_points) {
  return base::FormatDouble(ConvertPointsToInches(length_points),
                            /*fractional_digits=*/2);
}

// Formats a length given in points. The formatted length is in millimeters and
// contains no fractional digits.
std::u16string FormatLengthInMillimeters(int length_points) {
  return base::FormatDouble(ConvertPointsToMillimeters(length_points),
                            /*fractional_digits=*/0);
}

// Returns the localized string for the orientation.
std::u16string GetOrientation(const gfx::Size& size) {
  int message_id;
  if (size.height() > size.width())
    message_id = IDS_PDF_PROPERTIES_PAGE_SIZE_PORTRAIT;
  else if (size.height() < size.width())
    message_id = IDS_PDF_PROPERTIES_PAGE_SIZE_LANDSCAPE;
  else
    message_id = IDS_PDF_PROPERTIES_PAGE_SIZE_SQUARE;

  return l10n_util::GetStringUTF16(message_id);
}

bool ShowInches() {
  UErrorCode error_code = U_ZERO_ERROR;
  UMeasurementSystem system = ulocdata_getMeasurementSystem(
      base::i18n::GetConfiguredLocale().c_str(), &error_code);

  // On error, assume the units are SI.
  return U_SUCCESS(error_code) && system == UMS_US;
}

}  // namespace

std::u16string FormatPageSize(const std::optional<gfx::Size>& size_points) {
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

std::string FormatPdfVersion(PdfVersion version) {
  switch (version) {
    case PdfVersion::k1_0:
      return "1.0";
    case PdfVersion::k1_1:
      return "1.1";
    case PdfVersion::k1_2:
      return "1.2";
    case PdfVersion::k1_3:
      return "1.3";
    case PdfVersion::k1_4:
      return "1.4";
    case PdfVersion::k1_5:
      return "1.5";
    case PdfVersion::k1_6:
      return "1.6";
    case PdfVersion::k1_7:
      return "1.7";
    case PdfVersion::k2_0:
      return "2.0";
    case PdfVersion::kUnknown:
    case PdfVersion::k1_8:  // Not an actual version
      return std::string();
  }
  // The default case is excluded from the above switch statement to ensure that
  // all supported versions are determinantly handled.
}

}  // namespace chrome_pdf
