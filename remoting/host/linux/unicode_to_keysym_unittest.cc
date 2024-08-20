// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/linux/unicode_to_keysym.h"

#include <stddef.h>
#include <stdint.h>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(GetKeySymsForUnicode, Map) {
  const static struct Test {
    uint32_t code_point;
    uint32_t expected_keysyms[4];
  } kTests[] = {
      {0x0040, {0x0040, 0x01000040, 0}},
      {0x00b0, {0x00b0, 0x010000b0, 0}},
      {0x0100, {0x03c0, 0x01000100, 0}},
      {0x038e, {0x07a8, 0x0100038e, 0}},
      {0x22a3, {0x0bdc, 0x010022a3, 0}},
      {0x30bd, {0x04bf, 0x010030bd, 0}},
      {0x3171, {0x0ef0, 0x01003171, 0}},
      {0x318e, {0x0ef7, 0x0100318e, 0}},

      // Characters with 3 distinct keysyms.
      {0x0030, {0x0030, 0xffb0, 0x01000030, 0}},
      {0x005f, {0x005f, 0x0bc6, 0x0100005f, 0}},

      // Characters for which there are no regular keysyms.
      {0x4444, {0x01004444, 0}},
  };

  for (size_t i = 0; i < std::size(kTests); ++i) {
    std::vector<uint32_t> keysyms = GetKeySymsForUnicode(kTests[i].code_point);

    std::vector<uint32_t> expected(
        kTests[i].expected_keysyms,
        base::ranges::find(kTests[i].expected_keysyms, 0u));
    EXPECT_EQ(expected, keysyms);
  }
}

}  // namespace remoting
