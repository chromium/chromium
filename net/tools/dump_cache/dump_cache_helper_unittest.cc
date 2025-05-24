// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "net/tools/dump_cache/dump_cache_helper.h"

#include <cstring>

#include "testing/gtest/include/gtest/gtest.h"

TEST(DumpCacheHelper, HexDump) {
  std::string out = "Prefix\n";
  const char kIn[] = "0123456789ABCDEFGHIJ\x01\x80<&>";
  DumpCacheHelper::HexDump(kIn, strlen(kIn), &out);
  EXPECT_EQ(
      "Prefix\n00000000: 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46  "
      "0123456789ABCDEF\n00000010: 47 48 49 4a 01 80 3c 26 3e                  "
      "     GHIJ..&lt;&amp;&gt;\n",
      out);
}
