// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_util.h"

#include <map>
#include <vector>

#include "base/check_op.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/unicodestring.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtfmtsym.h"

using base::ASCIIToUTF16;
using base::StringPiece16;

// For examples of Unix<->VMS path conversions, see the unit test file. On VMS
// a path looks differently depending on whether it's a file or directory.

namespace net {

// static
std::string FtpUtil::UnixFilePathToVMS(const std::string& unix_path) {
  if (unix_path.empty())
    return std::string();

  base::StringTokenizer tokenizer(unix_path, "/");
  std::vector<std::string> tokens;
  while (tokenizer.GetNext())
    tokens.push_back(tokenizer.token());

  if (unix_path[0] == '/') {
    // It's an absolute path.

    if (tokens.empty()) {
      // It's just "/" or a series of slashes, which all mean the same thing.
      return "[]";
    }

    if (tokens.size() == 1)
      return tokens.front();  // Return without leading slashes.

    std::string result(tokens[0] + ":[");
    if (tokens.size() == 2) {
      // Don't ask why, it just works that way on VMS.
      result.append("000000");
    } else {
      result.append(tokens[1]);
      for (size_t i = 2; i < tokens.size() - 1; i++)
        result.append("." + tokens[i]);
    }
    result.append("]" + tokens.back());
    return result;
  }

  if (tokens.size() == 1)
    return unix_path;

  std::string result("[");
  for (size_t i = 0; i < tokens.size() - 1; i++)
    result.append("." + tokens[i]);
  result.append("]" + tokens.back());
  return result;
}

// static
std::string FtpUtil::UnixDirectoryPathToVMS(const std::string& unix_path) {
  if (unix_path.empty())
    return std::string();

  std::string path(unix_path);

  if (path.back() != '/')
    path.append("/");

  // Reuse logic from UnixFilePathToVMS by appending a fake file name to the
  // real path and removing it after conversion.
  path.append("x");
  path = UnixFilePathToVMS(path);
  return path.substr(0, path.length() - 1);
}

// static
std::string FtpUtil::VMSPathToUnix(const std::string& vms_path) {
  if (vms_path.empty())
    return ".";

  if (vms_path[0] == '/') {
    // This is not really a VMS path. Most likely the server is emulating UNIX.
    // Return path as-is.
    return vms_path;
  }

  if (vms_path == "[]")
    return "/";

  std::string result(vms_path);
  if (vms_path[0] == '[') {
    // It's a relative path.
    base::ReplaceFirstSubstringAfterOffset(
        &result, 0, "[.", base::StringPiece());
  } else {
    // It's an absolute path.
    result.insert(0, "/");
    base::ReplaceSubstringsAfterOffset(&result, 0, ":[000000]", "/");
    base::ReplaceSubstringsAfterOffset(&result, 0, ":[", "/");
  }
  std::replace(result.begin(), result.end(), '.', '/');
  std::replace(result.begin(), result.end(), ']', '/');

  // Make sure the result doesn't end with a slash.
  if (!result.empty() && result.back() == '/')
    result = result.substr(0, result.length() - 1);

  return result;
}

namespace {

// Lazy-initialized map of abbreviated month names.
class AbbreviatedMonthsMap {
 public:
  static AbbreviatedMonthsMap* GetInstance() {
    return base::Singleton<AbbreviatedMonthsMap>::get();
  }

  // Converts abbreviated month name |text| to its number (in range 1-12).
  // On success returns true and puts the number in |number|.
  bool GetMonthNumber(const base::string16& text, int* number) {
    // Ignore the case of the month names. The simplest way to handle that
    // is to make everything lowercase.
    base::string16 text_lower(base::i18n::ToLower(text));

    if (map_.find(text_lower) == map_.end())
      return false;

    *number = map_[text_lower];
    return true;
  }

 private:
  friend struct base::DefaultSingletonTraits<AbbreviatedMonthsMap>;

