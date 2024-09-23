// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/byte_string.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {

TEST(ByteStringTest, Test) {
  const std::string kCases[] = {
      "hello",                     // C-string
      {'\xEF', '\xB7', '\xAF'},    // invalid UTF-8
      {'h', '\0', 'w', 'd', 'y'},  // embedded null
  };
  for (size_t i = 0; i < std::size(kCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("case %" PRIuS, i));
    std::string out;
    EXPECT_TRUE(mojom::ByteString::Deserialize(
        mojom::ByteString::Serialize(&kCases[i]), &out));
    EXPECT_EQ(kCases[i], out);
  }
}

}  // namespace mojo_base
