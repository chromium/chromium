// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
using testing::Contains;

TEST(PermissionTypeHelpersTest, AllPermissionTypesSmokeTest) {
  const auto all_permission_types = GetAllPermissionTypes();

  // All but PermissionType::NUM should be added.
  EXPECT_EQ(all_permission_types.size(),
            static_cast<unsigned long>(PermissionType::NUM) - 2);

  // Check that some arbitrary permission types are in this vector.
  // The order is not relevant.
  EXPECT_THAT(all_permission_types, Contains(PermissionType::MIDI_SYSEX));
  EXPECT_THAT(all_permission_types, Contains(PermissionType::WAKE_LOCK_SYSTEM));
  EXPECT_THAT(all_permission_types, Contains(PermissionType::GEOLOCATION));
  EXPECT_THAT(all_permission_types, Contains(PermissionType::SENSORS));
  EXPECT_THAT(all_permission_types, Contains(PermissionType::DURABLE_STORAGE));

  // PUSH_MESSAGING has been removed, and was =2.
  EXPECT_THAT(all_permission_types,
              testing::Not(Contains(static_cast<PermissionType>(2))));
}

}  // namespace

}  // namespace content
