// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_setting_overrides_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(CookieSettingOverridesMojomTraitsTest, SerializeAndDeserialize) {
  const net::CookieSettingOverrides keys[] = {
      {},
      {},
  };

  for (auto original : keys) {
    net::CookieSettingOverrides copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::CookieSettingOverrides>(
            original, copied));
    EXPECT_EQ(original, copied);
  }
}

}  // namespace network