  // Constructor, initializes the map based on ICU data. It is much faster
  // to do that just once.
  AbbreviatedMonthsMap() {
    int32_t locales_count;
    const icu::Locale* locales =
        icu::DateFormat::getAvailableLocales(locales_count);

    for (int32_t locale = 0; locale < locales_count; locale++) {
      UErrorCode status(U_ZERO_ERROR);

      icu::DateFormatSymbols format_symbols(locales[locale], status);

      // If we cannot get format symbols for some locale, it's not a fatal
      // error. Just try another one.
      if (U_FAILURE(status))
        continue;

      int32_t months_count;
      const icu::UnicodeString* months =
          format_symbols.getShortMonths(months_count);

      for (int32_t month = 0; month < months_count; month++) {
        base::string16 month_name(
            base::i18n::UnicodeStringToString16(months[month]));

        // Ignore the case of the month names. The simplest way to handle that
        // is to make everything lowercase.
        month_name = base::i18n::ToLower(month_name);

        map_[month_name] = month + 1;

        // Sometimes ICU returns longer strings, but in FTP listings a shorter
        // abbreviation is used (for example for the Russian locale). Make sure
        // we always have a map entry for a three-letter abbreviation.
        map_[month_name.substr(0, 3)] = month + 1;
      }
    }

    // Fail loudly if the data returned by ICU is obviously incomplete.
    // This is intended to catch cases like http://crbug.com/177428
    // much earlier. Note that the issue above turned out to be non-trivial
    // to reproduce - crash data is much better indicator of a problem
    // than incomplete bug reports.
    CHECK_EQ(1, map_[ASCIIToUTF16("jan")]);
    CHECK_EQ(2, map_[ASCIIToUTF16("feb")]);
    CHECK_EQ(3, map_[ASCIIToUTF16("mar")]);
    CHECK_EQ(4, map_[ASCIIToUTF16("apr")]);
    CHECK_EQ(5, map_[ASCIIToUTF16("may")]);
    CHECK_EQ(6, map_[ASCIIToUTF16("jun")]);
    CHECK_EQ(7, map_[ASCIIToUTF16("jul")]);
    CHECK_EQ(8, map_[ASCIIToUTF16("aug")]);
    CHECK_EQ(9, map_[ASCIIToUTF16("sep")]);
    CHECK_EQ(10, map_[ASCIIToUTF16("oct")]);
    CHECK_EQ(11, map_[ASCIIToUTF16("nov")]);
    CHECK_EQ(12, map_[ASCIIToUTF16("dec")]);
  }

  // Maps lowercase month names to numbers in range 1-12.
  std::map<base::string16, int> map_;

