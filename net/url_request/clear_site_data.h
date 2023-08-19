// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_CLEAR_SITE_DATA_H_
#define NET_URL_REQUEST_CLEAR_SITE_DATA_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace net {

NET_EXPORT extern const char kClearSiteDataHeader[];

NET_EXPORT extern const char kDatatypeWildcard[];
NET_EXPORT extern const char kDatatypeCookies[];
NET_EXPORT extern const char kDatatypeStorage[];
NET_EXPORT extern const char kDatatypeStorageBucketPrefix[];
NET_EXPORT extern const char kDatatypeStorageBucketSuffix[];
NET_EXPORT extern const char kDatatypeCache[];
NET_EXPORT extern const char kDatatypeClientHints[];

NET_EXPORT std::vector<std::string> ClearSiteDataHeaderContents(
    std::string header);

}  // namespace net

#endif  // NET_URL_REQUEST_CLEAR_SITE_DATA_H_
