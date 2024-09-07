// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/sparse_vector.h"

#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {
namespace {

enum class FieldId {
  kFirst = 0,
  kFoo = 1,
  kBar = 2,
  kFive = 5,
  kBang = 20,
  kLast = 31,

  kNumFields = kLast + 1
};

struct IntFieldTest {
  SparseVector<FieldId, int> sparse_vector;

  using Handle = std::unique_ptr<IntFieldTest>;
  static Handle MakeTest() { return std::make_unique<IntFieldTest>(); }

  static int CreateField(int value) { return value; }
  static int GetValue(int field) { return field; }
};

struct UniquePtrFieldTest {
 public:
  SparseVector<FieldId, std::unique_ptr<int>> sparse_vector;

  using Handle = std::unique_ptr<UniquePtrFieldTest>;
  static Handle MakeTest() { return std::make_unique<UniquePtrFieldTest>(); }

  static std::unique_ptr<int> CreateField(int value) {
    return std::make_unique<int>(value);
  }
  static int GetValue(const std::unique_ptr<int>& field) { return *field; }
};

struct TraceableFieldTest : GarbageCollected<TraceableFieldTest> {
 public:
  struct Traceable {
    int value;
    void Trace(Visitor*) const {}
    DISALLOW_NEW();
  };
  SparseVector<FieldId, Traceable> sparse_vector;
  void Trace(Visitor* visitor) const { visitor->Trace(sparse_vector); }

  using Handle = Persistent<TraceableFieldTest>;
  static TraceableFieldTest* MakeTest() {
    return MakeGarbageCollected<TraceableFieldTest>();
  }

  static Traceable CreateField(int value) { return Traceable{value}; }
  static int GetValue(const Traceable& field) { return field.value; }
};

struct MemberFieldTest : GarbageCollected<MemberFieldTest> {
 public:
  struct GCObject : public GarbageCollected<GCObject> {
    explicit GCObject(int value) : value(value) {}
    int value;
    void Trace(Visitor*) const {}
  };
  SparseVector<FieldId, Member<GCObject>> sparse_vector;
  void Trace(Visitor* visitor) const { visitor->Trace(sparse_vector); }

  using Handle = Persistent<MemberFieldTest>;
  static MemberFieldTest* MakeTest() {
    return MakeGarbageCollected<MemberFieldTest>();
  }

  static GCObject* CreateField(int value) {
    return MakeGarbageCollected<GCObject>(value);
  }
  static int GetValue(const Member<GCObject>& field) { return field->value; }
};

template <typename TestType>
class SparseVectorTest : public testing::Test {
 protected:
  void SetField(FieldId field_id, int value) {
    sparse_vector().SetField(field_id, TestType::CreateField(value));
  }

  int GetField(FieldId field_id) {
    return TestType::GetValue(sparse_vector().GetField(field_id));
  }

  bool EraseField(FieldId field_id) {
    return sparse_vector().EraseField(field_id);
  }

  void CheckHasFields(std::initializer_list<FieldId> field_ids) {
    for (auto id = FieldId::kFirst; id <= FieldId::kLast;
         id = static_cast<FieldId>(static_cast<unsigned>(id) + 1)) {
      EXPECT_EQ(std::count(field_ids.begin(), field_ids.end(), id),
                sparse_vector().HasField(id))
          << static_cast<unsigned>(id);
    }
  }

  auto& sparse_vector() { return test_->sparse_vector; }

