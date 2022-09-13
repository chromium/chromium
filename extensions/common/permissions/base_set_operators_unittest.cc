// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/base_set_operators.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class TestPermission {
 public:
  TestPermission(int id) : id_(id) {}

  TestPermission(const TestPermission&) = delete;
  TestPermission& operator=(const TestPermission&) = delete;

  ~TestPermission() = default;

  // Methods required by BaseSetOperators operations.
  std::unique_ptr<TestPermission> Clone() const {
    return std::make_unique<TestPermission>(id_);
  }
  std::unique_ptr<TestPermission> Diff() const { return nullptr; }
  std::unique_ptr<TestPermission> Union() const { return Clone(); }
  std::unique_ptr<TestPermission> Intersect() const { return Clone(); }
  bool Equal(const TestPermission* other) const { return id_ == other->id_; }

  int id() const { return id_; }

 private:
  int id_;
};

}  // namespace

class TestPermissionSet;

template <>
struct BaseSetOperatorsTraits<TestPermissionSet> {
  using ElementType = TestPermission;
  using ElementIDType = int;
};

class TestPermissionSet : public BaseSetOperators<TestPermissionSet> {};

TEST(BaseSetOperatorsTest, Basic) {
  TestPermissionSet set;
  set.insert(std::make_unique<TestPermission>(1));
  set.insert(std::make_unique<TestPermission>(2));
  set.insert(std::make_unique<TestPermission>(2));

  EXPECT_EQ(2u, set.size());
  EXPECT_EQ(1u, set.count(1));
  EXPECT_EQ(1u, set.count(2));
  EXPECT_EQ(0u, set.count(3));

  set.erase(1);
  EXPECT_EQ(1u, set.size());
  EXPECT_EQ(0u, set.count(1));
  EXPECT_EQ(1u, set.count(2));

  set.insert(std::make_unique<TestPermission>(1));
  EXPECT_EQ(2u, set.size());

  set.clear();
  EXPECT_EQ(0u, set.size());
  EXPECT_TRUE(set.empty());
}

TEST(BaseSetOperatorsTest, CopyCorrectness) {
  TestPermissionSet set1;
  set1.insert(std::make_unique<TestPermission>(1));
  set1.insert(std::make_unique<TestPermission>(2));

  TestPermissionSet set2 = set1.Clone();
  EXPECT_EQ(set1, set2);
  EXPECT_EQ(2u, set2.size());
  EXPECT_EQ(1u, set2.count(1));
  EXPECT_EQ(1u, set2.count(2));
  EXPECT_EQ(0u, set2.count(3));

  set2.insert(std::make_unique<TestPermission>(3));
  EXPECT_NE(set1, set2);
  EXPECT_EQ(3u, set2.size());
  EXPECT_EQ(1u, set2.count(3));

  // Assigning should clear the set (https://crbug.com/908619).
  set2 = set1.Clone();
  EXPECT_EQ(set1, set2);
  EXPECT_EQ(2u, set2.size());
  EXPECT_EQ(1u, set2.count(1));
  EXPECT_EQ(1u, set2.count(2));
  EXPECT_EQ(0u, set2.count(3));
}

// Validates that cloning the BaseSetOperators<T> (through various methods) does
// not re-use the underlying items in the set - i.e., it should be a "deep"
// copy.
// Regression test for https://crbug.com/908619.
TEST(BaseSetOperatorsTest, CloningDoesNotReuseItems) {
  TestPermissionSet set;
  set.insert(std::make_unique<TestPermission>(1));
  set.insert(std::make_unique<TestPermission>(2));

  auto validate = [](const TestPermissionSet& set1,
                     const TestPermissionSet& set2) {
    // The contents within the set should not have the same underlying elements.
    for (const auto* e1 : set1) {
      for (const auto* e2 : set2)
        EXPECT_NE(e1, e2) << e1->id();
    }
  };

  {
    SCOPED_TRACE("Assignment Operator");
    TestPermissionSet copy = set.Clone();
    validate(set, copy);
  }

  {
    SCOPED_TRACE("Assigned to Return Value");
    TestPermissionSet copy =
        ([&]() -> TestPermissionSet { return set.Clone(); })();
    validate(set, copy);
  }
}

// TODO(devlin): Add tests for union, diff, and intersection?

}  // namespace extensions
