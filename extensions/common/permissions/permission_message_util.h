// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_UTIL_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_UTIL_H_

#include <set>
#include <string>

namespace extensions {
class URLPatternSet;
}

namespace permission_message_util {

std::set<std::string> GetDistinctHosts(
    const extensions::URLPatternSet& host_patterns,
    bool include_rcd,
    bool exclude_file_scheme);

}  // namespace permission_message_util

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_UTIL_H_
