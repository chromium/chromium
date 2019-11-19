// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/contiguous_container.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {
namespace {

struct Point2D {
  Point2D() : Point2D(0, 0) {}
  Point2D(int x, int y) : x(x), y(y) {}
  int x, y;
};

struct Point3D : public Point2D {
  Point3D() : Point3D(0, 0, 0) {}
  Point3D(int x, int y, int z) : Point2D(x, y), z(z) {}
  int z;
};

// Maximum size of a subclass of Point2D.
static const size_t kMaxPointSize = sizeof(Point3D);

// Alignment for Point2D and its subclasses.
static const size_t kPointAlignment = sizeof(int);

// How many elements to use for tests with "plenty" of elements.
static const unsigned kNumElements = 150;

TEST(ContiguousContainerTest, SimpleStructs) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  list.AllocateAndConstruct<Point2D>(1, 2);
  list.AllocateAndConstruct<Point3D>(3, 4, 5);
  list.AllocateAndConstruct<Point2D>(6, 7);

  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(1, list[0].x);
  EXPECT_EQ(2, list[0].y);
  EXPECT_EQ(3, list[1].x);
  EXPECT_EQ(4, list[1].y);
  EXPECT_EQ(5, static_cast<Point3D&>(list[1]).z);
  EXPECT_EQ(6, list[2].x);
  EXPECT_EQ(7, list[2].y);
}

TEST(ContiguousContainerTest, AllocateLots) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  for (int i = 0; i < (int)kNumElements; i++) {
    list.AllocateAndConstruct<Point2D>(i, i);
    list.AllocateAndConstruct<Point2D>(i, i);
    list.RemoveLast();
  }
  ASSERT_EQ(kNumElements, list.size());
  for (int i = 0; i < (int)kNumElements; i++) {
    ASSERT_EQ(i, list[i].x);
    ASSERT_EQ(i, list[i].y);
  }
}

class MockDestructible {
  USING_FAST_MALLOC(MockDestructible);

 public:
  ~MockDestructible() { Destruct(); }
  MOCK_METHOD0(Destruct, void());
};

TEST(ContiguousContainerTest, DestructorCalled) {
  ContiguousContainer<MockDestructible> list(sizeof(MockDestructible));
  auto& destructible = list.AllocateAndConstruct<MockDestructible>();
  EXPECT_EQ(&destructible, &list.First());
  EXPECT_CALL(destructible, Destruct());
}

TEST(ContiguousContainerTest, DestructorCalledOnceWhenClear) {
  ContiguousContainer<MockDestructible> list(sizeof(MockDestructible));
  auto& destructible = list.AllocateAndConstruct<MockDestructible>();
  EXPECT_EQ(&destructible, &list.First());

  testing::MockFunction<void()> separator;
  {
    testing::InSequence s;
    EXPECT_CALL(destructible, Destruct());
    EXPECT_CALL(separator, Call());
    EXPECT_CALL(destructible, Destruct()).Times(0);
  }

  list.Clear();
  separator.Call();
}

TEST(ContiguousContainerTest, DestructorCalledOnceWhenRemoveLast) {
  ContiguousContainer<MockDestructible> list(sizeof(MockDestructible));
  auto& destructible = list.AllocateAndConstruct<MockDestructible>();
  EXPECT_EQ(&destructible, &list.First());

  testing::MockFunction<void()> separator;
  {
    testing::InSequence s;
    EXPECT_CALL(destructible, Destruct());
    EXPECT_CALL(separator, Call());
    EXPECT_CALL(destructible, Destruct()).Times(0);
  }

  list.RemoveLast();
  separator.Call();
}

