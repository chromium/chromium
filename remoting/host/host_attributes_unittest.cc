// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_attributes.h"

#include <algorithm>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// Ensures there is no DCHECK failure or crash in Register() and Callbacks.
TEST(HostAttributesTest, Sanity) {
  std::vector<std::string> result = GetHostAttributes();
  bool is_debug_build = std::ranges::contains(result, "Debug-Build");

#if defined(NDEBUG)
  ASSERT_FALSE(is_debug_build);
#else
  ASSERT_TRUE(is_debug_build);
#endif
}

TEST(HostAttributesTest, NoDuplicateOrInvalidKeys) {
  std::vector<std::string> results = GetHostAttributes();
  ASSERT_FALSE(results.empty());
  for (auto it = results.begin(); it != results.end(); ++it) {
    EXPECT_FALSE(it->empty());
    EXPECT_FALSE(it->contains(','));
    ASSERT_EQ(std::find(it + 1, results.end(), *it), results.end());
  }
}

}  // namespace remoting
