// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/scoped_mode_collection.h"

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

using ::testing::_;
using ::testing::DoAll;
using ::testing::WithArg;
using ::testing::WithoutArgs;

using MockModeChangedCallback = ::testing::StrictMock<
    base::MockCallback<ScopedModeCollection::OnModeChangedCallback>>;

// An action that evaluates an expectation that `collection` is or is not
// empty.
ACTION_P(ExpectCollectionModeEqualsArg, collection) {
  EXPECT_EQ(collection->accessibility_mode(), arg0);
}

class ScopedModeCollectionTest : public ::testing::Test {
 protected:
  ScopedModeCollectionTest() {
    // Set a default action on the callback to check the invariant that the
    // `new_mode` given to the callback equals the collection's notion of the
    // effective mode.
    ON_CALL(callback_, Run(_, _))
        .WillByDefault(
            WithArg<1>(ExpectCollectionModeEqualsArg(&collection())));
  }

  void SetUp() override {
    // The mode must be empty at construction.
    ASSERT_EQ(collection_->accessibility_mode(), ui::AXMode());
  }

  bool HasCollection() const { return collection_.has_value(); }
  void PrematurelyDestroyCollection() { collection_.reset(); }

  MockModeChangedCallback& callback() { return callback_; }
  ScopedModeCollection& collection() { return collection_.value(); }
  std::unique_ptr<ScopedAccessibilityMode>& lazy_scoped_mode() {
    return lazy_scoped_mode_;
  }

 private:
  // Must precede `collection_`, which holds its callback.
  MockModeChangedCallback callback_;

  // Must precede `collection_` so that it is destructed after it; see
  // `OutstandingScoper` below.
  std::unique_ptr<ScopedAccessibilityMode> lazy_scoped_mode_;

  std::optional<ScopedModeCollection> collection_{callback_.Get()};
};

// Tests the most basic use of adding/removing a scoper.
TEST_F(ScopedModeCollectionTest, SimpleAddRemove) {
  // The callback should be run twice: once when a scoper is added and again
  // when it is destroyed.
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(callback(), Run(ui::AXMode(), ui::kAXModeComplete));
    EXPECT_CALL(callback(), Run(ui::kAXModeComplete, ui::AXMode()));
  }

  auto scoped_mode = collection().Add(ui::kAXModeComplete);
}

// Tests multiple scopers perfectly nested.
TEST_F(ScopedModeCollectionTest, MultipleNested) {
  // The callback should be run for each addition/removal for nested scopers.
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(callback(), Run(ui::AXMode(), ui::kAXModeBasic));
    EXPECT_CALL(callback(), Run(ui::kAXModeBasic, ui::kAXModeComplete));
    EXPECT_CALL(callback(), Run(ui::kAXModeComplete, ui::kAXModeBasic));
    EXPECT_CALL(callback(), Run(ui::kAXModeBasic, ui::AXMode()));
  }

  auto outer_scoped_mode = collection().Add(ui::kAXModeBasic);
  ASSERT_EQ(collection().accessibility_mode(), ui::kAXModeBasic);
  auto inner_scoped_mode = collection().Add(ui::kAXModeComplete);
  ASSERT_EQ(collection().accessibility_mode(), ui::kAXModeComplete);
  inner_scoped_mode.reset();
  ASSERT_EQ(collection().accessibility_mode(), ui::kAXModeBasic);
  outer_scoped_mode.reset();
  ASSERT_EQ(collection().accessibility_mode(), ui::AXMode());
}

// Tests multiple scopers deleted out of order.
TEST_F(ScopedModeCollectionTest, MultipleNotNested) {
  // The callback should not be run when the first scoper is deleted since its
  // mode flags are a subset of the second.
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(callback(), Run(ui::AXMode(), ui::kAXModeBasic));
    EXPECT_CALL(callback(), Run(ui::kAXModeBasic, ui::kAXModeComplete));
    EXPECT_CALL(callback(), Run(ui::kAXModeComplete, ui::AXMode()));
  }

  auto first_scoped_mode = collection().Add(ui::kAXModeBasic);
  ASSERT_EQ(collection().accessibility_mode(), ui::kAXModeBasic);
  auto second_scoped_mode = collection().Add(ui::kAXModeComplete);
  ASSERT_EQ(collection().accessibility_mode(), ui::kAXModeComplete);
  first_scoped_mode.reset();
  ASSERT_EQ(collection().accessibility_mode(), ui::kAXModeComplete);
  second_scoped_mode.reset();
  ASSERT_EQ(collection().accessibility_mode(), ui::AXMode());
}

// Tests that deleting the collection while it holds a scoper should neither
// crash nor run the callback when the scoper is destroyed. (Note: by
// "outstanding", we mean that there remains a scoper alive. We are not making a
// judgement about this particular scoper being better in any way than another
// scoper. All scopers are equal in the transistors of the CPU.)
TEST_F(ScopedModeCollectionTest, OutstandingScoper) {
  // The callback should be run once when the scoper is added but not when it
  // is destroyed.
  EXPECT_CALL(callback(), Run(ui::AXMode(), ui::kAXModeComplete));

  // Make sure that `scoped_mode` outlives `collection`.
  lazy_scoped_mode() = collection().Add(ui::kAXModeComplete);
}

// An action that evaluates an expectation that `collection` is empty.
ACTION_P(ExpectCollectionIsEmpty, collection) {
  EXPECT_TRUE(collection->empty());
}

// An action that evaluates an expectation that `collection` is not empty.
ACTION_P(ExpectCollectionIsNotEmpty, collection) {
  EXPECT_FALSE(collection->empty());
}

// Tests that `empty()` works.
TEST_F(ScopedModeCollectionTest, Empty) {
  // Expect that `empty()` will return the right thing from within the callback.
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(callback(), Run(_, _))
        .WillOnce(
            DoAll(WithArg<1>(ExpectCollectionModeEqualsArg(&collection())),
                  WithoutArgs(ExpectCollectionIsNotEmpty(&collection()))));
    EXPECT_CALL(callback(), Run(_, _))
        .WillOnce(
            DoAll(WithArg<1>(ExpectCollectionModeEqualsArg(&collection())),
                  WithoutArgs(ExpectCollectionIsEmpty(&collection()))));
  }

  ASSERT_TRUE(collection().empty());
  auto scoped_mode = collection().Add(ui::kAXModeComplete);
  ASSERT_FALSE(collection().empty());
  scoped_mode.reset();
  ASSERT_TRUE(collection().empty());
}

// Tests that destroying a collection from within its callback does not crash,
// even if scopers are still alive.
TEST_F(ScopedModeCollectionTest, DestroyFromCallback) {
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(callback(), Run(_, _));
    EXPECT_CALL(callback(), Run(_, _));
    EXPECT_CALL(callback(), Run(_, _)).WillOnce(WithoutArgs([this]() {
      PrematurelyDestroyCollection();
    }));
  }

  ASSERT_TRUE(collection().empty());
  auto first_scoped_mode = collection().Add(ui::kAXModeBasic);
  auto second_scoped_mode = collection().Add(ui::kAXModeComplete);
  // The collection will be destroyed by the callback when the mode drops back
  // down to `kAXModeBasic`.
  second_scoped_mode.reset();
  ASSERT_FALSE(HasCollection());
}

}  // namespace content