TEST(ContiguousContainerTest, DestructorCalledWithMultipleRemoveLastCalls) {
  // This container only requests space for one, but the implementation is
  // free to use more space if the allocator provides it.
  ContiguousContainer<MockDestructible> list(sizeof(MockDestructible),
                                             1 * sizeof(MockDestructible));
  testing::MockFunction<void()> separator;

  // We should be okay to allocate and remove a single one, like before.
  list.AllocateAndConstruct<MockDestructible>();
  EXPECT_EQ(1u, list.size());
  {
    testing::InSequence s;
    EXPECT_CALL(list[0], Destruct());
    EXPECT_CALL(separator, Call());
    EXPECT_CALL(list[0], Destruct()).Times(0);
  }
  list.RemoveLast();
  separator.Call();
  EXPECT_EQ(0u, list.size());

  testing::Mock::VerifyAndClearExpectations(&separator);

  // We should also be okay to allocate and remove multiple.
  list.AllocateAndConstruct<MockDestructible>();
  list.AllocateAndConstruct<MockDestructible>();
  list.AllocateAndConstruct<MockDestructible>();
  list.AllocateAndConstruct<MockDestructible>();
  list.AllocateAndConstruct<MockDestructible>();
  list.AllocateAndConstruct<MockDestructible>();
  EXPECT_EQ(6u, list.size());
  {
    // The last three should be destroyed by removeLast.
    testing::InSequence s;
    EXPECT_CALL(list[5], Destruct());
    EXPECT_CALL(separator, Call());
    EXPECT_CALL(list[5], Destruct()).Times(0);
    EXPECT_CALL(list[4], Destruct());
    EXPECT_CALL(separator, Call());
    EXPECT_CALL(list[4], Destruct()).Times(0);
    EXPECT_CALL(list[3], Destruct());
    EXPECT_CALL(separator, Call());
    EXPECT_CALL(list[3], Destruct()).Times(0);
  }
  list.RemoveLast();
  separator.Call();
  list.RemoveLast();
  separator.Call();
  list.RemoveLast();
  separator.Call();
  EXPECT_EQ(3u, list.size());

  // The remaining ones are destroyed when the test finishes.
  EXPECT_CALL(list[2], Destruct());
  EXPECT_CALL(list[1], Destruct());
  EXPECT_CALL(list[0], Destruct());
}

TEST(ContiguousContainerTest, InsertionAndIndexedAccess) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);

  auto& point1 = list.AllocateAndConstruct<Point2D>();
  auto& point2 = list.AllocateAndConstruct<Point2D>();
  auto& point3 = list.AllocateAndConstruct<Point2D>();

  EXPECT_EQ(3u, list.size());
  EXPECT_EQ(&point1, &list.First());
  EXPECT_EQ(&point3, &list.Last());
  EXPECT_EQ(&point1, &list[0]);
  EXPECT_EQ(&point2, &list[1]);
  EXPECT_EQ(&point3, &list[2]);
}

TEST(ContiguousContainerTest, InsertionAndClear) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  EXPECT_TRUE(list.IsEmpty());
  EXPECT_EQ(0u, list.size());

  list.AllocateAndConstruct<Point2D>();
  EXPECT_FALSE(list.IsEmpty());
  EXPECT_EQ(1u, list.size());

  list.Clear();
  EXPECT_TRUE(list.IsEmpty());
  EXPECT_EQ(0u, list.size());

  list.AllocateAndConstruct<Point2D>();
  EXPECT_FALSE(list.IsEmpty());
  EXPECT_EQ(1u, list.size());
}

TEST(ContiguousContainerTest, ElementAddressesAreStable) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  Vector<Point2D*> pointers;
  for (int i = 0; i < (int)kNumElements; i++)
    pointers.push_back(&list.AllocateAndConstruct<Point2D>());
  EXPECT_EQ(kNumElements, list.size());
  EXPECT_EQ(kNumElements, pointers.size());

  auto list_it = list.begin();
  auto** vector_it = pointers.begin();
  for (; list_it != list.end(); ++list_it, ++vector_it)
    EXPECT_EQ(&*list_it, *vector_it);
}

TEST(ContiguousContainerTest, ForwardIteration) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  for (int i = 0; i < (int)kNumElements; i++)
    list.AllocateAndConstruct<Point2D>(i, i);
  unsigned count = 0;
  for (Point2D& point : list) {
    EXPECT_EQ((int)count, point.x);
    count++;
  }
  EXPECT_EQ(kNumElements, count);

  static_assert(std::is_same<decltype(*list.begin()), Point2D&>::value,
                "Non-const iteration should produce non-const references.");
}

