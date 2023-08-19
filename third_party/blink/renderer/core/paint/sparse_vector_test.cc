// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/sparse_vector.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class TestDataField {
 public:
  explicit TestDataField(int value) : value_(value) {}

  bool operator==(const TestDataField& other) const {
    return value_ == other.value_;
  }

  void set_value(int value) { value_ = value; }
  int value() const { return value_; }

 private:
  int value_;
};

enum class TestFieldId : unsigned {
  kFoo = 0,
  kBar = 1,
  kBaz = 2,
  kFive = 5,
  kBang = 20,
  kBoom = 31,

  kNumFields = kBoom + 1
};

using TestSparseVector =
    SparseVector<TestFieldId, std::unique_ptr<TestDataField>>;

class SparseVectorTest : public testing::Test {
  USING_FAST_MALLOC(SparseVectorTest);

 public:
  SparseVectorTest() : sparse_vector_(std::make_unique<TestSparseVector>()) {}

  void SetField(unsigned field_id, std::unique_ptr<TestDataField> field) {
    sparse_vector_->SetField(static_cast<TestFieldId>(field_id),
                             std::move(field));
  }

  bool HasField(unsigned field_id) {
    return sparse_vector_->HasField(static_cast<TestFieldId>(field_id));
  }

  TestDataField& GetField(unsigned field_id) {
    // Ensure that const and non-const accessors always agree.
    const auto fid = static_cast<TestFieldId>(field_id);
    EXPECT_EQ(
        (*sparse_vector_->GetField(fid)),
        *(const_cast<TestSparseVector*>(sparse_vector_.get())->GetField(fid)));

    return *(sparse_vector_->GetField(fid));
  }

  bool ClearField(unsigned field_id) {
    return sparse_vector_->ClearField(static_cast<TestFieldId>(field_id));
  }

  const TestSparseVector& sparse_vector() { return *sparse_vector_; }

  std::unique_ptr<TestSparseVector> sparse_vector_;
};

TEST_F(SparseVectorTest, Basic) {
  SetField(1, std::make_unique<TestDataField>(101));
  EXPECT_TRUE(HasField(1));
  EXPECT_FALSE(HasField(2));
  EXPECT_EQ(GetField(1).value(), 101);

  SetField(2, std::make_unique<TestDataField>(202));
  EXPECT_TRUE(HasField(1));
  EXPECT_EQ(GetField(1).value(), 101);
  EXPECT_TRUE(HasField(2));
  EXPECT_EQ(GetField(2).value(), 202);
}

TEST_F(SparseVectorTest, MemoryUsage) {
  // An empty vector should not use any memory.
  EXPECT_TRUE(sparse_vector().empty());
  EXPECT_EQ(0u, sparse_vector().size());
  EXPECT_EQ(0u, sparse_vector().capacity());

  // Instead of reserving 4 like WTF::Vector does by default, we want
  // to reserve two to save a little bit more memory.
  SetField(20, std::make_unique<TestDataField>(101));
  EXPECT_FALSE(sparse_vector().empty());
  EXPECT_EQ(1u, sparse_vector().size());
  EXPECT_EQ(2u, sparse_vector().capacity());

  SetField(31, std::make_unique<TestDataField>(202));
  EXPECT_FALSE(sparse_vector().empty());
  EXPECT_EQ(2u, sparse_vector().size());
  EXPECT_EQ(2u, sparse_vector().capacity());

  // After two elements, what happens next to the capacity of the vector is
  // platform dependent, so we don't verify capacity.
}

TEST_F(SparseVectorTest, SupportsLargerValues) {
  SetField(20, std::make_unique<TestDataField>(101));
  EXPECT_TRUE(HasField(20));
  EXPECT_FALSE(HasField(31));
  EXPECT_EQ(GetField(20).value(), 101);

  SetField(31, std::make_unique<TestDataField>(202));
  EXPECT_TRUE(HasField(20));
  EXPECT_EQ(GetField(20).value(), 101);
  EXPECT_TRUE(HasField(31));
  EXPECT_EQ(GetField(31).value(), 202);
}

TEST_F(SparseVectorTest, MutateValue) {
  SetField(1, std::make_unique<TestDataField>(101));
  EXPECT_EQ(GetField(1).value(), 101);
  SetField(1, std::make_unique<TestDataField>(202));
  EXPECT_EQ(GetField(1).value(), 202);
}

TEST_F(SparseVectorTest, ClearField) {
  SetField(1, std::make_unique<TestDataField>(101));
  SetField(2, std::make_unique<TestDataField>(202));
  EXPECT_EQ(GetField(1).value(), 101);
  EXPECT_EQ(GetField(2).value(), 202);

  // Should successfully remove the field.
  EXPECT_TRUE(ClearField(2));

  // Multiple clears should return false since the value is already empty.
  EXPECT_FALSE(ClearField(2));

  // The second field should be removed now.
  EXPECT_FALSE(HasField(2));

  // The other field should be unaffected.
  EXPECT_TRUE(HasField(1));
  EXPECT_EQ(GetField(1).value(), 101);
}

TEST_F(SparseVectorTest, SettingToNullptrMaintainsField) {
  EXPECT_FALSE(HasField(1));

  // In this context nullptr is not a special type, unfortunately.
  SetField(1, nullptr);
  EXPECT_TRUE(HasField(1));

  SetField(1, std::make_unique<TestDataField>(101));
  EXPECT_EQ(GetField(1).value(), 101);
  EXPECT_TRUE(HasField(1));

  // Since not all types representable as the field type of SparseVector are
  // convertible to falsy, setting to nullptr should keep the field alive. This
  // could be fixed by passing a predicate to the template or constructor for
  // SparseVector, however at this time it's overkill.
  SetField(1, nullptr);
  EXPECT_TRUE(HasField(1));

  // Should still be clearable.
  EXPECT_TRUE(ClearField(1));
  EXPECT_FALSE(HasField(1));
}

TEST_F(SparseVectorTest, DoesNotOverwriteFieldsWithSmallerIndices) {
  SetField(5, std::make_unique<TestDataField>(42));
  SetField(2, std::make_unique<TestDataField>(29));
  EXPECT_EQ(GetField(5).value(), 42);
  EXPECT_EQ(GetField(2).value(), 29);
}

TEST_F(SparseVectorTest, DoesNotOverwriteFieldsWithLargerIndices) {
  SetField(2, std::make_unique<TestDataField>(29));
  SetField(5, std::make_unique<TestDataField>(42));
  EXPECT_EQ(GetField(5).value(), 42);
  EXPECT_EQ(GetField(2).value(), 29);
}

}  // namespace blink
