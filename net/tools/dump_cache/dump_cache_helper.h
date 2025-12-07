// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_DUMP_CACHE_DUMP_CACHE_HELPER_H_
#define NET_TOOLS_DUMP_CACHE_DUMP_CACHE_HELPER_H_

#include <stddef.h>

#include <string>

#include "base/containers/span.h"

class DumpCacheHelper {
 public:
  // Lower-level helper to produce a textual representation of binary data.
  // The results are appended to |result| and can be used in HTML pages
  // provided the dump is contained within <pre></pre> tags.
  static void HexDump(base::span<const uint8_t> buf, std::string* result);
};

#endif  // NET_TOOLS_DUMP_CACHE_DUMP_CACHE_HELPER_H_
