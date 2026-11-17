// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/scan_save_and_fill/ui/scan_card_expiration_date_formatter.h"

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Returns the result of typing `character` at the end of `text`.
ScanCardExpirationDateEditResult TypeAtEnd(std::u16string_view text,
                                           std::u16string_view character) {
  return FormatScanCardExpirationDateEdit(text, text.size(), /*range_length=*/0,
                                          character);
}

// Returns the result of pressing backspace at the end of `text`, which must
// not be empty.
ScanCardExpirationDateEditResult BackspaceAtEnd(std::u16string_view text) {
  CHECK(!text.empty());
  return FormatScanCardExpirationDateEdit(text, text.size() - 1,
                                          /*range_length=*/1, u"");
}

// Returns the result of pasting `pasted_text` into an empty field.
ScanCardExpirationDateEditResult PasteIntoEmptyField(
    std::u16string_view pasted_text) {
  return FormatScanCardExpirationDateEdit(u"", /*range_start=*/0,
                                          /*range_length=*/0, pasted_text);
}

// Tests that typing 2-9 first pads the month and adds the separator.
TEST(ScanCardExpirationDateFormatterTest, TypingFirstDigitTwoToNinePadsMonth) {
  EXPECT_EQ(u"02/", TypeAtEnd(u"", u"2").text);
  EXPECT_EQ(u"03/", TypeAtEnd(u"", u"3").text);
  EXPECT_EQ(u"09/", TypeAtEnd(u"", u"9").text);
}

// Tests that a first digit of 0 or 1 waits for the second month digit.
TEST(ScanCardExpirationDateFormatterTest, TypingFirstDigitZeroOrOneWaits) {
  EXPECT_EQ(u"0", TypeAtEnd(u"", u"0").text);
  EXPECT_EQ(u"1", TypeAtEnd(u"", u"1").text);
}

// Tests that a second month digit completes the month, even if invalid.
TEST(ScanCardExpirationDateFormatterTest,
     TypingSecondMonthDigitCompletesMonth) {
  EXPECT_EQ(u"10/", TypeAtEnd(u"1", u"0").text);
  EXPECT_EQ(u"12/", TypeAtEnd(u"1", u"2").text);
  EXPECT_EQ(u"00/", TypeAtEnd(u"0", u"0").text);
}

// Tests that typing a non-digit leaves the text unchanged.
TEST(ScanCardExpirationDateFormatterTest, TypingNonDigitIsIgnored) {
  ScanCardExpirationDateEditResult result = TypeAtEnd(u"1", u"a");
  EXPECT_EQ(u"1", result.text);
  EXPECT_EQ(1u, result.cursor_position);
}

// Tests that typing 1 then 3-9 reads as January and starts the year.
TEST(ScanCardExpirationDateFormatterTest, TypingOneThenThreeToNineIsJanuary) {
  EXPECT_EQ(u"01/3", TypeAtEnd(u"1", u"3").text);
  EXPECT_EQ(u"01/9", TypeAtEnd(u"1", u"9").text);
}

// Tests that the typed year keeps at most four digits.
TEST(ScanCardExpirationDateFormatterTest, TypingYearKeepsAtMostFourDigits) {
  EXPECT_EQ(u"12/2100", TypeAtEnd(u"12/210", u"0").text);
  EXPECT_EQ(u"12/2100", TypeAtEnd(u"12/2100", u"1").text);
}

// Tests that backspace after the separator also deletes the last month digit.
TEST(ScanCardExpirationDateFormatterTest,
     BackspaceAfterSeparatorDeletesMonthDigit) {
  EXPECT_EQ(u"1", BackspaceAtEnd(u"12/").text);
  EXPECT_EQ(u"0", BackspaceAtEnd(u"03/").text);
}

// Tests that an edit inside the text reformats the remaining digits.
TEST(ScanCardExpirationDateFormatterTest, MiddleEditReformatsDigits) {
  EXPECT_EQ(u"03/29",
            FormatScanCardExpirationDateEdit(u"12/29", /*range_start=*/0,
                                             /*range_length=*/2, u"3")
                .text);
}

// Tests that the caret lands right after the edited digit.
TEST(ScanCardExpirationDateFormatterTest, EditPlacesCursorAfterEditedDigit) {
  EXPECT_EQ(3u, TypeAtEnd(u"", u"3").cursor_position);
  EXPECT_EQ(1u, FormatScanCardExpirationDateEdit(u"12/29", /*range_start=*/1,
                                                 /*range_length=*/1, u"")
                    .cursor_position);
  EXPECT_EQ(4u, FormatScanCardExpirationDateEdit(u"12/29", /*range_start=*/2,
                                                 /*range_length=*/0, u"5")
                    .cursor_position);
}

// Tests that a pasted "/" or "-" splits the month and the year.
TEST(ScanCardExpirationDateFormatterTest,
     PasteWithSeparatorSplitsMonthAndYear) {
  EXPECT_EQ(u"03/29", PasteIntoEmptyField(u"3-29").text);
  EXPECT_EQ(u"12/15", PasteIntoEmptyField(u"12-15").text);
  EXPECT_EQ(u"12/2029", PasteIntoEmptyField(u"12/2029").text);
}

// Tests that pasted digits always use the first two digits as the month.
TEST(ScanCardExpirationDateFormatterTest, PasteDigitsUseTwoDigitMonth) {
  EXPECT_EQ(u"12/29", PasteIntoEmptyField(u"1229").text);
  EXPECT_EQ(u"01/29", PasteIntoEmptyField(u"0129").text);
  EXPECT_EQ(u"12/2029", PasteIntoEmptyField(u"122029").text);
  EXPECT_EQ(u"13/29", PasteIntoEmptyField(u"1329").text);
}

// Tests that whitespace around pasted text doesn't block parsing.
TEST(ScanCardExpirationDateFormatterTest, PasteTrimsSurroundingWhitespace) {
  EXPECT_EQ(u"12/29", PasteIntoEmptyField(u"  1229").text);
}

// Tests that a paste after existing text formats the combined text.
TEST(ScanCardExpirationDateFormatterTest, PasteAfterExistingTextFormatsResult) {
  EXPECT_EQ(u"12/29",
            FormatScanCardExpirationDateEdit(u"12/", /*range_start=*/3,
                                             /*range_length=*/0, u"29")
                .text);
}

// Tests that the field keeps pasted text the parser can't read.
TEST(ScanCardExpirationDateFormatterTest, PasteUnparsableTextIsKept) {
  EXPECT_EQ(u"329", PasteIntoEmptyField(u"329").text);
}

}  // namespace
