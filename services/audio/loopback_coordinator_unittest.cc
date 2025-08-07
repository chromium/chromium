// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_coordinator.h"

#include <set>
#include <vector>

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

TEST_F(LoopbackCoordinatorTest, ForEachMemberIteratesOverAllMembers) {
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

TEST_F(LoopbackCoordinatorTest, ForEachMemberOnEmptyCoordinatorIsNoOp) {
  bool callback_was_run = false;
  coordinator_.ForEachMember(base::BindRepeating(
      [](bool* was_run, const LoopbackCoordinator::Member& member) {
        *was_run = true;
      },
      &callback_was_run));

  EXPECT_FALSE(callback_was_run);
}

class MockObserverCallbacks {
 public:
  MOCK_METHOD(void, OnSourceAdded, (LoopbackSource*));
  MOCK_METHOD(void, OnSourceRemoved, (LoopbackSource*));
};

class LoopbackGroupObserverTest : public testing::Test {
 public:
  LoopbackGroupObserverTest() = default;
  ~LoopbackGroupObserverTest() override = default;

 protected:
  // Helper function to create a LoopbackGroupObserver with mock callbacks.
  std::unique_ptr<LoopbackGroupObserver> CreateObserver(
      const base::UnguessableToken& group_id,
      MockObserverCallbacks* callbacks) {
    return std::make_unique<LoopbackGroupObserver>(
        &coordinator_, group_id,
        base::BindRepeating(&MockObserverCallbacks::OnSourceAdded,
                            base::Unretained(callbacks)),
        base::BindRepeating(&MockObserverCallbacks::OnSourceRemoved,
                            base::Unretained(callbacks)));
  }

  LoopbackCoordinator coordinator_;
};

// Test that the OnSourceAdded callback is invoked when a member of the
// correct group is added.
TEST_F(LoopbackGroupObserverTest, OnSourceAdded_MatchingGroupId) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockObserverCallbacks> callbacks;
  auto observer = CreateObserver(group_id, &callbacks);
  MockLoopbackSource source;

  EXPECT_CALL(callbacks, OnSourceAdded(&source)).Times(1);

  observer->StartObserving();
  coordinator_.AddMember(group_id, &source);
  observer->StopObserving();
  coordinator_.RemoveMember(&source);
}

// Test that the OnSourceRemoved callback is invoked when a member of the
// correct group is removed.
TEST_F(LoopbackGroupObserverTest, OnSourceRemoved_MatchingGroupId) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockObserverCallbacks> callbacks;
  auto observer = CreateObserver(group_id, &callbacks);
  MockLoopbackSource source;

  coordinator_.AddMember(group_id, &source);

  EXPECT_CALL(callbacks, OnSourceRemoved(&source)).Times(1);

  observer->StartObserving();
  coordinator_.RemoveMember(&source);
}

// Test that the OnSourceAdded/Rmoved callback is NOT invoked when a member of a
// different group is added/removed.
TEST_F(LoopbackGroupObserverTest, OnSourceAddedRemoved_NonMatchingGroupId) {
  const auto group_id1 = base::UnguessableToken::Create();
  const auto group_id2 = base::UnguessableToken::Create();
  StrictMock<MockObserverCallbacks> callbacks;
  auto observer = CreateObserver(group_id1, &callbacks);
  MockLoopbackSource source;

  // StrictMock ensures norifications are not propagated.
  observer->StartObserving();
  coordinator_.AddMember(group_id2, &source);
  coordinator_.RemoveMember(&source);
}

// Test that ForEachMember iterates only over members of the observer's group.
TEST_F(LoopbackGroupObserverTest, ForEachMember) {
  const auto group_id1 = base::UnguessableToken::Create();
  const auto group_id2 = base::UnguessableToken::Create();
  StrictMock<MockObserverCallbacks> callbacks;
  auto observer = CreateObserver(group_id1, &callbacks);

  MockLoopbackSource source1_1, source1_2, source2_1;
  coordinator_.AddMember(group_id1, &source1_1);
  coordinator_.AddMember(group_id2, &source2_1);
  coordinator_.AddMember(group_id1, &source1_2);

  std::vector<LoopbackSource*> found_sources;
  observer->ForEachMember(base::BindRepeating(
      [](std::vector<LoopbackSource*>* found_sources, LoopbackSource* source) {
        found_sources->push_back(source);
      },
      &found_sources));

  EXPECT_THAT(found_sources,
              testing::UnorderedElementsAre(&source1_1, &source1_2));

  coordinator_.RemoveMember(&source1_1);
  coordinator_.RemoveMember(&source2_1);
  coordinator_.RemoveMember(&source1_2);
}