TEST(ContiguousContainerTest, ConstForwardIteration) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  for (int i = 0; i < (int)kNumElements; i++)
    list.AllocateAndConstruct<Point2D>(i, i);

  const auto& const_list = list;
  unsigned count = 0;
  for (const Point2D& point : const_list) {
    EXPECT_EQ((int)count, point.x);
    count++;
  }
  EXPECT_EQ(kNumElements, count);

  static_assert(
      std::is_same<decltype(*const_list.begin()), const Point2D&>::value,
      "Const iteration should produce const references.");
}

TEST(ContiguousContainerTest, ReverseIteration) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  for (int i = 0; i < (int)kNumElements; i++)
    list.AllocateAndConstruct<Point2D>(i, i);

  unsigned count = 0;
  for (auto it = list.rbegin(); it != list.rend(); ++it) {
    EXPECT_EQ((int)(kNumElements - 1 - count), it->x);
    count++;
  }
  EXPECT_EQ(kNumElements, count);

  static_assert(std::is_same<decltype(*list.rbegin()), Point2D&>::value,
                "Non-const iteration should produce non-const references.");
}

// Checks that the latter list has pointers to the elements of the former.
template <typename It1, typename It2>
bool EqualPointers(It1 it1, const It1& end1, It2 it2) {
  for (; it1 != end1; ++it1, ++it2) {
    if (&*it1 != *it2)
      return false;
  }
  return true;
}

TEST(ContiguousContainerTest, IterationAfterRemoveLast) {
  struct SmallStruct {
    char dummy[16];
  };
  ContiguousContainer<SmallStruct> list(sizeof(SmallStruct),
                                        1 * sizeof(SmallStruct));
  Vector<SmallStruct*> pointers;

  // Utilities which keep these two lists in sync and check that their
  // iteration order matches.
  auto push = [&list, &pointers]() {
    pointers.push_back(&list.AllocateAndConstruct<SmallStruct>());
  };
  auto pop = [&list, &pointers]() {
    pointers.pop_back();
    list.RemoveLast();
  };
  auto check_equal = [&list, &pointers]() {
    // They should be of the same size, and compare equal with all four
    // kinds of iteration.
    const auto& const_list = list;
    const auto& const_pointers = pointers;
    ASSERT_EQ(list.size(), pointers.size());
    ASSERT_TRUE(EqualPointers(list.begin(), list.end(), pointers.begin()));
    ASSERT_TRUE(EqualPointers(const_list.begin(), const_list.end(),
                              const_pointers.begin()));
    ASSERT_TRUE(EqualPointers(list.rbegin(), list.rend(), pointers.rbegin()));
    ASSERT_TRUE(EqualPointers(const_list.rbegin(), const_list.rend(),
                              const_pointers.rbegin()));
  };

  // Note that the allocations that actually happen may not match the
  // idealized descriptions here, since the implementation takes advantage of
  // space available in the underlying allocator.
  check_equal();  // Initially empty.
  push();
  check_equal();  // One full inner list.
  push();
  check_equal();  // One full, one partially full.
  push();
  push();
  check_equal();  // Two full, one partially full.
  pop();
  check_equal();  // Two full, one empty.
  pop();
  check_equal();  // One full, one partially full, one empty.
  pop();
  check_equal();  // One full, one empty.
  push();
  pop();
  pop();
  ASSERT_TRUE(list.IsEmpty());
  check_equal();  // Empty.
}

TEST(ContiguousContainerTest, AppendByMovingSameList) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  list.AllocateAndConstruct<Point3D>(1, 2, 3);

  // Moves the Point3D to the end, and default-constructs a Point2D in its
  // place.
  list.AppendByMoving(list.First(), sizeof(Point3D));
  EXPECT_EQ(1, list.Last().x);
  EXPECT_EQ(2, list.Last().y);
  EXPECT_EQ(3, static_cast<const Point3D&>(list.Last()).z);
  EXPECT_EQ(2u, list.size());

  // Moves that Point2D to the end, and default-constructs another in its
  // place.
  list.First().x = 4;
  list.AppendByMoving(list.First(), sizeof(Point2D));
  EXPECT_EQ(4, list.Last().x);
  EXPECT_EQ(3u, list.size());
}

