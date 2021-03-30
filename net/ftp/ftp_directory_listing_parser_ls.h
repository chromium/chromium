// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_LS_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_LS_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace base {
class Time;
}

namespace net {

struct FtpDirectoryListingEntry;

// Parses "ls -l" FTP directory listing. Returns true on success.
NET_EXPORT_PRIVATE bool ParseFtpDirectoryListingLs(
    const std::vector<std::u16string>& lines,
    const base::Time& current_time,
    std::vector<FtpDirectoryListingEntry>* entries);

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_LS_H_
