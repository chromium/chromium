// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_coordinator.h"

#include <set>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/gtest_util.h"
#include "base/unguessable_token.h"
#include "services/audio/loopback_source.h"
#include "services/audio/test/mock_loopback_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

namespace {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::StrictMock;

// A mock observer to verify that notifications are sent correctly.
class MockObserver : public LoopbackCoordinator::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnMemberAdded,
              (const LoopbackCoordinator::Member& member),
              (override));
  MOCK_METHOD(void,
              OnMemberRemoved,
              (const LoopbackCoordinator::Member& member),
              (override));
};

}  // namespace

class LoopbackCoordinatorTest : public testing::Test {
 public:
  LoopbackCoordinatorTest() = default;
  ~LoopbackCoordinatorTest() override = default;

 protected:
  LoopbackCoordinator coordinator_;
};

TEST_F(LoopbackCoordinatorTest, AddMemberNotifiesObserver) {
  StrictMock<MockObserver> observer;
  coordinator_.AddObserver(&observer);

  MockLoopbackSource source;
  const auto group_id = base::UnguessableToken::Create();

  EXPECT_CALL(
      observer,
      OnMemberAdded(AllOf(
          Field(&LoopbackCoordinator::Member::group_id, group_id),
          Field(&LoopbackCoordinator::Member::loopback_source, &source))));

  coordinator_.AddMember(group_id, &source);

  // Cleanup.
  coordinator_.RemoveObserver(&observer);
  coordinator_.RemoveMember(&source);
}

TEST_F(LoopbackCoordinatorTest, RemoveMemberNotifiesObserver) {
  StrictMock<MockObserver> observer;

  MockLoopbackSource source;
  const auto group_id = base::UnguessableToken::Create();
  coordinator_.AddMember(group_id, &source);
  coordinator_.AddObserver(&observer);

  // The removal notification should be sent for the correct member.
  EXPECT_CALL(
      observer,
      OnMemberRemoved(AllOf(
          Field(&LoopbackCoordinator::Member::group_id, group_id),
          Field(&LoopbackCoordinator::Member::loopback_source, &source))));

  coordinator_.RemoveMember(&source);

  // Cleanup.
  coordinator_.RemoveObserver(&observer);
}

TEST_F(LoopbackCoordinatorTest, RemovedObserverReceivesNoNotifications) {
  StrictMock<MockObserver> observer;
  coordinator_.AddObserver(&observer);
  coordinator_.RemoveObserver(&observer);

  // Since the observer was removed, no methods should be called on it.
  // StrictMock will fail the test if any unexpected calls occur.
  MockLoopbackSource source;
  const auto group_id = base::UnguessableToken::Create();
  coordinator_.AddMember(group_id, &source);
  coordinator_.RemoveMember(&source);
}

TEST_F(LoopbackCoordinatorTest, ForEachSourceIteratesOverAllMembers) {
  MockLoopbackSource source1, source2, source3;
  coordinator_.AddMember(base::UnguessableToken::Create(), &source1);
  coordinator_.AddMember(base::UnguessableToken::Create(), &source2);
  coordinator_.AddMember(base::UnguessableToken::Create(), &source3);

  std::set<LoopbackSource*> found_sources;
  coordinator_.ForEachMember(base::BindRepeating(
      [](std::set<LoopbackSource*>* found,
         const LoopbackCoordinator::Member& member) {
        found->insert(member.loopback_source);
      },
      &found_sources));

  // Verify that the callback was run for every member we added.
  EXPECT_EQ(found_sources.size(), 3u);
  EXPECT_TRUE(found_sources.count(&source1));
  EXPECT_TRUE(found_sources.count(&source2));
  EXPECT_TRUE(found_sources.count(&source3));

  // Cleanup.
  coordinator_.RemoveMember(&source1);
  coordinator_.RemoveMember(&source2);
  coordinator_.RemoveMember(&source3);
}

TEST_F(LoopbackCoordinatorTest, ForEachSourceOnEmptyCoordinatorIsNoOp) {
  bool callback_was_run = false;
  coordinator_.ForEachMember(base::BindRepeating(
      [](bool* was_run, const LoopbackCoordinator::Member& member) {
        *was_run = true;
      },
      &callback_was_run));

  EXPECT_FALSE(callback_was_run);
}

// Mock listener for LoopbackGroupObserver events.
class MockListener : public LoopbackGroupObserver::Listener {
 public:
  MockListener() = default;
  ~MockListener() override = default;

  MOCK_METHOD(void, OnSourceAdded, (LoopbackSource*), (override));
  MOCK_METHOD(void, OnSourceRemoved, (LoopbackSource*), (override));
};

