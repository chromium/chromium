// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_ls.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "net/ftp/ftp_util.h"

namespace net {

namespace {

bool TwoColumnDateListingToTime(const std::u16string& date,
                                const std::u16string& time,
                                base::Time* result) {
  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format YYYY-MM-DD.
  std::vector<std::u16string> date_parts = base::SplitString(
      date, u"-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (date_parts.size() != 3)
    return false;
  if (!base::StringToInt(date_parts[0], &time_exploded.year))
    return false;
  if (!base::StringToInt(date_parts[1], &time_exploded.month))
    return false;
  if (!base::StringToInt(date_parts[2], &time_exploded.day_of_month))
    return false;

  // Time should be in format HH:MM
  if (time.length() != 5)
    return false;

  std::vector<std::u16string> time_parts = base::SplitString(
      time, u":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (time_parts.size() != 2)
    return false;
  if (!base::StringToInt(time_parts[0], &time_exploded.hour))
    return false;
  if (!base::StringToInt(time_parts[1], &time_exploded.minute))
    return false;
  if (!time_exploded.HasValidValues())
    return false;

  // We don't know the time zone of the server, so just use UTC.
  return base::Time::FromUTCExploded(time_exploded, result);
}

// Returns the column index of the end of the date listing and detected
// last modification time.
bool DetectColumnOffsetSizeAndModificationTime(
    const std::vector<std::u16string>& columns,
    const base::Time& current_time,
    size_t* offset,
    std::u16string* size,
    base::Time* modification_time) {
  // The column offset can be arbitrarily large if some fields
  // like owner or group name contain spaces. Try offsets from left to right
  // and use the first one that matches a date listing.
  //
  // Here is how a listing line should look like. A star ("*") indicates
  // a required field:
  //
  //  * 1. permission listing
  //    2. number of links (optional)
  //  * 3. owner name (may contain spaces)
  //    4. group name (optional, may contain spaces)
  //  * 5. size in bytes
  //  * 6. month
  //  * 7. day of month
  //  * 8. year or time <-- column_offset will be the index of this column
  //    9. file name (optional, may contain spaces)
  for (size_t i = 5U; i < columns.size(); i++) {
    if (FtpUtil::LsDateListingToTime(columns[i - 2], columns[i - 1], columns[i],
                                     current_time, modification_time)) {
      *size = columns[i - 3];
      *offset = i;
      return true;
    }
  }

  // Some FTP listings have swapped the "month" and "day of month" columns
  // (for example Russian listings). We try to recognize them only after making
  // sure no column offset works above (this is a more strict way).
  for (size_t i = 5U; i < columns.size(); i++) {
    if (FtpUtil::LsDateListingToTime(columns[i - 1], columns[i - 2], columns[i],
                                     current_time, modification_time)) {
      *size = columns[i - 3];
      *offset = i;
      return true;
    }
  }

  // Some FTP listings use a different date format.
  for (size_t i = 5U; i < columns.size(); i++) {
    if (TwoColumnDateListingToTime(columns[i - 1],
                                   columns[i],
                                   modification_time)) {
      *size = columns[i - 2];
      *offset = i;
      return true;
    }
  }

  return false;
}

}  // namespace

bool ParseFtpDirectoryListingLs(
    const std::vector<std::u16string>& lines,
    const base::Time& current_time,
    std::vector<FtpDirectoryListingEntry>* entries) {
  // True after we have received a "total n" listing header, where n is an
  // integer. Only one such header is allowed per listing.
  bool received_total_line = false;

  for (size_t i = 0; i < lines.size(); i++) {
    if (lines[i].empty())
      continue;

    std::vector<std::u16string> columns =
        base::SplitString(base::CollapseWhitespace(lines[i], false), u" ",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    // Some FTP servers put a "total n" line at the beginning of the listing
    // (n is an integer). Allow such a line, but only once, and only if it's
    // the first non-empty line. Do not match the word exactly, because it may
    // be in different languages (at least English and German have been seen
    // in the field).
    if (columns.size() == 2 && !received_total_line) {
      received_total_line = true;

      // Some FTP servers incorrectly return a negative integer for "n". Since
      // this value is ignored anyway, just check any valid integer was
      // provided.
      int64_t total_number;
      if (!base::StringToInt64(columns[1], &total_number))
        return false;

      continue;
    }

    FtpDirectoryListingEntry entry;

    size_t column_offset;
    std::u16string size;
    if (!DetectColumnOffsetSizeAndModificationTime(columns,
                                                   current_time,
                                                   &column_offset,
                                                   &size,
                                                   &entry.last_modified)) {
      // Some servers send a message in one of the first few lines.
      // All those messages have in common is the string ".:",
      // where "." means the current directory, and ":" separates it
      // from the rest of the message, which may be empty.
      if (lines[i].find(u".:") != std::u16string::npos)
        continue;

      return false;
    }

    // Do not check "validity" of the permission listing. It's quirky,
    // and some servers send garbage here while other parts of the line are OK.

    if (!columns[0].empty() && columns[0][0] == 'l') {
      entry.type = FtpDirectoryListingEntry::SYMLINK;
    } else if (!columns[0].empty() && columns[0][0] == 'd') {
      entry.type = FtpDirectoryListingEntry::DIRECTORY;
    } else {
      entry.type = FtpDirectoryListingEntry::FILE;
    }

    if (!base::StringToInt64(size, &entry.size)) {
      // Some FTP servers do not separate owning group name from file size,
      // like "group1234". We still want to display the file name for that
      // entry, but can't really get the size (What if the group is named
      // "group1", and the size is in fact 234? We can't distinguish between
      // that and "group" with size 1234). Use a dummy value for the size.
      entry.size = -1;
    }
    if (entry.size < 0) {
      // Some FTP servers have bugs that cause them to display the file size
      // as negative. They're most likely big files like DVD ISO images.
      // We still want to display them, so just say the real file size
      // is unknown.
      entry.size = -1;
    }
    if (entry.type != FtpDirectoryListingEntry::FILE)
      entry.size = -1;

    if (column_offset == columns.size() - 1) {
      // If the end of the date listing is the last column, there is no file
      // name. Some FTP servers send listing entries with empty names.
      // It's not obvious how to display such an entry, so we ignore them.
      // We don't want to make the parsing fail at this point though.
      // Other entries can still be useful.
      continue;
    }

    entry.name = FtpUtil::GetStringPartAfterColumns(lines[i],
                                                    column_offset + 1);

    if (entry.type == FtpDirectoryListingEntry::SYMLINK) {
      std::u16string::size_type pos = entry.name.rfind(u" -> ");

      // We don't require the " -> " to be present. Some FTP servers don't send
      // the symlink target, possibly for security reasons.
      if (pos != std::u16string::npos)
        entry.name = entry.name.substr(0, pos);
    }

    entries->push_back(entry);
  }

  return true;
}

}  // namespace net