  DISALLOW_COPY_AND_ASSIGN(AbbreviatedMonthsMap);
};

}  // namespace

// static
bool FtpUtil::AbbreviatedMonthToNumber(const base::string16& text,
                                       int* number) {
  return AbbreviatedMonthsMap::GetInstance()->GetMonthNumber(text, number);
}

// static
bool FtpUtil::LsDateListingToTime(const base::string16& month,
                                  const base::string16& day,
                                  const base::string16& rest,
                                  const base::Time& current_time,
                                  base::Time* result) {
  base::Time::Exploded time_exploded = { 0 };

  if (!AbbreviatedMonthToNumber(month, &time_exploded.month)) {
    // Work around garbage sent by some servers in the same column
    // as the month. Take just last 3 characters of the string.
    if (month.length() < 3 ||
        !AbbreviatedMonthToNumber(month.substr(month.length() - 3),
                                  &time_exploded.month)) {
      return false;
    }
  }

  if (!base::StringToInt(day, &time_exploded.day_of_month))
    return false;
  if (time_exploded.day_of_month > 31)
    return false;

  if (!base::StringToInt(rest, &time_exploded.year)) {
    // Maybe it's time. Does it look like time? Note that it can be any of
    // "HH:MM", "H:MM", "HH:M" or maybe even "H:M".
    if (rest.length() > 5)
      return false;

    size_t colon_pos = rest.find(':');
    if (colon_pos == base::string16::npos)
      return false;
    if (colon_pos > 2)
      return false;

    if (!base::StringToInt(
            base::MakeStringPiece16(rest.begin(), rest.begin() + colon_pos),
            &time_exploded.hour)) {
      return false;
    }
    if (!base::StringToInt(
            base::MakeStringPiece16(rest.begin() + colon_pos + 1, rest.end()),
            &time_exploded.minute)) {
      return false;
    }

    // Guess the year.
    base::Time::Exploded current_exploded;
    current_time.UTCExplode(&current_exploded);

    // If it's not possible for the parsed date to be in the current year,
    // use the previous year.
    if (time_exploded.month > current_exploded.month ||
        (time_exploded.month == current_exploded.month &&
         time_exploded.day_of_month > current_exploded.day_of_month)) {
      time_exploded.year = current_exploded.year - 1;
    } else {
      time_exploded.year = current_exploded.year;
    }
  }

  // We don't know the time zone of the listing, so just use UTC.
  return base::Time::FromUTCExploded(time_exploded, result);
}

// static
bool FtpUtil::WindowsDateListingToTime(const base::string16& date,
                                       const base::string16& time,
                                       base::Time* result) {
  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format MM-DD-YY[YY].
  std::vector<base::StringPiece16> date_parts =
      base::SplitStringPiece(date, base::ASCIIToUTF16("-"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (date_parts.size() != 3)
    return false;
  if (!base::StringToInt(date_parts[0], &time_exploded.month))
    return false;
  if (!base::StringToInt(date_parts[1], &time_exploded.day_of_month))
    return false;
  if (!base::StringToInt(date_parts[2], &time_exploded.year))
    return false;
  if (time_exploded.year < 0)
    return false;
  // If year has only two digits then assume that 00-79 is 2000-2079,
  // and 80-99 is 1980-1999.
  if (time_exploded.year < 80)
    time_exploded.year += 2000;
  else if (time_exploded.year < 100)
    time_exploded.year += 1900;

  // Time should be in format HH:MM[(AM|PM)]
  if (time.length() < 5)
    return false;

  std::vector<base::StringPiece16> time_parts = base::SplitStringPiece(
      base::StringPiece16(time).substr(0, 5), base::ASCIIToUTF16(":"),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (time_parts.size() != 2)
    return false;
  if (!base::StringToInt(time_parts[0], &time_exploded.hour))
    return false;
  if (!base::StringToInt(time_parts[1], &time_exploded.minute))
    return false;
  if (!time_exploded.HasValidValues())
    return false;

  if (time.length() > 5) {
    if (time.length() != 7)
      return false;
    base::string16 am_or_pm(time.substr(5, 2));
    if (base::EqualsASCII(am_or_pm, "PM")) {
      if (time_exploded.hour < 12)
        time_exploded.hour += 12;
    } else if (base::EqualsASCII(am_or_pm, "AM")) {
      if (time_exploded.hour == 12)
        time_exploded.hour = 0;
    } else {
      return false;
    }
  }

  // We don't know the time zone of the server, so just use UTC.
  return base::Time::FromUTCExploded(time_exploded, result);
}

// static
base::string16 FtpUtil::GetStringPartAfterColumns(const base::string16& text,
                                                  int columns) {
  base::i18n::UTF16CharIterator iter(text);

  for (int i = 0; i < columns; i++) {
    // Skip the leading whitespace.
    while (!iter.end() && u_isspace(iter.get()))
      iter.Advance();

    // Skip the actual text of i-th column.
    while (!iter.end() && !u_isspace(iter.get()))
      iter.Advance();
  }

  base::string16 result(text.substr(iter.array_pos()));
  base::TrimWhitespace(result, base::TRIM_ALL, &result);
  return result;
}

}  // namespace net
