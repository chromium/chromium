// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/effective_connection_type.h"

#include <optional>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Tests that the effective connection type is converted correctly to a
// descriptive string name, and vice-versa.
TEST(EffectiveConnectionTypeTest, NameConnectionTypeConversion) {
  // Verify GetEffectiveConnectionTypeForName() returns an unset value when an
  // invalid effective connection type name is provided.
  EXPECT_FALSE(
      GetEffectiveConnectionTypeForName("InvalidEffectiveConnectionTypeName"));
  EXPECT_FALSE(GetEffectiveConnectionTypeForName(std::string()));

  for (size_t i = 0; i < EFFECTIVE_CONNECTION_TYPE_LAST; ++i) {
    const EffectiveConnectionType effective_connection_type =
        static_cast<EffectiveConnectionType>(i);
    std::string connection_type_name = std::string(
        GetNameForEffectiveConnectionType(effective_connection_type));
    EXPECT_FALSE(connection_type_name.empty());

    if (effective_connection_type != EFFECTIVE_CONNECTION_TYPE_SLOW_2G) {
      // For all effective connection types except Slow2G,
      // DeprecatedGetNameForEffectiveConnectionType should return the same
      // name as GetNameForEffectiveConnectionType.
      EXPECT_EQ(connection_type_name,
                DeprecatedGetNameForEffectiveConnectionType(
                    effective_connection_type));
    }

    EXPECT_EQ(effective_connection_type,
              GetEffectiveConnectionTypeForName(connection_type_name));
  }
}
// Tests that the Slow 2G effective connection type is converted correctly to a
// descriptive string name, and vice-versa.
TEST(EffectiveConnectionTypeTest, Slow2GTypeConversion) {
  // GetEffectiveConnectionTypeForName should return Slow2G as effective
  // connection type for both the deprecated and the current string
  // representation.
  std::optional<EffectiveConnectionType> type =
      GetEffectiveConnectionTypeForName("Slow2G");
  EXPECT_EQ(EFFECTIVE_CONNECTION_TYPE_SLOW_2G, type.value());

  type = GetEffectiveConnectionTypeForName("Slow-2G");
  EXPECT_EQ(EFFECTIVE_CONNECTION_TYPE_SLOW_2G, type.value());

  EXPECT_EQ("Slow-2G", std::string(GetNameForEffectiveConnectionType(
                           EFFECTIVE_CONNECTION_TYPE_SLOW_2G)));
  EXPECT_EQ("Slow2G", std::string(DeprecatedGetNameForEffectiveConnectionType(
                          EFFECTIVE_CONNECTION_TYPE_SLOW_2G)));
}

}  // namespace

}  // namespace net
