// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_windows.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "net/ftp/ftp_util.h"

namespace net {

bool ParseFtpDirectoryListingWindows(
    const std::vector<std::u16string>& lines,
    std::vector<FtpDirectoryListingEntry>* entries) {
  for (size_t i = 0; i < lines.size(); i++) {
    if (lines[i].empty())
      continue;

    std::vector<std::u16string> columns =
        base::SplitString(base::CollapseWhitespace(lines[i], false), u" ",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    // Every line of the listing consists of the following:
    //
    //   1. date
    //   2. time
    //   3. size in bytes (or "<DIR>" for directories)
    //   4. filename (may be empty or contain spaces)
    //
    // For now, make sure we have 1-3, and handle 4 later.
    if (columns.size() < 3)
      return false;

    FtpDirectoryListingEntry entry;
    if (base::EqualsASCII(columns[2], "<DIR>")) {
      entry.type = FtpDirectoryListingEntry::DIRECTORY;
      entry.size = -1;
    } else {
      entry.type = FtpDirectoryListingEntry::FILE;
      if (!base::StringToInt64(columns[2], &entry.size))
        return false;
      if (entry.size < 0)
        return false;
    }

    if (!FtpUtil::WindowsDateListingToTime(columns[0],
                                           columns[1],
                                           &entry.last_modified)) {
      return false;
    }

    entry.name = FtpUtil::GetStringPartAfterColumns(lines[i], 3);
    if (entry.name.empty()) {
      // Some FTP servers send listing entries with empty names.
      // It's not obvious how to display such an entry, so ignore them.
      // We don't want to make the parsing fail at this point though.
      // Other entries can still be useful.
      continue;
    }

    entries->push_back(entry);
  }

  return true;
}

}  // namespace net
