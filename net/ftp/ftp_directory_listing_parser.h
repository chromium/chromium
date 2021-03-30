// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

struct FtpDirectoryListingEntry {
  enum Type {
    UNKNOWN,
    FILE,
    DIRECTORY,
    SYMLINK,
  };

  FtpDirectoryListingEntry();

  Type type;
  std::u16string name;   // Name (UTF-16-encoded).
  std::string raw_name;  // Name in original character encoding.
  int64_t size;          // File size, in bytes. -1 if not applicable.

  // Last modified time, in local time zone.
  base::Time last_modified;
};

// Parses an FTP directory listing |text|. On success fills in |entries|.
// Returns network error code.
NET_EXPORT int ParseFtpDirectoryListing(
    const std::string& text,
    const base::Time& current_time,
    std::vector<FtpDirectoryListingEntry>* entries);

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_H_
