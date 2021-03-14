// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_WINDOWS_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_WINDOWS_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace net {

struct FtpDirectoryListingEntry;

// Parses Windows FTP directory listing. Returns true on success.
NET_EXPORT_PRIVATE bool ParseFtpDirectoryListingWindows(
    const std::vector<std::u16string>& lines,
    std::vector<FtpDirectoryListingEntry>* entries);

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_WINDOWS_H_
