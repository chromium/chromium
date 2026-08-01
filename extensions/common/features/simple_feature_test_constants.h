// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_TEST_CONSTANTS_H_
#define EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_TEST_CONSTANTS_H_

#include <string_view>

namespace extensions {

inline constexpr char kFooId[] = "fooabbbbccccddddeeeeffffgggghhhh";
inline constexpr char kBarId[] = "barabbbbccccddddeeeeffffgggghhhh";

// echo -n "fooabbbbccccddddeeeeffffgggghhhh" |
//   sha1sum | tr '[:lower:]' '[:upper:]'
// SHA1 of kFooId.
inline constexpr std::string_view kHashedFooId =
    "55BC7228A0D502A2A48C9BB16B07062A01E62897";
// SHA1 of kBarId.
inline constexpr std::string_view kHashedBarId =
    "36FE24A48CFCCE317359BF43AA66F35624ECD356";

static_assert(kHashedFooId.size() == 40);
static_assert(kHashedBarId.size() == 40);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_TEST_CONSTANTS_H_