TEST(ContiguousContainerTest, AppendByMovingDoesNotDestruct) {
  // GMock mock objects (e.g. MockDestructible) aren't guaranteed to be safe
  // to memcpy (which is required for appendByMoving).
  class DestructionNotifier {
    USING_FAST_MALLOC(DestructionNotifier);

   public:
    DestructionNotifier(bool* flag = nullptr) : flag_(flag) {}
    ~DestructionNotifier() {
      if (flag_)
        *flag_ = true;
    }

   private:
    bool* flag_;
  };

  bool destroyed = false;
  ContiguousContainer<DestructionNotifier> list1(sizeof(DestructionNotifier));
  list1.AllocateAndConstruct<DestructionNotifier>(&destroyed);
  {
    // Make sure destructor isn't called during appendByMoving.
    ContiguousContainer<DestructionNotifier> list2(sizeof(DestructionNotifier));
    list2.AppendByMoving(list1.Last(), sizeof(DestructionNotifier));
    EXPECT_FALSE(destroyed);
  }
  // But it should be destroyed when list2 is.
  EXPECT_TRUE(destroyed);
}

TEST(ContiguousContainerTest, AppendByMovingReturnsMovedPointer) {
  ContiguousContainer<Point2D, kPointAlignment> list1(kMaxPointSize);
  ContiguousContainer<Point2D, kPointAlignment> list2(kMaxPointSize);

  Point2D& point = list1.AllocateAndConstruct<Point2D>();
  Point2D& moved_point1 = list2.AppendByMoving(point, sizeof(Point2D));
  EXPECT_EQ(&moved_point1, &list2.Last());

  Point2D& moved_point2 = list1.AppendByMoving(moved_point1, sizeof(Point2D));
  EXPECT_EQ(&moved_point2, &list1.Last());
  EXPECT_NE(&moved_point1, &moved_point2);
}

TEST(ContiguousContainerTest, AppendByMovingReplacesSourceWithNewElement) {
  ContiguousContainer<Point2D, kPointAlignment> list1(kMaxPointSize);
  ContiguousContainer<Point2D, kPointAlignment> list2(kMaxPointSize);

  list1.AllocateAndConstruct<Point2D>(1, 2);
  EXPECT_EQ(1, list1.First().x);
  EXPECT_EQ(2, list1.First().y);

  list2.AppendByMoving(list1.First(), sizeof(Point2D));
  EXPECT_EQ(0, list1.First().x);
  EXPECT_EQ(0, list1.First().y);
  EXPECT_EQ(1, list2.First().x);
  EXPECT_EQ(2, list2.First().y);

  EXPECT_EQ(1u, list1.size());
  EXPECT_EQ(1u, list2.size());
}

TEST(ContiguousContainerTest, AppendByMovingElementsOfDifferentSizes) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  list.AllocateAndConstruct<Point3D>(1, 2, 3);
  list.AllocateAndConstruct<Point2D>(4, 5);

  EXPECT_EQ(1, list[0].x);
  EXPECT_EQ(2, list[0].y);
  EXPECT_EQ(3, static_cast<const Point3D&>(list[0]).z);
  EXPECT_EQ(4, list[1].x);
  EXPECT_EQ(5, list[1].y);

  // Test that moving the first element actually moves the entire object, not
  // just the base element.
  list.AppendByMoving(list[0], sizeof(Point3D));
  EXPECT_EQ(1, list[2].x);
  EXPECT_EQ(2, list[2].y);
  EXPECT_EQ(3, static_cast<const Point3D&>(list[2]).z);
  EXPECT_EQ(4, list[1].x);
  EXPECT_EQ(5, list[1].y);

  list.AppendByMoving(list[1], sizeof(Point2D));
  EXPECT_EQ(1, list[2].x);
  EXPECT_EQ(2, list[2].y);
  EXPECT_EQ(3, static_cast<const Point3D&>(list[2]).z);
  EXPECT_EQ(4, list[3].x);
  EXPECT_EQ(5, list[3].y);
}