 private:
  typename TestType::Handle test_ = TestType::MakeTest();
};

using TestTypes = ::testing::Types<IntFieldTest,
                                   UniquePtrFieldTest,
                                   TraceableFieldTest,
                                   MemberFieldTest>;
TYPED_TEST_SUITE(SparseVectorTest, TestTypes);

TYPED_TEST(SparseVectorTest, Basic) {
  this->CheckHasFields({});

  this->SetField(FieldId::kFoo, 101);
  this->CheckHasFields({FieldId::kFoo});
  EXPECT_EQ(101, this->GetField(FieldId::kFoo));

  this->SetField(FieldId::kBar, 202);
  this->CheckHasFields({FieldId::kFoo, FieldId::kBar});
  EXPECT_EQ(202, this->GetField(FieldId::kBar));

  this->sparse_vector().clear();
  this->CheckHasFields({});
}

TYPED_TEST(SparseVectorTest, MemoryUsage) {
  // An empty vector should not use any memory.
  EXPECT_TRUE(this->sparse_vector().empty());
  EXPECT_EQ(0u, this->sparse_vector().size());
  EXPECT_EQ(0u, this->sparse_vector().capacity());

  this->SetField(FieldId::kBang, 101);
  EXPECT_FALSE(this->sparse_vector().empty());
  EXPECT_EQ(1u, this->sparse_vector().size());
  EXPECT_GE(this->sparse_vector().capacity(), 1u);

  this->SetField(FieldId::kLast, 202);
  EXPECT_FALSE(this->sparse_vector().empty());
  EXPECT_EQ(2u, this->sparse_vector().size());
  EXPECT_GE(this->sparse_vector().capacity(), 2u);

  this->sparse_vector().reserve(10);
  EXPECT_FALSE(this->sparse_vector().empty());
  EXPECT_EQ(2u, this->sparse_vector().size());
  EXPECT_GE(this->sparse_vector().capacity(), 10u);
}

TYPED_TEST(SparseVectorTest, FirstAndLastValues) {
  this->SetField(FieldId::kBang, 101);
  this->CheckHasFields({FieldId::kBang});
  EXPECT_EQ(101, this->GetField(FieldId::kBang));

  this->SetField(FieldId::kFirst, 99);
  this->SetField(FieldId::kLast, 202);
  this->CheckHasFields({FieldId::kFirst, FieldId::kBang, FieldId::kLast});
  EXPECT_EQ(99, this->GetField(FieldId::kFirst));
  EXPECT_EQ(101, this->GetField(FieldId::kBang));
  EXPECT_EQ(202, this->GetField(FieldId::kLast));
}

TYPED_TEST(SparseVectorTest, MutateValue) {
  this->SetField(FieldId::kFoo, 101);
  EXPECT_EQ(101, this->GetField(FieldId::kFoo));
  this->SetField(FieldId::kFoo, 202);
  EXPECT_EQ(202, this->GetField(FieldId::kFoo));
}

TYPED_TEST(SparseVectorTest, EraseField) {
  this->SetField(FieldId::kFoo, 101);
  this->SetField(FieldId::kBar, 202);
  EXPECT_EQ(101, this->GetField(FieldId::kFoo));
  EXPECT_EQ(202, this->GetField(FieldId::kBar));

  // Should successfully remove the field.
  EXPECT_TRUE(this->EraseField(FieldId::kBar));
  this->CheckHasFields({FieldId::kFoo});
  EXPECT_EQ(101, this->GetField(FieldId::kFoo));

  // Multiple clears should return false since the value is already empty.
  EXPECT_FALSE(this->EraseField(FieldId::kBar));
  this->CheckHasFields({FieldId::kFoo});
  EXPECT_EQ(101, this->GetField(FieldId::kFoo));

  EXPECT_TRUE(this->EraseField(FieldId::kFoo));
  this->CheckHasFields({});
}

TYPED_TEST(SparseVectorTest, DoesNotOverwriteFieldsWithSmallerIndices) {
  this->SetField(FieldId::kFive, 42);
  this->SetField(FieldId::kBar, 29);
  EXPECT_EQ(42, this->GetField(FieldId::kFive));
  EXPECT_EQ(29, this->GetField(FieldId::kBar));
}

TYPED_TEST(SparseVectorTest, DoesNotOverwriteFieldsWithLargerIndices) {
  this->SetField(FieldId::kBar, 29);
  this->SetField(FieldId::kFive, 42);
  EXPECT_EQ(42, this->GetField(FieldId::kFive));
  EXPECT_EQ(29, this->GetField(FieldId::kBar));
}

TEST(SparseVectorPtrTest, SettingToNullptrMaintainsField) {
  SparseVector<FieldId, std::unique_ptr<int>> sparse_vector;
  EXPECT_FALSE(sparse_vector.HasField(FieldId::kFoo));

  sparse_vector.SetField(FieldId::kFoo, nullptr);
  EXPECT_TRUE(sparse_vector.HasField(FieldId::kFoo));
  EXPECT_EQ(nullptr, sparse_vector.GetField(FieldId::kFoo));

  sparse_vector.SetField(FieldId::kFoo, std::make_unique<int>(101));
  EXPECT_EQ(101, *sparse_vector.GetField(FieldId::kFoo));
  EXPECT_TRUE(sparse_vector.HasField(FieldId::kFoo));

  sparse_vector.SetField(FieldId::kFoo, nullptr);
  EXPECT_TRUE(sparse_vector.HasField(FieldId::kFoo));
  EXPECT_EQ(nullptr, sparse_vector.GetField(FieldId::kFoo));

  EXPECT_TRUE(sparse_vector.EraseField(FieldId::kFoo));
  EXPECT_FALSE(sparse_vector.HasField(FieldId::kFoo));
}

// WTF::Vector always uses 0 inline capacity when ANNOTATE_CONTIGUOUS_CONTAINER
// is defined.
#ifndef ANNOTATE_CONTIGUOUS_CONTAINER
TEST(SparseVectorInlineCapacityTest, Basic) {
  SparseVector<FieldId, int, 16> sparse_vector;
  EXPECT_EQ(16u, sparse_vector.capacity());
  EXPECT_GT(sizeof(sparse_vector), sizeof(int) * 16);
}
#endif

}  // namespace
}  // namespace blink
