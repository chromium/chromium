// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_flags_to_string.h"

#include <bit>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"

namespace net {

namespace {

struct LoadFlagInfo {
  std::string_view name;
  int value;
};

constexpr LoadFlagInfo kInfo[] = {

#define LOAD_FLAG(label, value) {#label, value},
#include "net/base/load_flags_list.h"
#undef LOAD_FLAG

};

std::string AddLoadPrefix(std::string_view suffix) {
  return base::StrCat({"LOAD_", suffix});
}

}  // namespace

std::string LoadFlagsToString(int load_flags) {
  if (load_flags == 0) {
    static_assert(std::size(kInfo) > 0, "The kInfo array must be non-empty");
    static_assert(kInfo[0].value == 0, "The first entry should be LOAD_NORMAL");
    return AddLoadPrefix(kInfo[0].name);
  }

  const size_t expected_size =
      static_cast<size_t>(std::popcount(static_cast<uint32_t>(load_flags)));
  CHECK_GT(expected_size, 0u);
  CHECK_LE(expected_size, 33u);
  std::vector<std::string_view> flag_names;
  flag_names.reserve(expected_size);
  // Skip the first entry in kInfo as including LOAD_NORMAL in the output would
  // be confusing.
  for (const auto& flag : base::span(kInfo).subspan<1>()) {
    if (load_flags & flag.value) {
      flag_names.push_back(flag.name);
    }
  }
  CHECK_EQ(expected_size, flag_names.size());

  return AddLoadPrefix(base::JoinString(flag_names, " | LOAD_"));
}

}  // namespace net
