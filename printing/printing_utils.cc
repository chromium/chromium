// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_utils.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include <unicode/ulocdata.h>

#include <cmath>

#include "base/strings/string_piece.h"
#include "printing/units.h"
#include "ui/gfx/geometry/size.h"
#endif

namespace printing {

namespace {

constexpr size_t kMaxDocumentTitleLength = 80;

#if BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr gfx::Size kIsoA4Microns = gfx::Size(210000, 297000);
#endif

}  // namespace

std::u16string SimplifyDocumentTitleWithLength(const std::u16string& title,
                                               size_t length) {
  std::u16string no_controls(title);
  no_controls.erase(
      std::remove_if(no_controls.begin(), no_controls.end(), &u_iscntrl),
      no_controls.end());

  static constexpr const char* kCharsToReplace[] = {
      "\\", "/", "<", ">", ":", "\"", "'", "|", "?", "*", "~",
  };
  for (const char* c : kCharsToReplace) {
    base::ReplaceChars(no_controls, base::ASCIIToUTF16(c), u"_", &no_controls);
  }

  std::u16string result;
  gfx::ElideString(no_controls, length, &result);
  return result;
}

std::u16string FormatDocumentTitleWithOwnerAndLength(
    const std::u16string& owner,
    const std::u16string& title,
    size_t length) {
  const std::u16string separator = u": ";
  DCHECK_LT(separator.size(), length);

  std::u16string short_title =
      SimplifyDocumentTitleWithLength(owner, length - separator.size());
  short_title += separator;
  if (short_title.size() < length) {
    short_title +=
        SimplifyDocumentTitleWithLength(title, length - short_title.size());
  }

  return short_title;
}

std::u16string SimplifyDocumentTitle(const std::u16string& title) {
  return SimplifyDocumentTitleWithLength(title, kMaxDocumentTitleLength);
}

std::u16string FormatDocumentTitleWithOwner(const std::u16string& owner,
                                            const std::u16string& title) {
  return FormatDocumentTitleWithOwnerAndLength(owner, title,
                                               kMaxDocumentTitleLength);
}

#if BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)
gfx::Size GetDefaultPaperSizeFromLocaleMicrons(base::StringPiece locale) {
  if (locale.empty())
    return kIsoA4Microns;

  int32_t width = 0;
  int32_t height = 0;
  UErrorCode error = U_ZERO_ERROR;
  ulocdata_getPaperSize(std::string(locale).c_str(), &height, &width, &error);
  if (error > U_ZERO_ERROR) {
    // If the call failed, assume Letter paper size.
    LOG(WARNING) << "ulocdata_getPaperSize failed, using ISO A4 Paper, error: "
                 << error;

    return kIsoA4Microns;
  }
  // Convert millis to microns
  return gfx::Size(width * kMicronsPerMm, height * kMicronsPerMm);
}

bool SizesEqualWithinEpsilon(const gfx::Size& lhs,
                             const gfx::Size& rhs,
                             int epsilon) {
  DCHECK_GE(epsilon, 0);

  if (lhs.IsEmpty() && rhs.IsEmpty())
    return true;

  return std::abs(lhs.width() - rhs.width()) <= epsilon &&
         std::abs(lhs.height() - rhs.height()) <= epsilon;
}
#endif  // BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
gfx::Rect GetCenteredPageContentRect(const gfx::Size& paper_size,
                                     const gfx::Size& page_size,
                                     const gfx::Rect& page_content_rect) {
  gfx::Rect content_rect = page_content_rect;
  if (paper_size.width() > page_size.width()) {
    int diff = paper_size.width() - page_size.width();
    content_rect.set_x(content_rect.x() + diff / 2);
  }
  if (paper_size.height() > page_size.height()) {
    int diff = paper_size.height() - page_size.height();
    content_rect.set_y(content_rect.y() + diff / 2);
  }
  return content_rect;
}
#endif  // BUILDFLAG(IS_WIN)

bool LooksLikePdf(base::span<const char> maybe_pdf_data) {
  return maybe_pdf_data.size() >= 50u &&
         std::memcmp(maybe_pdf_data.data(), "%PDF-", 5) == 0;
}

}  // namespace printing