class LoopbackGroupObserverTest : public testing::Test {
 public:
  LoopbackGroupObserverTest() = default;
  ~LoopbackGroupObserverTest() override = default;

 protected:
  LoopbackCoordinator coordinator_;
};

// Test that the OnSourceAdded listener is invoked when a member of the
// correct group is added.
TEST_F(LoopbackGroupObserverTest, MatchingObserver_OnSourceAdded) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;
  auto observer = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id);
  MockLoopbackSource source;

  EXPECT_CALL(listener, OnSourceAdded(&source)).Times(1);

  observer->StartObserving(&listener);
  coordinator_.AddMember(group_id, &source);

  // Cleanup
  observer->StopObserving();
  coordinator_.RemoveMember(&source);
}

// Test that the OnSourceRemoved listener is invoked when a member of the
// correct group is removed.
TEST_F(LoopbackGroupObserverTest, MatchingObserver_OnSourceRemoved) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;
  auto observer = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id);
  MockLoopbackSource source;

  coordinator_.AddMember(group_id, &source);

  EXPECT_CALL(listener, OnSourceRemoved(&source)).Times(1);

  observer->StartObserving(&listener);
  coordinator_.RemoveMember(&source);
}

// Test that the listener is NOT invoked when a member of a different group is
// added/removed.
TEST_F(LoopbackGroupObserverTest, MatchingObserver_NonMatchingGroupId) {
  const auto group_id1 = base::UnguessableToken::Create();
  const auto group_id2 = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;
  auto observer = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id1);
  MockLoopbackSource source;

  // StrictMock ensures notifications are not propagated.
  observer->StartObserving(&listener);
  coordinator_.AddMember(group_id2, &source);
  coordinator_.RemoveMember(&source);
}

// Test that ForEachSource iterates only over members of the observer's group.
TEST_F(LoopbackGroupObserverTest, MatchingObserver_ForEachSource) {
  const auto group_id1 = base::UnguessableToken::Create();
  const auto group_id2 = base::UnguessableToken::Create();
  auto observer = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id1);

  MockLoopbackSource source1_1, source1_2, source2_1;
  coordinator_.AddMember(group_id1, &source1_1);
  coordinator_.AddMember(group_id2, &source2_1);
  coordinator_.AddMember(group_id1, &source1_2);

  std::vector<LoopbackSource*> found_sources;
  observer->ForEachSource(base::BindRepeating(
      [](std::vector<LoopbackSource*>* found_sources, LoopbackSource* source) {
        found_sources->push_back(source);
      },
      &found_sources));

  EXPECT_THAT(found_sources,
              testing::UnorderedElementsAre(&source1_1, &source1_2));

  // Cleanup.
  coordinator_.RemoveMember(&source1_1);
  coordinator_.RemoveMember(&source2_1);
  coordinator_.RemoveMember(&source1_2);
}

// Test that no notifications are received after StopObserving() is called.
TEST_F(LoopbackGroupObserverTest, MatchingObserver_StopObserving) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;
  auto observer = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id);
  MockLoopbackSource source;

  observer->StartObserving(&listener);
  observer->StopObserving();

  // StrictMock ensures norifications are not propagated.
  coordinator_.AddMember(group_id, &source);
  coordinator_.RemoveMember(&source);
}

// Test that the observer correctly unregisters itself upon destruction.
TEST_F(LoopbackGroupObserverTest, MatchingObserver_Destruction) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;
  auto observer = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id);
  MockLoopbackSource source;

  observer->StartObserving(&listener);
  observer.reset();  // Destroy the observer.

  // No crash should occur here, and no mock methods should be called.
  coordinator_.AddMember(group_id, &source);
  coordinator_.RemoveMember(&source);
}

// Test that an observer handles members that existed before it started
// observing. It should not receive add notifications for them, but should
// be able to iterate over them.
TEST_F(LoopbackGroupObserverTest, MatchingObserver_ObservesPreExistingMembers) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;

  MockLoopbackSource source1, source2;

  // Add a member before the observer starts.
  coordinator_.AddMember(group_id, &source1);

  auto observer = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id);

  // The observer should not be notified of pre-existing members.
  EXPECT_CALL(listener, OnSourceAdded(_)).Times(0);
  observer->StartObserving(&listener);
  testing::Mock::VerifyAndClearExpectations(&listener);

  // But it should be able to iterate over them.
  std::vector<LoopbackSource*> found_sources;
  observer->ForEachSource(base::BindRepeating(
      [](std::vector<LoopbackSource*>* found_sources, LoopbackSource* source) {
        found_sources->push_back(source);
      },
      &found_sources));
  EXPECT_THAT(found_sources, testing::ElementsAre(&source1));

  // And it should be notified of new members.
  EXPECT_CALL(listener, OnSourceAdded(&source2)).Times(1);
  coordinator_.AddMember(group_id, &source2);

  observer->StopObserving();
  coordinator_.RemoveMember(&source2);
  coordinator_.RemoveMember(&source1);
}

