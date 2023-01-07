// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/io_thread/leak_tracker.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ClassA {
 private:
  LeakTracker<ClassA> leak_tracker_;
};

class ClassB {
 private:
  LeakTracker<ClassB> leak_tracker_;
};

#ifndef ENABLE_LEAK_TRACKER

// If leak tracking is disabled, we should do nothing.
TEST(LeakTrackerTest, NotEnabled) {
  EXPECT_EQ(-1, LeakTracker<ClassA>::NumLiveInstances());
  EXPECT_EQ(-1, LeakTracker<ClassB>::NumLiveInstances());

  // Use unique_ptr so compiler doesn't complain about unused variables.
  std::unique_ptr<ClassA> a1(new ClassA);
  std::unique_ptr<ClassB> b1(new ClassB);
  std::unique_ptr<ClassB> b2(new ClassB);

  EXPECT_EQ(-1, LeakTracker<ClassA>::NumLiveInstances());
  EXPECT_EQ(-1, LeakTracker<ClassB>::NumLiveInstances());
}

#else

TEST(LeakTrackerTest, Basic) {
  {
    ClassA a1;

    EXPECT_EQ(1, LeakTracker<ClassA>::NumLiveInstances());
    EXPECT_EQ(0, LeakTracker<ClassB>::NumLiveInstances());

    ClassB b1;
    ClassB b2;

    EXPECT_EQ(1, LeakTracker<ClassA>::NumLiveInstances());
    EXPECT_EQ(2, LeakTracker<ClassB>::NumLiveInstances());

    std::unique_ptr<ClassA> a2(new ClassA);

    EXPECT_EQ(2, LeakTracker<ClassA>::NumLiveInstances());
    EXPECT_EQ(2, LeakTracker<ClassB>::NumLiveInstances());

    a2.reset();

    EXPECT_EQ(1, LeakTracker<ClassA>::NumLiveInstances());
    EXPECT_EQ(2, LeakTracker<ClassB>::NumLiveInstances());
  }

  EXPECT_EQ(0, LeakTracker<ClassA>::NumLiveInstances());
  EXPECT_EQ(0, LeakTracker<ClassB>::NumLiveInstances());
}

// Try some orderings of create/remove to hit different cases in the linked-list
// assembly.
TEST(LeakTrackerTest, LinkedList) {
  EXPECT_EQ(0, LeakTracker<ClassB>::NumLiveInstances());

  std::unique_ptr<ClassA> a1(new ClassA);
  std::unique_ptr<ClassA> a2(new ClassA);
  std::unique_ptr<ClassA> a3(new ClassA);
  std::unique_ptr<ClassA> a4(new ClassA);

  EXPECT_EQ(4, LeakTracker<ClassA>::NumLiveInstances());

  // Remove the head of the list (a1).
  a1.reset();
  EXPECT_EQ(3, LeakTracker<ClassA>::NumLiveInstances());

  // Remove the tail of the list (a4).
  a4.reset();
  EXPECT_EQ(2, LeakTracker<ClassA>::NumLiveInstances());

  // Append to the new tail of the list (a3).
  std::unique_ptr<ClassA> a5(new ClassA);
  EXPECT_EQ(3, LeakTracker<ClassA>::NumLiveInstances());

  a2.reset();
  a3.reset();

  EXPECT_EQ(1, LeakTracker<ClassA>::NumLiveInstances());

  a5.reset();
  EXPECT_EQ(0, LeakTracker<ClassA>::NumLiveInstances());
}

TEST(LeakTrackerTest, NoOpCheckForLeaks) {
  // There are no live instances of ClassA, so this should do nothing.
  LeakTracker<ClassA>::CheckForLeaks();
}

#endif  // ENABLE_LEAK_TRACKER

}  // namespace
