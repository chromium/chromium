// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/contiguous_container.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
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
static const wtf_size_t kMaxPointSize = sizeof(Point3D);

// Alignment for Point2D and its subclasses.
static const wtf_size_t kPointAlignment = sizeof(int);

// How many elements to use for tests with "plenty" of elements.
static const wtf_size_t kNumElements = 150;

static const wtf_size_t kDefaultInitialCapacityInBytes = 256;

class PointList : public ContiguousContainer<Point2D, kPointAlignment> {
 public:
  explicit PointList(
      wtf_size_t initial_capacity_in_bytes = kDefaultInitialCapacityInBytes)
      : ContiguousContainer(kMaxPointSize, initial_capacity_in_bytes) {}
};

TEST(ContiguousContainerTest, SimpleStructs) {
  PointList list;
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
  PointList list;
  for (int i = 0; i < static_cast<int>(kNumElements); i++)
    list.AllocateAndConstruct<Point2D>(i, i);
  ASSERT_EQ(kNumElements, list.size());
  for (int i = 0; i < static_cast<int>(kNumElements); i++) {
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

class MockDestructibleList : public ContiguousContainer<MockDestructible> {
 public:
  explicit MockDestructibleList(
      wtf_size_t initial_capacity_in_bytes = kDefaultInitialCapacityInBytes)
      : ContiguousContainer(sizeof(MockDestructible),
                            initial_capacity_in_bytes) {}
};

TEST(ContiguousContainerTest, DestructorCalled) {
  MockDestructibleList list;
  auto& destructible = list.AllocateAndConstruct<MockDestructible>();
  EXPECT_EQ(&destructible, &list.front());
  EXPECT_CALL(destructible, Destruct());
}

TEST(ContiguousContainerTest, InsertionAndIndexedAccess) {
  PointList list;

  auto& point1 = list.AllocateAndConstruct<Point2D>();
  auto& point2 = list.AllocateAndConstruct<Point2D>();
  auto& point3 = list.AllocateAndConstruct<Point2D>();

  EXPECT_EQ(3u, list.size());
  EXPECT_EQ(&point1, &list.front());
  EXPECT_EQ(&point3, &list.back());
  EXPECT_EQ(&point1, &list[0]);
  EXPECT_EQ(&point2, &list[1]);
  EXPECT_EQ(&point3, &list[2]);
}

TEST(ContiguousContainerTest, Insertion) {
  PointList list;
  EXPECT_TRUE(list.IsEmpty());
  EXPECT_EQ(0u, list.size());
  EXPECT_EQ(0u, list.CapacityInBytes());
  EXPECT_EQ(0u, list.UsedCapacityInBytes());

  list.AllocateAndConstruct<Point2D>();
  EXPECT_FALSE(list.IsEmpty());
  EXPECT_EQ(1u, list.size());
  EXPECT_GE(list.CapacityInBytes(), kDefaultInitialCapacityInBytes);
  EXPECT_EQ(sizeof(Point2D), list.UsedCapacityInBytes());
}

TEST(ContiguousContainerTest, ElementAddressesAreStable) {
  PointList list;
  Vector<Point2D*> pointers;
  for (int i = 0; i < static_cast<int>(kNumElements); i++)
    pointers.push_back(&list.AllocateAndConstruct<Point2D>());
  EXPECT_EQ(kNumElements, list.size());
  EXPECT_EQ(kNumElements, pointers.size());

  auto list_it = list.begin();
  auto** vector_it = pointers.begin();
  for (; list_it != list.end(); ++list_it, ++vector_it)
    EXPECT_EQ(&*list_it, *vector_it);
}

TEST(ContiguousContainerTest, ForwardIteration) {
  PointList list;
  for (int i = 0; i < static_cast<int>(kNumElements); i++)
    list.AllocateAndConstruct<Point2D>(i, i);
  wtf_size_t count = 0;
  for (Point2D& point : list) {
    EXPECT_EQ(static_cast<int>(count), point.x);
    count++;
  }
  EXPECT_EQ(kNumElements, count);

  static_assert(std::is_same<decltype(*list.begin()), Point2D&>::value,
                "Non-const iteration should produce non-const references.");
}

TEST(ContiguousContainerTest, ConstForwardIteration) {
  PointList list;
  for (int i = 0; i < static_cast<int>(kNumElements); i++)
    list.AllocateAndConstruct<Point2D>(i, i);

  const auto& const_list = list;
  wtf_size_t count = 0;
  for (const Point2D& point : const_list) {
    EXPECT_EQ(static_cast<int>(count), point.x);
    count++;
  }
  EXPECT_EQ(kNumElements, count);

  static_assert(
      std::is_same<decltype(*const_list.begin()), const Point2D&>::value,
      "Const iteration should produce const references.");
}

TEST(ContiguousContainerTest, ReverseIteration) {
  PointList list;
  for (int i = 0; i < static_cast<int>(kNumElements); i++)
    list.AllocateAndConstruct<Point2D>(i, i);

  wtf_size_t count = 0;
  for (auto it = list.rbegin(); it != list.rend(); ++it) {
    EXPECT_EQ(static_cast<int>(kNumElements - 1 - count), it->x);
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

TEST(ContiguousContainerTest, AppendByMovingSameList) {
  PointList list;
  list.AllocateAndConstruct<Point3D>(1, 2, 3);

  // Moves the Point3D to the end, and default-constructs a Point2D in its
  // place.
  list.AppendByMoving(list.front(), sizeof(Point3D));
  EXPECT_EQ(1, list.back().x);
  EXPECT_EQ(2, list.back().y);
  EXPECT_EQ(3, static_cast<const Point3D&>(list.back()).z);
  EXPECT_EQ(2u, list.size());

  // Moves that Point2D to the end, and default-constructs another in its
  // place.
  list.front().x = 4;
  list.AppendByMoving(list.front(), sizeof(Point2D));
  EXPECT_EQ(4, list.back().x);
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
  ContiguousContainer<DestructionNotifier> list1(
      sizeof(DestructionNotifier), kDefaultInitialCapacityInBytes);
  list1.AllocateAndConstruct<DestructionNotifier>(&destroyed);
  {
    // Make sure destructor isn't called during appendByMoving.
    ContiguousContainer<DestructionNotifier> list2(
        sizeof(DestructionNotifier), kDefaultInitialCapacityInBytes);
    list2.AppendByMoving(list1.back(), sizeof(DestructionNotifier));
    EXPECT_FALSE(destroyed);
  }
  // But it should be destroyed when list2 is.
  EXPECT_TRUE(destroyed);
}

TEST(ContiguousContainerTest, AppendByMovingReturnsMovedPointer) {
  PointList list1;
  PointList list2;

  Point2D& point = list1.AllocateAndConstruct<Point2D>();
  Point2D& moved_point1 = list2.AppendByMoving(point, sizeof(Point2D));
  EXPECT_EQ(&moved_point1, &list2.back());

  Point2D& moved_point2 = list1.AppendByMoving(moved_point1, sizeof(Point2D));
  EXPECT_EQ(&moved_point2, &list1.back());
  EXPECT_NE(&moved_point1, &moved_point2);
}

TEST(ContiguousContainerTest, AppendByMovingReplacesSourceWithNewElement) {
  PointList list1;
  PointList list2;

  list1.AllocateAndConstruct<Point2D>(1, 2);
  EXPECT_EQ(1, list1.front().x);
  EXPECT_EQ(2, list1.front().y);

  list2.AppendByMoving(list1.front(), sizeof(Point2D));
  EXPECT_EQ(0, list1.front().x);
  EXPECT_EQ(0, list1.front().y);
  EXPECT_EQ(1, list2.front().x);
  EXPECT_EQ(2, list2.front().y);

  EXPECT_EQ(1u, list1.size());
  EXPECT_EQ(1u, list2.size());
}

TEST(ContiguousContainerTest, AppendByMovingElementsOfDifferentSizes) {
  PointList list;
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

TEST(ContiguousContainerTest, CapacityInBytes) {
  const int kIterations = 500;
  const wtf_size_t kInitialCapacity = 10 * kMaxPointSize;
  const wtf_size_t kUpperBoundOnMinCapacity = kInitialCapacity;
  // In worst case, there are 2 buffers, and the second buffer contains only one
  // element, so the factor is close to 3 as the second buffer is twice as big
  // as the first buffer.
  const size_t kMaxWasteFactor = 3;

  PointList list(kInitialCapacity);

  // The capacity should grow with the list.
  for (int i = 0; i < kIterations; i++) {
    size_t capacity = list.CapacityInBytes();
    ASSERT_GE(capacity, list.size() * sizeof(Point2D));
    ASSERT_LE(capacity, std::max<wtf_size_t>(list.size() * sizeof(Point2D),
                                             kUpperBoundOnMinCapacity) *
                            kMaxWasteFactor);
    list.AllocateAndConstruct<Point2D>();
  }
}

TEST(ContiguousContainerTest, Alignment) {
  const size_t kMaxAlign = alignof(long double);
  ContiguousContainer<Point2D, kMaxAlign> list(kMaxPointSize,
                                               kDefaultInitialCapacityInBytes);

  list.AllocateAndConstruct<Point2D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));
  list.AllocateAndConstruct<Point2D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));
  list.AllocateAndConstruct<Point3D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));
  list.AllocateAndConstruct<Point3D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));
  list.AllocateAndConstruct<Point2D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));

  list.AppendByMoving(list[0], sizeof(Point2D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));
  list.AppendByMoving(list[1], sizeof(Point2D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));
  list.AppendByMoving(list[2], sizeof(Point3D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));
  list.AppendByMoving(list[3], sizeof(Point3D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));
  list.AppendByMoving(list[4], sizeof(Point2D));
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.back()) & (kMaxAlign - 1));
}

}  // namespace
}  // namespace blink
