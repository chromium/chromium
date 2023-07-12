// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/clear_site_data.h"
#include "base/strings/string_split.h"

namespace net {

const char kClearSiteDataHeader[] = "Clear-Site-Data";

const char kDatatypeWildcard[] = "\"*\"";
const char kDatatypeCookies[] = "\"cookies\"";
const char kDatatypeStorage[] = "\"storage\"";
const char kDatatypeStorageBucketPrefix[] = "\"storage:";
const char kDatatypeStorageBucketSuffix[] = "\"";
const char kDatatypeCache[] = "\"cache\"";
const char kDatatypeClientHints[] = "\"clientHints\"";

std::vector<std::string> ClearSiteDataHeaderContents(std::string header) {
  return base::SplitString(header, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace net
