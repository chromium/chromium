// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/test/mock_group_coordinator.h"

#include "base/containers/contains.h"
#include "base/unguessable_token.h"
#include "services/audio/test/mock_group_member.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UnguessableToken;

using testing::AtLeast;
using testing::NiceMock;
using testing::ReturnRef;
using testing::Sequence;
using testing::StrictMock;
using testing::_;

namespace audio {

class TestGroupCoordinator : public GroupCoordinator<MockGroupMember> {
 public:
  const std::vector<MockGroupMember*>& GetCurrentMembers(
      const base::UnguessableToken& group_id) const {
    return GetCurrentMembersUnsafe(group_id);
  }
};

namespace {

class MockGroupObserver : public TestGroupCoordinator::Observer {
 public:
  MockGroupObserver() = default;

  MockGroupObserver(const MockGroupObserver&) = delete;
  MockGroupObserver& operator=(const MockGroupObserver&) = delete;

  ~MockGroupObserver() override = default;

  MOCK_METHOD1(OnMemberJoinedGroup, void(MockGroupMember* member));
  MOCK_METHOD1(OnMemberLeftGroup, void(MockGroupMember* member));
};

TEST(GroupCoordinatorTest, NeverUsed) {
  TestGroupCoordinator coordinator;
}

TEST(GroupCoordinatorTest, RegistersMembersInSameGroup) {
  const UnguessableToken group_id = UnguessableToken::Create();
  StrictMock<MockGroupMember> member1;
  StrictMock<MockGroupMember> member2;

  // An observer should see each member join and leave the group once.
  StrictMock<MockGroupObserver> observer;
  Sequence join_leave_sequence;
  EXPECT_CALL(observer, OnMemberJoinedGroup(&member1))
      .InSequence(join_leave_sequence);
  EXPECT_CALL(observer, OnMemberJoinedGroup(&member2))
      .InSequence(join_leave_sequence);
  EXPECT_CALL(observer, OnMemberLeftGroup(&member1))
      .InSequence(join_leave_sequence);
  EXPECT_CALL(observer, OnMemberLeftGroup(&member2))
      .InSequence(join_leave_sequence);

  TestGroupCoordinator coordinator;
  coordinator.AddObserver(group_id, &observer);
  coordinator.RegisterMember(group_id, &member1);
  coordinator.RegisterMember(group_id, &member2);

  const std::vector<MockGroupMember*>& members =
      coordinator.GetCurrentMembers(group_id);
  EXPECT_EQ(2u, members.size());
  EXPECT_TRUE(base::Contains(members, &member1));
  EXPECT_TRUE(base::Contains(members, &member2));
  EXPECT_TRUE(
      coordinator.GetCurrentMembers(UnguessableToken::Create()).empty());

  coordinator.UnregisterMember(group_id, &member1);
  coordinator.UnregisterMember(group_id, &member2);
  EXPECT_TRUE(coordinator.GetCurrentMembers(group_id).empty());

  coordinator.RemoveObserver(group_id, &observer);
  EXPECT_TRUE(coordinator.GetCurrentMembers(group_id).empty());
}

TEST(GroupCoordinatorTest, RegistersMembersInDifferentGroups) {
  const UnguessableToken group_id_a = UnguessableToken::Create();
  StrictMock<MockGroupMember> member_a_1;
  StrictMock<MockGroupMember> member_a_2;

  StrictMock<MockGroupObserver> observer_a;
  Sequence join_leave_sequence_a;
  EXPECT_CALL(observer_a, OnMemberJoinedGroup(&member_a_1))
      .InSequence(join_leave_sequence_a);
  EXPECT_CALL(observer_a, OnMemberJoinedGroup(&member_a_2))
      .InSequence(join_leave_sequence_a);
  EXPECT_CALL(observer_a, OnMemberLeftGroup(&member_a_1))
      .InSequence(join_leave_sequence_a);
  EXPECT_CALL(observer_a, OnMemberLeftGroup(&member_a_2))
      .InSequence(join_leave_sequence_a);

  const UnguessableToken group_id_b = UnguessableToken::Create();
  StrictMock<MockGroupMember> member_b_1;

  StrictMock<MockGroupObserver> observer_b;
  Sequence join_leave_sequence_b;
  EXPECT_CALL(observer_b, OnMemberJoinedGroup(&member_b_1))
      .InSequence(join_leave_sequence_b);
  EXPECT_CALL(observer_b, OnMemberLeftGroup(&member_b_1))
      .InSequence(join_leave_sequence_b);

  TestGroupCoordinator coordinator;
  coordinator.AddObserver(group_id_a, &observer_a);
  coordinator.AddObserver(group_id_b, &observer_b);
  coordinator.RegisterMember(group_id_a, &member_a_1);
  coordinator.RegisterMember(group_id_b, &member_b_1);
  coordinator.RegisterMember(group_id_a, &member_a_2);
  const std::vector<MockGroupMember*>& members_a =
      coordinator.GetCurrentMembers(group_id_a);
  EXPECT_EQ(2u, members_a.size());
  EXPECT_TRUE(base::Contains(members_a, &member_a_1));
  EXPECT_TRUE(base::Contains(members_a, &member_a_2));
  EXPECT_EQ(std::vector<MockGroupMember*>({&member_b_1}),
            coordinator.GetCurrentMembers(group_id_b));
  EXPECT_TRUE(
      coordinator.GetCurrentMembers(UnguessableToken::Create()).empty());

  coordinator.UnregisterMember(group_id_a, &member_a_1);
  EXPECT_EQ(std::vector<MockGroupMember*>({&member_a_2}),
            coordinator.GetCurrentMembers(group_id_a));

  coordinator.UnregisterMember(group_id_b, &member_b_1);
  EXPECT_TRUE(coordinator.GetCurrentMembers(group_id_b).empty());

  coordinator.UnregisterMember(group_id_a, &member_a_2);
  EXPECT_TRUE(coordinator.GetCurrentMembers(group_id_a).empty());

  coordinator.RemoveObserver(group_id_a, &observer_a);
  coordinator.RemoveObserver(group_id_b, &observer_b);
  EXPECT_TRUE(coordinator.GetCurrentMembers(group_id_a).empty());
  EXPECT_TRUE(coordinator.GetCurrentMembers(group_id_b).empty());
}

TEST(GroupCoordinatorTest, TracksMembersWithoutAnObserverPresent) {
  const UnguessableToken group_id = UnguessableToken::Create();
  StrictMock<MockGroupMember> member1;
  StrictMock<MockGroupMember> member2;

  TestGroupCoordinator coordinator;
  coordinator.RegisterMember(group_id, &member1);
  coordinator.RegisterMember(group_id, &member2);

  const std::vector<MockGroupMember*>& members =
      coordinator.GetCurrentMembers(group_id);
  EXPECT_EQ(2u, members.size());
  EXPECT_TRUE(base::Contains(members, &member1));
  EXPECT_TRUE(base::Contains(members, &member2));
  EXPECT_TRUE(
      coordinator.GetCurrentMembers(UnguessableToken::Create()).empty());

  coordinator.UnregisterMember(group_id, &member1);
  coordinator.UnregisterMember(group_id, &member2);
  EXPECT_TRUE(coordinator.GetCurrentMembers(group_id).empty());
}

TEST(GroupCoordinatorTest, NotifiesOnlyWhileObserving) {
  const UnguessableToken group_id = UnguessableToken::Create();
  StrictMock<MockGroupMember> member1;
  StrictMock<MockGroupMember> member2;

  // The observer will only be around at the time when member2 joins the group
  // and when member1 leaves the group.
  StrictMock<MockGroupObserver> observer;
  Sequence join_leave_sequence;
  EXPECT_CALL(observer, OnMemberJoinedGroup(&member1)).Times(0);
  EXPECT_CALL(observer, OnMemberJoinedGroup(&member2))
      .InSequence(join_leave_sequence);
  EXPECT_CALL(observer, OnMemberLeftGroup(&member1))
      .InSequence(join_leave_sequence);
  EXPECT_CALL(observer, OnMemberLeftGroup(&member2)).Times(0);

  TestGroupCoordinator coordinator;
  coordinator.RegisterMember(group_id, &member1);
  EXPECT_EQ(std::vector<MockGroupMember*>({&member1}),
            coordinator.GetCurrentMembers(group_id));

  coordinator.AddObserver(group_id, &observer);
  coordinator.RegisterMember(group_id, &member2);
  const std::vector<MockGroupMember*>& members =
      coordinator.GetCurrentMembers(group_id);
  EXPECT_EQ(2u, members.size());
  EXPECT_TRUE(base::Contains(members, &member1));
  EXPECT_TRUE(base::Contains(members, &member2));

  coordinator.UnregisterMember(group_id, &member1);
  EXPECT_EQ(std::vector<MockGroupMember*>({&member2}),
            coordinator.GetCurrentMembers(group_id));

  coordinator.RemoveObserver(group_id, &observer);
  EXPECT_EQ(std::vector<MockGroupMember*>({&member2}),
            coordinator.GetCurrentMembers(group_id));

  coordinator.UnregisterMember(group_id, &member2);
  EXPECT_TRUE(coordinator.GetCurrentMembers(group_id).empty());
}

}  // namespace
}  // namespace audio
