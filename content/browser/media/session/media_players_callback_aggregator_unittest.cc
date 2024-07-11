// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_players_callback_aggregator.h"

#include "base/not_fatal_until.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// A simple `FakeMediaSession` class to simulate GetVisibility. This class takes
// as a constructor a vector of `desired_visibilities`, where the indices
// represent `player_id` s and the values the desired visibility to return to
// callers.
//
// This class also uses `PostTask` during `SimulateGetVisibility` to simulate
// asynchronous behavior.
class FakeMediaSession {
 public:
  using TaskCb = base::OnceCallback<void(bool)>;

  explicit FakeMediaSession(std::vector<bool> const& desired_visibilities) {
    for (size_t index = 0; index < desired_visibilities.size(); ++index) {
      bool desired_visibility = desired_visibilities[index];
      fake_players_.insert(std::pair{index, desired_visibility});
    }
  }

  FakeMediaSession(const FakeMediaSession&) = delete;
  FakeMediaSession(FakeMediaSession&&) = delete;
  FakeMediaSession& operator=(const FakeMediaSession&) = delete;

  void SimulateGetVisibility(
      scoped_refptr<MediaPlayersCallbackAggregator> aggregator) {
    total_players_count_ = base::saturated_cast<int>(fake_players_.size());
    callbacks_executed_count_ = 0;
    done_ = false;

    for (const auto fake_player : fake_players_) {
      int player_id = fake_player.first;
      TaskCb task = aggregator->CreateVisibilityCallback();

      // base::Unretained() is safe since no callbacks will be executed after
      // exiting the `RunLoop`.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&FakeMediaSession::OnRequestVisibility,
                         base::Unretained(this), player_id, std::move(task)));
    }
    WaitUntilDone();
  }

  void OnRequestVisibility(int player_id,
                           base::OnceCallback<void(bool)> get_visibility_cb) {
    auto it = fake_players_.find(player_id);
    CHECK(it != fake_players_.end(), base::NotFatalUntil::M130);

    bool desired_visibility = it->second;
    std::move(get_visibility_cb).Run(desired_visibility);
    callbacks_executed_count_++;

    if (desired_visibility ||
        callbacks_executed_count_ == total_players_count_) {
      done_ = true;
      wait_until_done_loop_.Quit();
    }
  }

  void WaitUntilDone() {
    if (done_ || callbacks_executed_count_ == total_players_count_) {
      return;
    }
    wait_until_done_loop_.Run();
  }

  int TotalPlayersCount() const { return total_players_count_; }

  int CallbacksExecutedCount() const { return callbacks_executed_count_; }

 private:
  std::map<int, bool> fake_players_;
  base::RunLoop wait_until_done_loop_;
  bool done_ = false;
  int total_players_count_ = 0;
  int callbacks_executed_count_ = 0;
};

class MediaPlayersCallbackAggregatorTest : public testing::Test {
 public:
  using ReportVisibilityCb = base::OnceCallback<void(bool)>;

  MediaPlayersCallbackAggregatorTest() = default;
  MediaPlayersCallbackAggregatorTest(
      const MediaPlayersCallbackAggregatorTest&) = delete;
  MediaPlayersCallbackAggregatorTest(MediaPlayersCallbackAggregatorTest&&) =
      delete;
  MediaPlayersCallbackAggregatorTest& operator=(
      const MediaPlayersCallbackAggregatorTest&) = delete;

  scoped_refptr<MediaPlayersCallbackAggregator> CreateAggregator() {
    // base::Unretained() is safe since `ReportVisibilityCb` will always be
    // executed at destruction time (if it is not null).
    ReportVisibilityCb report_visibility_cb =
        base::BindOnce(&MediaPlayersCallbackAggregatorTest::GetVisibility,
                       base::Unretained(this));
    return MakeRefCounted<MediaPlayersCallbackAggregator>(
        std::move(report_visibility_cb));
  }

  void GetVisibility(bool meets_visibility) {
    meets_visibility_ = meets_visibility;
    report_visibility_cb_executed_ = true;
  }

  void WaitUntilReportVisibilityCbRuns() {
    if (report_visibility_cb_executed_) {
      return;
    }
    wait_until_report_visibility_cb_runs_loop_.Run();
  }

  bool ReportVisibilityCbExecuted() const {
    return report_visibility_cb_executed_;
  }

