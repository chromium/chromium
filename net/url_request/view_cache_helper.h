// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_VIEW_CACHE_HELPER_H_
#define NET_URL_REQUEST_VIEW_CACHE_HELPER_H_

#include <stddef.h>

#include <string>

#include "net/base/net_export.h"

namespace net {

class NET_EXPORT ViewCacheHelper {
 public:
  // Lower-level helper to produce a textual representation of binary data.
  // The results are appended to |result| and can be used in HTML pages
  // provided the dump is contained within <pre></pre> tags.
  static void HexDump(const char *buf, size_t buf_len, std::string* result);
};

}  // namespace net.

#endif  // NET_URL_REQUEST_VIEW_CACHE_HELPER_H_
