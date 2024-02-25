// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

Vector<std::pair<SMILTime, SMILTimeOrigin>> ExtractListContents(
    const SMILInstanceTimeList& list) {
  Vector<std::pair<SMILTime, SMILTimeOrigin>> times;
  for (const auto& item : list)
    times.push_back(std::make_pair(item.Time(), item.Origin()));
  return times;
}

TEST(SMILInstanceTimeListTest, Sort) {
  test::TaskEnvironment task_environment;
  SMILInstanceTimeList list;
  list.Append(SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(4), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 5u);
  list.Sort();

  Vector<std::pair<SMILTime, SMILTimeOrigin>> expected_times(
      {{SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(4), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute}});
  ASSERT_EQ(ExtractListContents(list), expected_times);
}

TEST(SMILInstanceTimeListTest, InsertSortedAndUnique) {
  test::TaskEnvironment task_environment;
  SMILInstanceTimeList list;
  list.Append(SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(2), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 3u);

  // Unique time/item.
  list.InsertSortedAndUnique(SMILTime::FromSecondsD(4),
                             SMILTimeOrigin::kScript);
  ASSERT_EQ(list.size(), 4u);
  Vector<std::pair<SMILTime, SMILTimeOrigin>> expected_times1(
      {{SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(2), SMILTimeOrigin::kScript},
       {SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(4), SMILTimeOrigin::kScript}});
  ASSERT_EQ(ExtractListContents(list), expected_times1);

  // Non-unique item.
  list.InsertSortedAndUnique(SMILTime::FromSecondsD(2),
                             SMILTimeOrigin::kScript);
  ASSERT_EQ(list.size(), 4u);
  ASSERT_EQ(ExtractListContents(list), expected_times1);

  // Same time but different origin.
  list.InsertSortedAndUnique(SMILTime::FromSecondsD(2),
                             SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 5u);
  Vector<std::pair<SMILTime, SMILTimeOrigin>> expected_times2(
      {{SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(2), SMILTimeOrigin::kScript},
       {SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(4), SMILTimeOrigin::kScript}});
  ASSERT_EQ(ExtractListContents(list), expected_times2);
}

TEST(SMILInstanceTimeListTest, RemoveWithOrigin) {
  test::TaskEnvironment task_environment;
  SMILInstanceTimeList list;
  list.Append(SMILTime::FromSecondsD(1), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(4), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 5u);

  list.RemoveWithOrigin(SMILTimeOrigin::kScript);
  ASSERT_EQ(list.size(), 3u);
  Vector<std::pair<SMILTime, SMILTimeOrigin>> expected_times(
      {{SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute}});
  ASSERT_EQ(ExtractListContents(list), expected_times);
}

TEST(SMILInstanceTimeListTest, NextAfter) {
  test::TaskEnvironment task_environment;
  SMILInstanceTimeList list;
  list.Append(SMILTime::FromSecondsD(1), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(4), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 5u);

  // Just before an entry in the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(2) - SMILTime::Epsilon()),
            SMILTime::FromSecondsD(2));
  // Equal to an entry in the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(2)),
            SMILTime::FromSecondsD(3));
  // Just after an entry in the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(2) + SMILTime::Epsilon()),
            SMILTime::FromSecondsD(3));
  // Equal to the last entry in the the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(5)), SMILTime::Unresolved());
  // After the last entry in the the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(6)), SMILTime::Unresolved());
}

class EmptyEventListener : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override {}
};

TEST(SVGSMILElementTest, RepeatNEventListenerUseCounted) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  WebFeature feature = WebFeature::kSMILElementHasRepeatNEventListener;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML("<svg><set/></svg>");
  Element* set = document.QuerySelector(AtomicString("set"));
  ASSERT_TRUE(set);
  set->addEventListener(AtomicString("repeatn"),
                        MakeGarbageCollected<EmptyEventListener>());
  EXPECT_TRUE(document.IsUseCounted(feature));
}

}  // namespace

}  // namespace blink