// Test that no notifications are received after StopObserving() is called.
TEST_F(LoopbackGroupObserverTest, StopObserving) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockObserverCallbacks> callbacks;
  auto observer = CreateObserver(group_id, &callbacks);
  MockLoopbackSource source;

  observer->StartObserving();
  observer->StopObserving();

  // StrictMock ensures norifications are not propagated.
  coordinator_.AddMember(group_id, &source);
  coordinator_.RemoveMember(&source);
}

// Test that the observer correctly unregisters itself upon destruction.
TEST_F(LoopbackGroupObserverTest, Destruction) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockObserverCallbacks> callbacks;
  auto observer = CreateObserver(group_id, &callbacks);
  MockLoopbackSource source;

  observer->StartObserving();
  observer.reset();  // Destroy the observer.

  // No crash should occur here, and no mock methods should be called.
  coordinator_.AddMember(group_id, &source);
  coordinator_.RemoveMember(&source);
}

// Test that an observer handles members that existed before it started
// observing. It should not receive add notifications for them, but should
// be able to iterate over them.
TEST_F(LoopbackGroupObserverTest, ObservesPreExistingMembers) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockObserverCallbacks> callbacks;
  MockLoopbackSource source1, source2;

  // Add a member before the observer starts.
  coordinator_.AddMember(group_id, &source1);

  auto observer = CreateObserver(group_id, &callbacks);

  // The observer should not be notified of pre-existing members.
  EXPECT_CALL(callbacks, OnSourceAdded(_)).Times(0);
  observer->StartObserving();
  testing::Mock::VerifyAndClearExpectations(&callbacks);

  // But it should be able to iterate over them.
  std::vector<LoopbackSource*> found_sources;
  observer->ForEachMember(base::BindRepeating(
      [](std::vector<LoopbackSource*>* found_sources, LoopbackSource* source) {
        found_sources->push_back(source);
      },
      &found_sources));
  EXPECT_THAT(found_sources, testing::ElementsAre(&source1));

  // And it should be notified of new members.
  EXPECT_CALL(callbacks, OnSourceAdded(&source2)).Times(1);
  coordinator_.AddMember(group_id, &source2);

  observer->StopObserving();
  coordinator_.RemoveMember(&source2);
  coordinator_.RemoveMember(&source1);
}

// Test that multiple observers on the same group all receive notifications.
TEST_F(LoopbackGroupObserverTest, MultipleObserversOnSameGroup) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockObserverCallbacks> callbacks1, callbacks2;
  auto observer1 = CreateObserver(group_id, &callbacks1);
  auto observer2 = CreateObserver(group_id, &callbacks2);
  MockLoopbackSource source;

  observer1->StartObserving();
  observer2->StartObserving();

  EXPECT_CALL(callbacks1, OnSourceAdded(&source)).Times(1);
  EXPECT_CALL(callbacks2, OnSourceAdded(&source)).Times(1);
  coordinator_.AddMember(group_id, &source);
  testing::Mock::VerifyAndClearExpectations(&callbacks1);
  testing::Mock::VerifyAndClearExpectations(&callbacks2);

  EXPECT_CALL(callbacks1, OnSourceRemoved(&source)).Times(1);
  EXPECT_CALL(callbacks2, OnSourceRemoved(&source)).Times(1);
  coordinator_.RemoveMember(&source);
}

// Test that calling StartObserving and StopObserving multiple times is safe.
TEST_F(LoopbackGroupObserverTest, StartAndStopObservingIsRobust) {
  const auto group_id = base::UnguessableToken::Create();
  StrictMock<MockObserverCallbacks> callbacks;
  auto observer = CreateObserver(group_id, &callbacks);
  MockLoopbackSource source1, source2;

  // Multiple starts should be fine.
  observer->StartObserving();
  observer->StartObserving();

  EXPECT_CALL(callbacks, OnSourceAdded(&source1)).Times(1);
  coordinator_.AddMember(group_id, &source1);
  testing::Mock::VerifyAndClearExpectations(&callbacks);

  // Multiple stops should be fine.
  observer->StopObserving();
  observer->StopObserving();

  // No notifications should be received after stopping.
  EXPECT_CALL(callbacks, OnSourceAdded(_)).Times(0);
  EXPECT_CALL(callbacks, OnSourceRemoved(_)).Times(0);
  coordinator_.AddMember(group_id, &source2);
  coordinator_.RemoveMember(&source1);
  coordinator_.RemoveMember(&source2);
}

}  // namespace audio
