// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_attributes.h"

#include <algorithm>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// Ensures there is no DCHECK failure or crash in Register() and Callbacks.
TEST(HostAttributesTest, Sanity) {
  std::string result = GetHostAttributes();
  bool is_debug_build = base::Contains(result, "Debug-Build");

#if defined(NDEBUG)
  ASSERT_FALSE(is_debug_build);
#else
  ASSERT_TRUE(is_debug_build);
#endif
}

TEST(HostAttributesTest, NoDuplicateKeys) {
  std::vector<std::string> results = base::SplitString(
      GetHostAttributes(), ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (auto it = results.begin(); it != results.end(); it++) {
    ASSERT_EQ(std::find(it + 1, results.end(), *it), results.end());
  }
}

}  // namespace remoting
