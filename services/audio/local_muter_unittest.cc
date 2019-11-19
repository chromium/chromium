// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/local_muter.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "services/audio/loopback_coordinator.h"
#include "services/audio/loopback_group_member.h"
#include "services/audio/test/mock_group_member.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UnguessableToken;

using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::StrictMock;

namespace audio {
namespace {

TEST(LocalMuterTest, MutesExistingMembers) {
  LoopbackCoordinator coordinator;

  // Create a group with two members.
  const UnguessableToken group_id = UnguessableToken::Create();
  StrictMock<MockGroupMember> member1;
  StrictMock<MockGroupMember> member2;

  // Create another group with one member, which should never have its mute
  // state changed.
  const UnguessableToken other_group_id = UnguessableToken::Create();
  ASSERT_NE(group_id, other_group_id);
  StrictMock<MockGroupMember> non_member;
  EXPECT_CALL(non_member, StartMuting()).Times(0);
  EXPECT_CALL(non_member, StopMuting()).Times(0);

  // When the members join the group, no mute change should occur.
  coordinator.RegisterMember(group_id, &member1);
  coordinator.RegisterMember(group_id, &member2);

  // When the muter is created, both members should be muted.
  EXPECT_CALL(member1, StartMuting());
  EXPECT_CALL(member2, StartMuting());
  auto muter = std::make_unique<LocalMuter>(&coordinator, group_id);
  Mock::VerifyAndClearExpectations(&member1);
  Mock::VerifyAndClearExpectations(&member2);

  // When the muter is destroyed, both members should be un-muted.
  EXPECT_CALL(member1, StopMuting());
  EXPECT_CALL(member2, StopMuting());
  muter = nullptr;
  Mock::VerifyAndClearExpectations(&member1);
  Mock::VerifyAndClearExpectations(&member2);

  coordinator.UnregisterMember(group_id, &member1);
  coordinator.UnregisterMember(group_id, &member2);
}

TEST(LocalMuterTest, MutesJoiningMembers) {
  LoopbackCoordinator coordinator;
  const UnguessableToken group_id = UnguessableToken::Create();

  LocalMuter muter(&coordinator, group_id);

  StrictMock<MockGroupMember> member;

  // Since muting is in-effect, the group member is immediately muted when
  // joining the group.
  EXPECT_CALL(member, StartMuting());
  coordinator.RegisterMember(group_id, &member);
  Mock::VerifyAndClearExpectations(&member);

  // Leaving the group should have no effect on the mute state of the member.
  EXPECT_CALL(member, StartMuting()).Times(0);
  EXPECT_CALL(member, StopMuting()).Times(0);
  coordinator.UnregisterMember(group_id, &member);
  Mock::VerifyAndClearExpectations(&member);
}

TEST(LocalMuter, UnmutesWhenLastBindingIsLost) {
  base::test::TaskEnvironment task_environment;
  LoopbackCoordinator coordinator;
  const UnguessableToken group_id = UnguessableToken::Create();

  // Later in this test, once both bindings have been closed, the following
  // callback should be run. The callback will delete the LocalMuter in the same
  // stack as the mojo connection error handler, just as would take place in the
  // live build.
  auto muter = std::make_unique<LocalMuter>(&coordinator, group_id);
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run()).WillOnce(InvokeWithoutArgs([&muter]() {
    muter.reset();
  }));
  muter->SetAllBindingsLostCallback(callback.Get());

  // Create two bindings to the muter.
  mojo::AssociatedRemote<mojom::LocalMuter> remote_muter1;
  muter->AddReceiver(remote_muter1.BindNewEndpointAndPassReceiver());
  mojo::AssociatedRemote<mojom::LocalMuter> remote_muter2;
  muter->AddReceiver(remote_muter2.BindNewEndpointAndPassReceiver());

  // A member joins the group and should be muted.
  StrictMock<MockGroupMember> member;
  EXPECT_CALL(member, StartMuting());
  coordinator.RegisterMember(group_id, &member);
  Mock::VerifyAndClearExpectations(&member);

  // Nothing happens to the member when one of the bindings is closed.
  EXPECT_CALL(member, StopMuting()).Times(0);
  remote_muter1.reset();
  task_environment.RunUntilIdle();  // Propagate mojo tasks.
  Mock::VerifyAndClearExpectations(&member);

  // The member is unmuted once the second binding is closed.
  EXPECT_CALL(member, StopMuting());
  remote_muter2.reset();
  task_environment.RunUntilIdle();  // Propagate mojo tasks.
  Mock::VerifyAndClearExpectations(&member);

  // At this point, the LocalMuter should have been destroyed.
  EXPECT_FALSE(muter);

  coordinator.UnregisterMember(group_id, &member);
}

}  // namespace
}  // namespace audio
