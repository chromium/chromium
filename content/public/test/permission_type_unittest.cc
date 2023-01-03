// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

namespace {
using testing::Contains;

TEST(PermissionTypeHelpersTest, AllPermissionTypesSmokeTest) {
  const auto all_permission_types = blink::GetAllPermissionTypes();

  // All but five removed permissions and PermissionType::NUM should be added.
  EXPECT_EQ(all_permission_types.size(),
            static_cast<unsigned long>(blink::PermissionType::NUM) - 6);

  // Check that some arbitrary permission types are in this vector.
  // The order is not relevant.
  EXPECT_THAT(all_permission_types,
              Contains(blink::PermissionType::MIDI_SYSEX));
  EXPECT_THAT(all_permission_types,
              Contains(blink::PermissionType::WAKE_LOCK_SYSTEM));
  EXPECT_THAT(all_permission_types,
              Contains(blink::PermissionType::GEOLOCATION));
  EXPECT_THAT(all_permission_types, Contains(blink::PermissionType::SENSORS));
  EXPECT_THAT(all_permission_types,
              Contains(blink::PermissionType::DURABLE_STORAGE));

  // PUSH_MESSAGING has been removed, and was =2.
  EXPECT_THAT(all_permission_types,
              testing::Not(Contains(static_cast<blink::PermissionType>(2))));
}

}  // namespace

}  // namespace content