TEST(ContiguousContainerTest, Swap) {
  ContiguousContainer<Point2D, kPointAlignment> list1(kMaxPointSize);
  list1.AllocateAndConstruct<Point2D>(1, 2);
  ContiguousContainer<Point2D, kPointAlignment> list2(kMaxPointSize);
  list2.AllocateAndConstruct<Point2D>(3, 4);
  list2.AllocateAndConstruct<Point2D>(5, 6);

  EXPECT_EQ(1u, list1.size());
  EXPECT_EQ(1, list1[0].x);
  EXPECT_EQ(2, list1[0].y);
  EXPECT_EQ(2u, list2.size());
  EXPECT_EQ(3, list2[0].x);
  EXPECT_EQ(4, list2[0].y);
  EXPECT_EQ(5, list2[1].x);
  EXPECT_EQ(6, list2[1].y);

  list2.Swap(list1);

  EXPECT_EQ(1u, list2.size());
  EXPECT_EQ(1, list2[0].x);
  EXPECT_EQ(2, list2[0].y);
  EXPECT_EQ(2u, list1.size());
  EXPECT_EQ(3, list1[0].x);
  EXPECT_EQ(4, list1[0].y);
  EXPECT_EQ(5, list1[1].x);
  EXPECT_EQ(6, list1[1].y);
}

TEST(ContiguousContainerTest, CapacityInBytes) {
  const int kIterations = 500;
  const size_t kInitialCapacity = 10 * kMaxPointSize;
  const size_t kUpperBoundOnMinCapacity = kInitialCapacity;

  // At time of writing, removing elements from the end can cause up to 7x the
  // memory required to be consumed, in the worst case, since we can have up to
  // two trailing inner lists that are empty (for 2*size + 4*size in unused
  // memory, due to the exponential growth strategy).
  // Unfortunately, this captures behaviour of the underlying allocator as
  // well as this container, so we're pretty loose here. This constant may
  // need to be adjusted.
  const size_t kMaxWasteFactor = 8;

  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize,
                                                     kInitialCapacity);

  // The capacity should grow with the list.
  for (int i = 0; i < kIterations; i++) {
    size_t capacity = list.CapacityInBytes();
    ASSERT_GE(capacity, list.size() * sizeof(Point2D));
    ASSERT_LE(capacity, std::max(list.size() * sizeof(Point2D),
                                 kUpperBoundOnMinCapacity) *
                            kMaxWasteFactor);
    list.AllocateAndConstruct<Point2D>();
  }

  // The capacity should shrink with the list.
  for (int i = 0; i < kIterations; i++) {
    size_t capacity = list.CapacityInBytes();
    ASSERT_GE(capacity, list.size() * sizeof(Point2D));
    ASSERT_LE(capacity, std::max(list.size() * sizeof(Point2D),
                                 kUpperBoundOnMinCapacity) *
                            kMaxWasteFactor);
    list.RemoveLast();
  }
}

TEST(ContiguousContainerTest, CapacityInBytesAfterClear) {
  // Clearing should restore the capacity of the container to the same as a
  // newly allocated one (without reserved capacity requested).
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  size_t empty_capacity = list.CapacityInBytes();
  list.AllocateAndConstruct<Point2D>();
  list.AllocateAndConstruct<Point2D>();
  list.Clear();
  EXPECT_EQ(empty_capacity, list.CapacityInBytes());
}

TEST(ContiguousContainerTest, Alignment) {
  const size_t kMaxAlign = alignof(long double);
  ContiguousContainer<Point2D, kMaxAlign> list(kMaxPointSize);

  list.AllocateAndConstruct<Point2D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));
  list.AllocateAndConstruct<Point2D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));
  list.AllocateAndConstruct<Point3D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));
  list.AllocateAndConstruct<Point3D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));
  list.AllocateAndConstruct<Point2D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));

  list.AppendByMoving(list[0], sizeof(Point2D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));
  list.AppendByMoving(list[1], sizeof(Point2D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));
  list.AppendByMoving(list[2], sizeof(Point3D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));
  list.AppendByMoving(list[3], sizeof(Point3D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));
  list.AppendByMoving(list[4], sizeof(Point2D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.Last()) & (kMaxAlign - 1));
}

}  // namespace
}  // namespace blink
