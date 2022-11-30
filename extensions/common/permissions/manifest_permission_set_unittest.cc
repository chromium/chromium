// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/manifest_permission_set.h"

#include "base/pickle.h"
#include "base/values.h"
#include "extensions/common/permissions/mock_manifest_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ManifestPermissionSetTest, General) {
  ManifestPermissionSet set;
  set.insert(std::make_unique<MockManifestPermission>("p1"));
  set.insert(std::make_unique<MockManifestPermission>("p2"));
  set.insert(std::make_unique<MockManifestPermission>("p3"));
  set.insert(std::make_unique<MockManifestPermission>("p4"));
  set.insert(std::make_unique<MockManifestPermission>("p5"));

  EXPECT_EQ(set.find("p1")->id(), "p1");
  EXPECT_TRUE(set.find("p10") == set.end());

  EXPECT_EQ(set.size(), 5u);

  EXPECT_EQ(set.erase("p1"), 1u);
  EXPECT_EQ(set.size(), 4u);

  EXPECT_EQ(set.erase("p1"), 0u);
  EXPECT_EQ(set.size(), 4u);
}

TEST(ManifestPermissionSetTest, CreateUnion) {
  ManifestPermissionSet permissions1;
  ManifestPermissionSet permissions2;
  ManifestPermissionSet expected_permissions;
  ManifestPermissionSet result;

  auto permission = std::make_unique<MockManifestPermission>("p3");

  // Union with an empty set.
  permissions1.insert(std::make_unique<MockManifestPermission>("p1"));
  permissions1.insert(std::make_unique<MockManifestPermission>("p2"));
  permissions1.insert(permission->Clone());
  expected_permissions.insert(std::make_unique<MockManifestPermission>("p1"));
  expected_permissions.insert(std::make_unique<MockManifestPermission>("p2"));
  expected_permissions.insert(std::move(permission));

  ManifestPermissionSet::Union(permissions1, permissions2, &result);

  EXPECT_TRUE(permissions1.Contains(permissions2));
  EXPECT_TRUE(permissions1.Contains(result));
  EXPECT_FALSE(permissions2.Contains(permissions1));
  EXPECT_FALSE(permissions2.Contains(result));
  EXPECT_TRUE(result.Contains(permissions1));
  EXPECT_TRUE(result.Contains(permissions2));

  EXPECT_EQ(expected_permissions, result);

  // Now use a real second set.
  permissions2.insert(std::make_unique<MockManifestPermission>("p1"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p2"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p33"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p4"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p5"));

  expected_permissions.insert(std::make_unique<MockManifestPermission>("p1"));
  expected_permissions.insert(std::make_unique<MockManifestPermission>("p2"));
  expected_permissions.insert(std::make_unique<MockManifestPermission>("p3"));
  expected_permissions.insert(std::make_unique<MockManifestPermission>("p4"));
  expected_permissions.insert(std::make_unique<MockManifestPermission>("p5"));
  expected_permissions.insert(std::make_unique<MockManifestPermission>("p33"));

  ManifestPermissionSet::Union(permissions1, permissions2, &result);

  {
    ManifestPermissionSet set1;
    set1.insert(std::make_unique<MockManifestPermission>("p1"));
    set1.insert(std::make_unique<MockManifestPermission>("p2"));
    ManifestPermissionSet set2;
    set2.insert(std::make_unique<MockManifestPermission>("p3"));

    EXPECT_FALSE(set1.Contains(set2));
    EXPECT_FALSE(set2.Contains(set1));
  }

  EXPECT_FALSE(permissions1.Contains(permissions2));
  EXPECT_FALSE(permissions1.Contains(result));
  EXPECT_FALSE(permissions2.Contains(permissions1));
  EXPECT_FALSE(permissions2.Contains(result));
  EXPECT_TRUE(result.Contains(permissions1));
  EXPECT_TRUE(result.Contains(permissions2));

  EXPECT_EQ(expected_permissions, result);
}

TEST(ManifestPermissionSetTest, CreateIntersection) {
  ManifestPermissionSet permissions1;
  ManifestPermissionSet permissions2;
  ManifestPermissionSet expected_permissions;
  ManifestPermissionSet result;

  // Intersection with an empty set.
  permissions1.insert(std::make_unique<MockManifestPermission>("p1"));
  permissions1.insert(std::make_unique<MockManifestPermission>("p2"));
  permissions1.insert(std::make_unique<MockManifestPermission>("p3"));

  ManifestPermissionSet::Intersection(permissions1, permissions2, &result);
  EXPECT_TRUE(permissions1.Contains(result));
  EXPECT_TRUE(permissions2.Contains(result));
  EXPECT_TRUE(permissions1.Contains(permissions2));
  EXPECT_FALSE(permissions2.Contains(permissions1));
  EXPECT_FALSE(result.Contains(permissions1));
  EXPECT_TRUE(result.Contains(permissions2));

  EXPECT_TRUE(result.empty());
  EXPECT_EQ(expected_permissions, result);

  // Now use a real second set.
  permissions2.insert(std::make_unique<MockManifestPermission>("p1"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p3"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p4"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p5"));

  expected_permissions.insert(std::make_unique<MockManifestPermission>("p1"));
  expected_permissions.insert(std::make_unique<MockManifestPermission>("p3"));

  ManifestPermissionSet::Intersection(permissions1, permissions2, &result);

  EXPECT_TRUE(permissions1.Contains(result));
  EXPECT_TRUE(permissions2.Contains(result));
  EXPECT_FALSE(permissions1.Contains(permissions2));
  EXPECT_FALSE(permissions2.Contains(permissions1));
  EXPECT_FALSE(result.Contains(permissions1));
  EXPECT_FALSE(result.Contains(permissions2));

  EXPECT_EQ(expected_permissions, result);
}

TEST(ManifestPermissionSetTest, CreateDifference) {
  ManifestPermissionSet permissions1;
  ManifestPermissionSet permissions2;
  ManifestPermissionSet expected_permissions;
  ManifestPermissionSet result;

  // Difference with an empty set.
  permissions1.insert(std::make_unique<MockManifestPermission>("p1"));
  permissions1.insert(std::make_unique<MockManifestPermission>("p2"));
  permissions1.insert(std::make_unique<MockManifestPermission>("p3"));

  ManifestPermissionSet::Difference(permissions1, permissions2, &result);

  EXPECT_EQ(permissions1, result);

  // Now use a real second set.
  permissions2.insert(std::make_unique<MockManifestPermission>("p1"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p2"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p4"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p5"));
  permissions2.insert(std::make_unique<MockManifestPermission>("p6"));

  expected_permissions.insert(std::make_unique<MockManifestPermission>("p3"));

  ManifestPermissionSet::Difference(permissions1, permissions2, &result);

  EXPECT_TRUE(permissions1.Contains(result));
  EXPECT_FALSE(permissions2.Contains(result));

  EXPECT_EQ(expected_permissions, result);

  // |result| = |permissions1| - |permissions2| -->
  //   |result| intersect |permissions2| == empty_set
  ManifestPermissionSet result2;
  ManifestPermissionSet::Intersection(result, permissions2, &result2);
  EXPECT_TRUE(result2.empty());
}

}  // namespace extensions
