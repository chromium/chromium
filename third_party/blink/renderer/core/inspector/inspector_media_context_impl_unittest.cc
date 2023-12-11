// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class InspectorMediaContextImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dummy_page_holder_ =
        std::make_unique<DummyPageHolder>(gfx::Size(), nullptr, nullptr);
    impl = MediaInspectorContextImpl::From(
        *dummy_page_holder_->GetFrame().DomWindow());
  }

  InspectorPlayerEvents MakeEvents(size_t ev_count) {
    InspectorPlayerEvents to_add;
    while (ev_count-- > 0) {
      blink::InspectorPlayerEvent ev = {base::TimeTicks::Now(), "foo"};
      to_add.emplace_back(std::move(ev));
    }
    return to_add;
  }

  test::TaskEnvironment task_environment_;

  Persistent<MediaInspectorContextImpl> impl;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(InspectorMediaContextImplTest, CanCreatePlayerAndAddEvents) {
  auto id = impl->CreatePlayer();
  auto* players = impl->GetPlayersForTesting();
  EXPECT_EQ(players->size(), 1u);
  EXPECT_TRUE(players->at(id)->errors.empty());
  EXPECT_TRUE(players->at(id)->events.empty());
  EXPECT_TRUE(players->at(id)->messages.empty());
  EXPECT_TRUE(players->at(id)->properties.empty());

  impl->NotifyPlayerEvents(id, MakeEvents(10));
  EXPECT_EQ(players->at(id)->events.size(), wtf_size_t{10});
}

TEST_F(InspectorMediaContextImplTest, KillsPlayersInCorrectOrder) {
  auto alive_player_id = impl->CreatePlayer();
  auto expendable_player_id = impl->CreatePlayer();
  // Also marks the alive / expendable players as sent.
  ASSERT_EQ(impl->AllPlayerIdsAndMarkSent().size(), wtf_size_t{2});

  // These are created, but unsent.
  auto dead_player_id = impl->CreatePlayer();
  auto unsent_player_id = impl->CreatePlayer();

  // check that there are 4.
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{4});

  // mark these as dead to get them into their respective states.
  impl->DestroyPlayer(dead_player_id);
  impl->DestroyPlayer(expendable_player_id);

  // check that there are still 4.
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{4});

  // Almost fill up the total cache size.
  impl->NotifyPlayerEvents(dead_player_id, MakeEvents(10));
  impl->NotifyPlayerEvents(unsent_player_id, MakeEvents(10));
  impl->NotifyPlayerEvents(expendable_player_id, MakeEvents(10));
  impl->NotifyPlayerEvents(alive_player_id,
                           MakeEvents(kMaxCachedPlayerEvents - 32));

  EXPECT_EQ(impl->GetTotalEventCountForTesting(), kMaxCachedPlayerEvents - 2);
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{4});

  // If we keep adding events to the alive player in groups of 10, it should
  // delete the other players in the order: dead, expendable, unsent.
  impl->NotifyPlayerEvents(alive_player_id, MakeEvents(10));

  // The number of events remains unchanged, players at 3, and no dead id.
  EXPECT_EQ(impl->GetTotalEventCountForTesting(), kMaxCachedPlayerEvents - 2);
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{3});
  EXPECT_FALSE(impl->GetPlayersForTesting()->Contains(dead_player_id));

  // Kill the expendable player.
  impl->NotifyPlayerEvents(alive_player_id, MakeEvents(10));

  // The number of events remains unchanged, players at 2, and no expendable id.
  EXPECT_EQ(impl->GetTotalEventCountForTesting(), kMaxCachedPlayerEvents - 2);
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{2});
  EXPECT_FALSE(impl->GetPlayersForTesting()->Contains(expendable_player_id));

  // Kill the unsent player.
  impl->NotifyPlayerEvents(alive_player_id, MakeEvents(10));

  // The number of events remains unchanged, players at 1, and no unsent id.
  EXPECT_EQ(impl->GetTotalEventCountForTesting(), kMaxCachedPlayerEvents - 2);
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{1});
  EXPECT_FALSE(impl->GetPlayersForTesting()->Contains(unsent_player_id));

  // Overflow the the cache and start trimming events.
  impl->NotifyPlayerEvents(alive_player_id, MakeEvents(10));

  // The number of events remains unchanged, players at 1, and no unsent id.
  EXPECT_EQ(impl->GetTotalEventCountForTesting(), kMaxCachedPlayerEvents);
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{1});
  EXPECT_TRUE(impl->GetPlayersForTesting()->Contains(alive_player_id));
}

TEST_F(InspectorMediaContextImplTest, OkToSendForDeadPlayers) {
  auto player_1 = impl->CreatePlayer();
  auto player_2 = impl->CreatePlayer();
  ASSERT_EQ(impl->AllPlayerIdsAndMarkSent().size(), wtf_size_t{2});

  // This should evict player1.
  impl->NotifyPlayerEvents(player_1, MakeEvents(kMaxCachedPlayerEvents - 1));
  impl->NotifyPlayerEvents(player_2, MakeEvents(10));
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{1});
  EXPECT_FALSE(impl->GetPlayersForTesting()->Contains(player_1));

  // Sending events to an evicted player shouldn't cause the cache size to
  // increase, or any new evictions to happen.
  impl->NotifyPlayerEvents(player_1, MakeEvents(kMaxCachedPlayerEvents - 1));
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{1});
  EXPECT_FALSE(impl->GetPlayersForTesting()->Contains(player_1));
}

TEST_F(InspectorMediaContextImplTest, TrimLastRemainingPlayer) {
  auto player_1 = impl->CreatePlayer();
  ASSERT_EQ(impl->AllPlayerIdsAndMarkSent().size(), wtf_size_t{1});

  impl->NotifyPlayerEvents(player_1, MakeEvents(kMaxCachedPlayerEvents - 1));
  impl->NotifyPlayerEvents(player_1, MakeEvents(kMaxCachedPlayerEvents - 1));
  EXPECT_EQ(impl->GetPlayersForTesting()->size(), wtf_size_t{1});
  EXPECT_TRUE(impl->GetPlayersForTesting()->Contains(player_1));
  EXPECT_EQ(impl->GetTotalEventCountForTesting(), kMaxCachedPlayerEvents);
}

}  // namespace
}  // namespace blink
