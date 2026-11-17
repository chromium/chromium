// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/scan_save_and_fill/ui/scan_card_expiration_date_formatter.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/data_model/data_model_util.h"

namespace {

// Separator displayed between the month and the year.
constexpr char16_t kSeparator = u'/';

// Digit used to pad a single-digit month.
constexpr char16_t kPaddingDigit = u'0';

// Number of digits of a complete month.
constexpr size_t kMonthDigits = 2;

// Maximum number of year digits kept in the field.
constexpr size_t kMaxYearDigits = 4;

// Highest first digit of a two-digit month; "2" and up are single-digit months.
constexpr char16_t kMaxTwoDigitMonthFirstDigit = u'1';

// Highest second digit after "1" that forms a month (10-12).
constexpr char16_t kMaxSecondDigitAfterOne = u'2';

// How the field lays out its digits as `MM/YYYY`.
struct DigitLayout {
  // Whether a `kPaddingDigit` precedes the month digits.
  bool pad_month = false;
  // Number of leading digits that form the month.
  size_t month_digits = 0;
  // Whether `kSeparator` follows the month.
  bool has_separator = false;
  // Number of digits after the month that form the year.
  size_t year_digits = 0;
};

// Returns the number of ASCII digits in `text`.
size_t CountDigits(std::u16string_view text) {
  return std::ranges::count_if(text, base::IsAsciiDigit<char16_t>);
}

// Returns `text` with every non-digit removed.
std::u16string DigitsOnly(std::u16string_view text) {
  std::u16string digits;
  std::ranges::copy_if(text, std::back_inserter(digits),
                       base::IsAsciiDigit<char16_t>);
  return digits;
}

// Returns a complete-month layout keeping up to `kMaxYearDigits` year digits.
DigitLayout CompleteMonthLayout(bool pad_month,
                                size_t month_digits,
                                size_t remaining_digits) {
  return {.pad_month = pad_month,
          .month_digits = month_digits,
          .has_separator = true,
          .year_digits = std::min(remaining_digits, kMaxYearDigits)};
}

// Lays out typed `digits`, reading "1" then 3-9 as January since no month is
// 13-19; pastes don't do this so they keep matching the validation parser.
DigitLayout LayoutTypedDigits(std::u16string_view digits) {
  if (digits.empty()) {
    return {};
  }
  if (digits[0] > kMaxTwoDigitMonthFirstDigit) {
    return CompleteMonthLayout(/*pad_month=*/true, /*month_digits=*/1,
                               digits.size() - 1);
  }
  if (digits.size() == 1) {
    return {.month_digits = 1};
  }
  if (digits[0] == kMaxTwoDigitMonthFirstDigit &&
      digits[1] > kMaxSecondDigitAfterOne) {
    return CompleteMonthLayout(/*pad_month=*/true, /*month_digits=*/1,
                               digits.size() - 1);
  }
  return CompleteMonthLayout(/*pad_month=*/false, kMonthDigits,
                             digits.size() - kMonthDigits);
}

// Lays out pasted `text` like `autofill::data_util::ParseExpirationDate()` so
// the field agrees with card validation, or `std::nullopt` if it can't parse.
std::optional<DigitLayout> LayoutPastedText(std::u16string_view text) {
  // Trimming ensures the month starts with a digit.
  std::u16string_view trimmed = base::TrimWhitespace(text, base::TRIM_ALL);
  std::u16string month;
  std::u16string year;
  if (!autofill::data_util::ParseExpirationDate(trimmed, &month, &year)) {
    return std::nullopt;
  }
  // `CountDigits()` skips whitespace a split without separator may keep.
  size_t month_digits = CountDigits(month);
  size_t year_digits = CountDigits(year);
  return CompleteMonthLayout(/*pad_month=*/month_digits < kMonthDigits,
                             month_digits, year_digits);
}

// Returns the field text for `digits` laid out as `layout`.
std::u16string BuildText(std::u16string_view digits,
                         const DigitLayout& layout) {
  std::u16string text;
  if (layout.pad_month) {
    text += kPaddingDigit;
  }
  text += digits.substr(0, layout.month_digits);
  if (layout.has_separator) {
    text += kSeparator;
  }
  text += digits.substr(layout.month_digits, layout.year_digits);
  return text;
}

// Returns the caret position after `digits_before_cursor` digits; a caret after
// a complete month skips the separator so typing continues the year.
size_t CursorPosition(const DigitLayout& layout, size_t digits_before_cursor) {
  size_t digits =
      std::min(digits_before_cursor, layout.month_digits + layout.year_digits);
  if (digits == 0) {
    return 0;
  }
  size_t padding = layout.pad_month ? 1 : 0;
  if (digits < layout.month_digits) {
    return padding + digits;
  }
  size_t separator = layout.has_separator ? 1 : 0;
  return padding + layout.month_digits + separator +
         (digits - layout.month_digits);
}

}  // namespace

ScanCardExpirationDateEditResult FormatScanCardExpirationDateEdit(
    std::u16string_view text,
    size_t range_start,
    size_t range_length,
    std::u16string_view replacement) {
  // The text-field caller already drops stale undo ranges.
  CHECK_LE(range_start, text.size());
  CHECK_LE(range_length, text.size() - range_start);

  // Deleting only the separator also deletes the month digit before it, or the
  // still-complete month would re-insert the separator right away.
  bool is_separator_deletion = replacement.empty() && range_length == 1 &&
                               range_start > 0 &&
                               text[range_start] == kSeparator &&
                               base::IsAsciiDigit(text[range_start - 1]);
  if (is_separator_deletion) {
    --range_start;
    ++range_length;
  }

  std::u16string edited_text =
      base::StrCat({text.substr(0, range_start), replacement,
                    text.substr(range_start + range_length)});

  // The delegate can't detect pastes, so multi-character edits are pastes.
  bool is_paste = replacement.size() > 1;
  std::optional<DigitLayout> pasted_layout;
  if (is_paste) {
    pasted_layout = LayoutPastedText(edited_text);
    if (!pasted_layout) {
      return {.text = std::move(edited_text),
              .cursor_position = range_start + replacement.size()};
    }
  }
  std::u16string digits = DigitsOnly(edited_text);
  DigitLayout layout = is_paste ? *pasted_layout : LayoutTypedDigits(digits);
  size_t digits_before_cursor =
      CountDigits(std::u16string_view(edited_text)
                      .substr(0, range_start + replacement.size()));
  return {.text = BuildText(digits, layout),
          .cursor_position = CursorPosition(layout, digits_before_cursor)};
}
