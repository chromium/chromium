// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_SCAN_CARD_EXPIRATION_DATE_FORMATTER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_SCAN_CARD_EXPIRATION_DATE_FORMATTER_H_

#include <stddef.h>

#include <string>
#include <string_view>

// Text and caret position of the expiration date field after an edit.
struct ScanCardExpirationDateEditResult {
  // Formatted field text.
  std::u16string text;
  // Caret offset into `text`, in UTF-16 code units.
  size_t cursor_position = 0;
};

// Returns the result of replacing `range_length` UTF-16 code units in `text`
// starting at `range_start` with `replacement`. Formats valid expiration dates
// with a zero-padded month and a '/' separator, while leaving unparsable
// multi-character replacements unformatted.
ScanCardExpirationDateEditResult FormatScanCardExpirationDateEdit(
    std::u16string_view text,
    size_t range_start,
    size_t range_length,
    std::u16string_view replacement);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_SCAN_CARD_EXPIRATION_DATE_FORMATTER_H_