// Test that multiple observers on the same group all receive notifications.
TEST_F(LoopbackGroupObserverTest, MultipleObserversOnSameGroup) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockListener> listener1, listener2;
  auto observer1 = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id);
  auto observer2 = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id);
  MockLoopbackSource source;

  observer1->StartObserving(&listener1);
  observer2->StartObserving(&listener2);

  EXPECT_CALL(listener1, OnSourceAdded(&source)).Times(1);
  EXPECT_CALL(listener2, OnSourceAdded(&source)).Times(1);
  coordinator_.AddMember(group_id, &source);
  testing::Mock::VerifyAndClearExpectations(&listener1);
  testing::Mock::VerifyAndClearExpectations(&listener2);

  EXPECT_CALL(listener1, OnSourceRemoved(&source)).Times(1);
  EXPECT_CALL(listener2, OnSourceRemoved(&source)).Times(1);
  coordinator_.RemoveMember(&source);
}

// Test that calling StartObserving and StopObserving multiple times is safe.
TEST_F(LoopbackGroupObserverTest, StartAndStopObservingIsRobust) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;
  auto observer = LoopbackGroupObserver::CreateMatchingGroupObserver(
      &coordinator_, group_id);
  MockLoopbackSource source1, source2;

  // Multiple starts should be fine.
  observer->StartObserving(&listener);
  observer->StartObserving(&listener);

  EXPECT_CALL(listener, OnSourceAdded(&source1)).Times(1);
  coordinator_.AddMember(group_id, &source1);
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Multiple stops should be fine.
  observer->StopObserving();
  observer->StopObserving();

  // No notifications should be received after stopping.
  EXPECT_CALL(listener, OnSourceAdded(_)).Times(0);
  EXPECT_CALL(listener, OnSourceRemoved(_)).Times(0);
  coordinator_.AddMember(group_id, &source2);
  coordinator_.RemoveMember(&source1);
  coordinator_.RemoveMember(&source2);
}

// Test that listener is NOT invoked for the excluded group.
TEST_F(LoopbackGroupObserverTest, ExcludingObserver_OnSourceAdded_Excluded) {
  const auto group_id_to_exclude = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;
  auto observer = LoopbackGroupObserver::CreateExcludingGroupObserver(
      &coordinator_, group_id_to_exclude);
  MockLoopbackSource source;

  // StrictMock will ensure OnSourceAdded is not called.
  observer->StartObserving(&listener);
  coordinator_.AddMember(group_id_to_exclude, &source);

  // Cleanup
  coordinator_.RemoveMember(&source);
}

// Test that listener is invoked for a non-excluded group.
TEST_F(LoopbackGroupObserverTest, ExcludingObserver_OnSourceAdded_NotExcluded) {
  const auto group_id_to_exclude = base::UnguessableToken::Create();
  const auto other_group_id = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;
  auto observer = LoopbackGroupObserver::CreateExcludingGroupObserver(
      &coordinator_, group_id_to_exclude);
  MockLoopbackSource source;

  EXPECT_CALL(listener, OnSourceAdded(&source)).Times(1);
  EXPECT_CALL(listener, OnSourceRemoved(&source)).Times(1);

  observer->StartObserving(&listener);
  coordinator_.AddMember(other_group_id, &source);
  coordinator_.RemoveMember(&source);
}

// Test that ForEachSource iterates over all non-excluded members.
TEST_F(LoopbackGroupObserverTest, ExcludingObserver_ForEachSource) {
  const auto group_id_to_exclude = base::UnguessableToken::Create();
  const auto other_group_id = base::UnguessableToken::Create();
  StrictMock<MockListener> listener;
  auto observer = LoopbackGroupObserver::CreateExcludingGroupObserver(
      &coordinator_, group_id_to_exclude);

  MockLoopbackSource source1, source2, excluded_source;
  coordinator_.AddMember(other_group_id, &source1);
  coordinator_.AddMember(other_group_id, &source2);
  coordinator_.AddMember(group_id_to_exclude, &excluded_source);

  std::vector<LoopbackSource*> found_sources;
  observer->ForEachSource(base::BindRepeating(
      [](std::vector<LoopbackSource*>* found_sources, LoopbackSource* source) {
        found_sources->push_back(source);
      },
      &found_sources));

  EXPECT_THAT(found_sources, testing::UnorderedElementsAre(&source1, &source2));

  // Cleanup.
  coordinator_.RemoveMember(&source1);
  coordinator_.RemoveMember(&source2);
  coordinator_.RemoveMember(&excluded_source);
}

}  // namespace audio
