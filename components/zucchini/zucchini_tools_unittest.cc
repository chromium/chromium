// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/zucchini_tools.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "components/zucchini/buildflags.h"
#include "components/zucchini/element_detection.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {
namespace {

TEST(ZucchiniToolsTest, PrintInfo) {
  std::ostringstream out;
  PrintInfo(out);

  const std::string output = out.str();
  ASSERT_FALSE(output.empty());
  ASSERT_EQ(output.back(), '\n');
  const std::vector<std::string_view> lines = base::SplitStringPiece(
      std::string_view(output).substr(0, output.size() - 1), "\n",
      base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  const base::span<const DisassemblerVersionInfo> disassemblers =
      GetSupportedDisassemblers();
  ASSERT_EQ(lines.size(), disassemblers.size() + 2U);

  EXPECT_EQ(lines[0],
            base::StringPrintf("Version: %u.%u", kMajorVersion, kMinorVersion));
  EXPECT_EQ(lines[1], "Supported architectures:");

  std::optional<size_t> default_col;
  size_t min_padding = std::string::npos;
  bool saw_noop = false;
  bool saw_ea64 = false;
  for (size_t i = 0; i < disassemblers.size(); ++i) {
    const auto& info = disassemblers[i];
    const std::string_view line = lines[i + 2U];
    const std::string prefix = "  " +
                               CastExecutableTypeToString(info.exe_type) +
                               "  " + std::string(info.name);
    ASSERT_EQ(line.substr(0, prefix.size()), prefix);

    // Verify column alignment of "default=" across all architecture lines,
    // with space-only padding and exact 2-space minimum padding for the
    // longest name.
    const size_t col = line.find("default=");
    ASSERT_NE(col, std::string::npos);
    ASSERT_GE(col, prefix.size() + 2U);
    const size_t padding = col - prefix.size();
    EXPECT_EQ(line.substr(prefix.size(), padding), std::string(padding, ' '));
    min_padding = std::min(min_padding, padding);
    if (!default_col.has_value()) {
      default_col = col;
    } else {
      EXPECT_EQ(col, *default_col);
    }

    // Intentional tripwire: update when bumping NoOp or AArch64 disassembler
    // versions.
    if (info.exe_type == kExeTypeNoOp) {
      saw_noop = true;
      EXPECT_EQ(line.substr(col), "default=1  gen=[1]  apply=[1]");
    } else if (info.exe_type == kExeTypeElfAArch64) {
      saw_ea64 = true;
      EXPECT_EQ(line.substr(col), "default=1  gen=[1]  apply=[1]");
    }
  }
  EXPECT_EQ(min_padding, 2U);
  EXPECT_TRUE(saw_noop);
  EXPECT_EQ(saw_ea64, static_cast<bool>(BUILDFLAG(ENABLE_ELF)));
}

}  // namespace
}  // namespace zucchini