  bool MeetsVisibility() const { return meets_visibility_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop wait_until_report_visibility_cb_runs_loop_;
  bool report_visibility_cb_executed_ = false;
  bool meets_visibility_ = false;
};

}  // anonymous namespace

TEST_F(MediaPlayersCallbackAggregatorTest, DoesNotCrashWithEmptyPlayers) {
  // Create fake media session with no players.
  FakeMediaSession fake_media_session({});

  // Simulate get visibility and verify that: no players were created and, no
  // callbacks were executed.
  fake_media_session.SimulateGetVisibility(CreateAggregator());
  EXPECT_EQ(0, fake_media_session.TotalPlayersCount());
  EXPECT_EQ(0, fake_media_session.CallbacksExecutedCount());

  // Wait for ReportVisibilityCb to run and verify expectations.
  WaitUntilReportVisibilityCbRuns();
  EXPECT_TRUE(ReportVisibilityCbExecuted());
  EXPECT_FALSE(MeetsVisibility());
}

TEST_F(MediaPlayersCallbackAggregatorTest, MultiplePlayersAllMeetVisibility) {
  // Create fake media session with multiple players. All players meet
  // visibility.
  FakeMediaSession fake_media_session({true, true, true});

  // Simulate get visibility and verify that: all players were created and, a
  // single TaskCb is executed.
  fake_media_session.SimulateGetVisibility(CreateAggregator());
  EXPECT_EQ(3, fake_media_session.TotalPlayersCount());
  EXPECT_EQ(1, fake_media_session.CallbacksExecutedCount());

  // Wait for ReportVisibilityCb to run and verify expectations.
  WaitUntilReportVisibilityCbRuns();
  EXPECT_TRUE(ReportVisibilityCbExecuted());
  EXPECT_TRUE(MeetsVisibility());
}

TEST_F(MediaPlayersCallbackAggregatorTest,
       MultiplePlayersAllDoNotMeetVisibility) {
  // Create fake media session with multiple players. All players do not meet
  // visibility.
  FakeMediaSession fake_media_session({false, false, false});

  // Simulate get visibility and verify that: all players were created and, all
  // TaskCb s are executed.
  fake_media_session.SimulateGetVisibility(CreateAggregator());
  EXPECT_EQ(3, fake_media_session.TotalPlayersCount());
  EXPECT_EQ(3, fake_media_session.CallbacksExecutedCount());

  // Wait for ReportVisibilityCb to run and verify expectations.
  WaitUntilReportVisibilityCbRuns();
  EXPECT_TRUE(ReportVisibilityCbExecuted());
  EXPECT_FALSE(MeetsVisibility());
}

TEST_F(MediaPlayersCallbackAggregatorTest, MultiplePlayersSomeMeetVisibility) {
  // Create fake media session with multiple players. Some of the players meet
  // visibility.
  FakeMediaSession fake_media_session({false, false, false, true, true});

  // Simulate get visibility and verify that: all players were created and,
  // TaskCb s stop executing once the first player that meets visibility is
  // found.
  fake_media_session.SimulateGetVisibility(CreateAggregator());
  EXPECT_EQ(5, fake_media_session.TotalPlayersCount());
  EXPECT_EQ(4, fake_media_session.CallbacksExecutedCount());

  // Wait for ReportVisibilityCb to run and verify expectations.
  WaitUntilReportVisibilityCbRuns();
  EXPECT_TRUE(ReportVisibilityCbExecuted());
  EXPECT_TRUE(MeetsVisibility());
}

TEST_F(MediaPlayersCallbackAggregatorTest,
       MultiplePlayersStopsAtFirstMeetVisibility) {
  // Create fake media session with multiple players. Only the first player
  // meets visibility.
  FakeMediaSession fake_media_session({true, false, false, false, false});

  // Simulate get visibility and verify that: all players were created and,
  // TaskCb s stop executing once the first player that meets visibility is
  // found.
  fake_media_session.SimulateGetVisibility(CreateAggregator());
  EXPECT_EQ(5, fake_media_session.TotalPlayersCount());
  EXPECT_EQ(1, fake_media_session.CallbacksExecutedCount());

  // Wait for ReportVisibilityCb to run and verify expectations.
  WaitUntilReportVisibilityCbRuns();
  EXPECT_TRUE(ReportVisibilityCbExecuted());
  EXPECT_TRUE(MeetsVisibility());
}

}  // namespace content
