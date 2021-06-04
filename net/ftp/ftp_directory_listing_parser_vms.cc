// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_vms.h"

#include <vector>

#include "base/cxx17_backports.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "net/ftp/ftp_util.h"

namespace net {

namespace {

// Converts the filename component in listing to the filename we can display.
// Returns true on success.
bool ParseVmsFilename(const std::u16string& raw_filename,
                      std::u16string* parsed_filename,
                      FtpDirectoryListingEntry::Type* type) {
  // On VMS, the files and directories are versioned. The version number is
  // separated from the file name by a semicolon. Example: ANNOUNCE.TXT;2.
  std::vector<std::u16string> listing_parts = base::SplitString(
      raw_filename, u";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (listing_parts.size() != 2)
    return false;
  int version_number;
  if (!base::StringToInt(listing_parts[1], &version_number))
    return false;
  if (version_number < 0)
    return false;

  // Even directories have extensions in the listings. Don't display extensions
  // for directories; it's awkward for non-VMS users. Also, VMS is
  // case-insensitive, but generally uses uppercase characters. This may look
  // awkward, so we convert them to lower case.
  std::vector<std::u16string> filename_parts = base::SplitString(
      listing_parts[0], u".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (filename_parts.size() != 2)
    return false;
  if (base::EqualsASCII(filename_parts[1], "DIR")) {
    *parsed_filename = base::ToLowerASCII(filename_parts[0]);
    *type = FtpDirectoryListingEntry::DIRECTORY;
  } else {
    *parsed_filename = base::ToLowerASCII(listing_parts[0]);
    *type = FtpDirectoryListingEntry::FILE;
  }
  return true;
}

// VMS's directory listing gives file size in blocks. The exact file size is
// unknown both because it is measured in blocks, but also because the block
// size is unknown (but assumed to be 512 bytes).
bool ApproximateFilesizeFromBlockCount(int64_t num_blocks, int64_t* out_size) {
  if (num_blocks < 0)
    return false;

  const int kBlockSize = 512;
  base::CheckedNumeric<int64_t> num_bytes = num_blocks;
  num_bytes *= kBlockSize;

  if (!num_bytes.IsValid())
    return false;  // Block count is too large.

  *out_size = num_bytes.ValueOrDie();
  return true;
}

bool ParseVmsFilesize(const std::u16string& input, int64_t* size) {
  if (base::ContainsOnlyChars(input, u"*")) {
    // Response consisting of asterisks means unknown size.
    *size = -1;
    return true;
  }

  int64_t num_blocks;
  if (base::StringToInt64(input, &num_blocks))
    return ApproximateFilesizeFromBlockCount(num_blocks, size);

  std::vector<base::StringPiece16> parts = base::SplitStringPiece(
      input, u"/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2)
    return false;

  int64_t blocks_used, blocks_allocated;
  if (!base::StringToInt64(parts[0], &blocks_used))
    return false;
  if (!base::StringToInt64(parts[1], &blocks_allocated))
    return false;
  if (blocks_used > blocks_allocated)
    return false;
  if (blocks_used < 0 || blocks_allocated < 0)
    return false;

  return ApproximateFilesizeFromBlockCount(blocks_used, size);
}

bool LooksLikeVmsFileProtectionListingPart(const std::u16string& input) {
  if (input.length() > 4)
    return false;

  // On VMS there are four different permission bits: Read, Write, Execute,
  // and Delete. They appear in that order in the permission listing.
  std::string pattern("RWED");
  std::u16string match(input);
  while (!match.empty() && !pattern.empty()) {
    if (match[0] == pattern[0])
      match = match.substr(1);
    pattern = pattern.substr(1);
  }
  return match.empty();
}

bool LooksLikeVmsFileProtectionListing(const std::u16string& input) {
  if (input.length() < 2)
    return false;
  if (input.front() != '(' || input.back() != ')')
    return false;

  // We expect four parts of the file protection listing: for System, Owner,
  // Group, and World.
  std::vector<std::u16string> parts = base::SplitString(
      base::StringPiece16(input).substr(1, input.length() - 2), u",",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 4)
    return false;

  return LooksLikeVmsFileProtectionListingPart(parts[0]) &&
      LooksLikeVmsFileProtectionListingPart(parts[1]) &&
      LooksLikeVmsFileProtectionListingPart(parts[2]) &&
      LooksLikeVmsFileProtectionListingPart(parts[3]);
}

bool LooksLikeVmsUserIdentificationCode(const std::u16string& input) {
  if (input.length() < 2)
    return false;
  return input.front() == '[' && input.back() == ']';
}

bool LooksLikeVMSError(const std::u16string& text) {
  static const char* const kPermissionDeniedMessages[] = {
    "%RMS-E-FNF",  // File not found.
    "%RMS-E-PRV",  // Access denied.
    "%SYSTEM-F-NOPRIV",
    "privilege",
  };

  for (size_t i = 0; i < base::size(kPermissionDeniedMessages); i++) {
    if (text.find(base::ASCIIToUTF16(kPermissionDeniedMessages[i])) !=
        std::u16string::npos)
      return true;
  }

  return false;
}

bool VmsDateListingToTime(const std::vector<std::u16string>& columns,
                          base::Time* time) {
  DCHECK_EQ(4U, columns.size());

  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format DD-MMM-YYYY.
  std::vector<base::StringPiece16> date_parts = base::SplitStringPiece(
      columns[2], u"-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (date_parts.size() != 3)
    return false;
  if (!base::StringToInt(date_parts[0], &time_exploded.day_of_month))
    return false;
  if (!FtpUtil::AbbreviatedMonthToNumber(std::u16string(date_parts[1]),
                                         &time_exploded.month))
    return false;
  if (!base::StringToInt(date_parts[2], &time_exploded.year))
    return false;

  // Time can be in format HH:MM, HH:MM:SS, or HH:MM:SS.mm. Try to recognize the
  // last type first. Do not parse the seconds, they will be ignored anyway.
  std::u16string time_column(columns[3]);
  if (time_column.length() == 11 && time_column[8] == '.')
    time_column = time_column.substr(0, 8);
  if (time_column.length() == 8 && time_column[5] == ':')
    time_column = time_column.substr(0, 5);
  if (time_column.length() != 5)
    return false;
  std::vector<base::StringPiece16> time_parts = base::SplitStringPiece(
      time_column, u":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (time_parts.size() != 2)
    return false;
  if (!base::StringToInt(time_parts[0], &time_exploded.hour))
    return false;
  if (!base::StringToInt(time_parts[1], &time_exploded.minute))
    return false;

  // We don't know the time zone of the server, so just use UTC.
  return base::Time::FromUTCExploded(time_exploded, time);
}

}  // namespace

bool ParseFtpDirectoryListingVms(
    const std::vector<std::u16string>& lines,
    std::vector<FtpDirectoryListingEntry>* entries) {
  // The first non-empty line is the listing header. It often
  // starts with "Directory ", but not always. We set a flag after
  // seing the header.
  bool seen_header = false;

  // Sometimes the listing doesn't end with a "Total" line, but
  // it's only okay when it contains some errors (it's needed
  // to distinguish it from "ls -l" format).
  bool seen_error = false;

  std::u16string total_of = u"Total of ";
  char16_t space[2] = {' ', 0};
  for (size_t i = 0; i < lines.size(); i++) {
    if (lines[i].empty())
      continue;

    if (base::StartsWith(lines[i], total_of, base::CompareCase::SENSITIVE)) {
      // After the "total" line, all following lines must be empty.
      for (size_t j = i + 1; j < lines.size(); j++)
        if (!lines[j].empty())
          return false;

      return true;
    }

    if (!seen_header) {
      seen_header = true;
      continue;
    }

    if (LooksLikeVMSError(lines[i])) {
      seen_error = true;
      continue;
    }

    std::vector<std::u16string> columns =
        base::SplitString(base::CollapseWhitespace(lines[i], false), space,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    if (columns.size() == 1) {
      // There can be no continuation if the current line is the last one.
      if (i == lines.size() - 1)
        return false;

      // Skip the next line.
      i++;

      // This refers to the continuation line.
      if (LooksLikeVMSError(lines[i])) {
        seen_error = true;
        continue;
      }

      // Join the current and next line and split them into columns.
      columns = base::SplitString(
          base::CollapseWhitespace(
              lines[i - 1] + space + lines[i], false),
          space, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    }

    if (columns.empty())
      return false;

    FtpDirectoryListingEntry entry;
    if (!ParseVmsFilename(columns[0], &entry.name, &entry.type))
      return false;

    // There are different variants of a VMS listing. Some display
    // the protection listing and user identification code, some do not.
    if (columns.size() == 6) {
      if (!LooksLikeVmsFileProtectionListing(columns[5]))
        return false;
      if (!LooksLikeVmsUserIdentificationCode(columns[4]))
        return false;

      // Drop the unneeded data, so that the following code can always expect
      // just four columns.
      columns.resize(4);
    }

    if (columns.size() != 4)
      return false;

    if (!ParseVmsFilesize(columns[1], &entry.size))
      return false;
    if (entry.type != FtpDirectoryListingEntry::FILE)
      entry.size = -1;
    if (!VmsDateListingToTime(columns, &entry.last_modified))
      return false;

    entries->push_back(entry);
  }

  // The only place where we return true is after receiving the "Total" line,
  // that should be present in every VMS listing. Alternatively, if the listing
  // contains error messages, it's OK not to have the "Total" line.
  return seen_error;
}

}  // namespace net
