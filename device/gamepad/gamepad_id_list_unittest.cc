// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_id_list.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(GamepadIdTest, NoDuplicateIds) {
  // Each vendor/product ID pair and each gamepad ID should appear only once in
  // the table of known gamepads. This ensures that there are no duplicates, and
  // also that no two known devices map to the same ID.
  const auto gamepads = GamepadIdList::Get().GetGamepadListForTesting();
  std::unordered_set<uint32_t> seen_vendor_product_ids;
  std::unordered_set<GamepadId> seen_gamepad_ids;
  for (const auto& item : gamepads) {
    uint16_t vendor = std::get<0>(item);
    uint16_t product = std::get<1>(item);
    uint32_t vendor_product_id = (vendor << 16) | product;
    GamepadId gamepad_id =
        GamepadIdList::Get().GetGamepadId(std::string_view(), vendor, product);
    seen_vendor_product_ids.insert(vendor_product_id);
    seen_gamepad_ids.insert(gamepad_id);
    EXPECT_NE(gamepad_id, GamepadId::kUnknownGamepad);
  }
  EXPECT_EQ(seen_vendor_product_ids.size(), gamepads.size());
  EXPECT_EQ(seen_gamepad_ids.size(), gamepads.size());
}

}  // namespace device
