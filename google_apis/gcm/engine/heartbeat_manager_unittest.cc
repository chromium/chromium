// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/heartbeat_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

mcs_proto::HeartbeatConfig BuildHeartbeatConfig(int interval_ms) {
  mcs_proto::HeartbeatConfig config;
  config.set_interval_ms(interval_ms);
  return config;
}

class TestHeartbeatManager : public HeartbeatManager {
 public:
  TestHeartbeatManager(scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                       scoped_refptr<base::SequencedTaskRunner>
                           maybe_power_wrapped_io_task_runner)
      : HeartbeatManager(io_task_runner, maybe_power_wrapped_io_task_runner) {}
  ~TestHeartbeatManager() override {}

  // Bypass the heartbeat timer, and send the heartbeat now.
  void TriggerHearbeat();

  // Check for a missed heartbeat now.
  void TriggerMissedHeartbeatCheck();
};

void TestHeartbeatManager::TriggerHearbeat() {
  OnHeartbeatTriggered();
}

void TestHeartbeatManager::TriggerMissedHeartbeatCheck() {
  CheckForMissedHeartbeat();
}

class HeartbeatManagerTest : public testing::Test {
 public:
  HeartbeatManagerTest();
  ~HeartbeatManagerTest() override {}

  TestHeartbeatManager* manager() const { return manager_.get(); }
  int heartbeats_sent() const { return heartbeats_sent_; }
  int reconnects_triggered() const { return reconnects_triggered_; }

  // Starts the heartbeat manager.
  void StartManager();

 private:
  // Helper functions for verifying heartbeat manager effects.
  void SendHeartbeatClosure();
  void TriggerReconnectClosure(ConnectionFactory::ConnectionResetReason reason);

  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<TestHeartbeatManager> manager_;

  int heartbeats_sent_;
  int reconnects_triggered_;
};

HeartbeatManagerTest::HeartbeatManagerTest()
    : manager_(new TestHeartbeatManager(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::SequencedTaskRunner::GetCurrentDefault())),
      heartbeats_sent_(0),
      reconnects_triggered_(0) {}

void HeartbeatManagerTest::StartManager() {
  manager_->Start(
      base::BindRepeating(&HeartbeatManagerTest::SendHeartbeatClosure,
                          base::Unretained(this)),
      base::BindRepeating(&HeartbeatManagerTest::TriggerReconnectClosure,
                          base::Unretained(this)));
}

void HeartbeatManagerTest::SendHeartbeatClosure() {
  heartbeats_sent_++;
}

void HeartbeatManagerTest::TriggerReconnectClosure(
    ConnectionFactory::ConnectionResetReason reason) {
  reconnects_triggered_++;
}

// Basic initialization. No heartbeat should be pending.
TEST_F(HeartbeatManagerTest, Init) {
  EXPECT_TRUE(manager()->GetNextHeartbeatTime().is_null());
}

// Acknowledging a heartbeat before starting the manager should have no effect.
TEST_F(HeartbeatManagerTest, AckBeforeStart) {
  manager()->OnHeartbeatAcked();
  EXPECT_TRUE(manager()->GetNextHeartbeatTime().is_null());
}

// Starting the manager should start the heartbeat timer.
TEST_F(HeartbeatManagerTest, Start) {
  StartManager();
  EXPECT_GT(manager()->GetNextHeartbeatTime(), base::TimeTicks::Now());
  EXPECT_EQ(0, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());
}

// Acking the heartbeat should trigger a new heartbeat timer.
TEST_F(HeartbeatManagerTest, AckedHeartbeat) {
  StartManager();
  manager()->TriggerHearbeat();
  base::TimeTicks heartbeat = manager()->GetNextHeartbeatTime();
  EXPECT_GT(heartbeat, base::TimeTicks::Now());
  EXPECT_EQ(1, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());

  manager()->OnHeartbeatAcked();
  EXPECT_LT(heartbeat, manager()->GetNextHeartbeatTime());
  EXPECT_EQ(1, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());

  manager()->TriggerHearbeat();
  EXPECT_EQ(2, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());
}

// Trigger a heartbeat when one was outstanding should reset the connection.
TEST_F(HeartbeatManagerTest, UnackedHeartbeat) {
  StartManager();
  manager()->TriggerHearbeat();
  EXPECT_EQ(1, heartbeats_sent());
  EXPECT_EQ(0, reconnects_triggered());

  manager()->TriggerHearbeat();
  EXPECT_EQ(1, heartbeats_sent());
  EXPECT_EQ(1, reconnects_triggered());
}

// Updating the heartbeat interval before starting should result in the new
// interval being used at Start time.
TEST_F(HeartbeatManagerTest, UpdateIntervalThenStart) {
  const int kIntervalMs = 60 * 1000;  // 60 seconds.
  manager()->UpdateHeartbeatConfig(BuildHeartbeatConfig(kIntervalMs));
  EXPECT_TRUE(manager()->GetNextHeartbeatTime().is_null());
  StartManager();
  EXPECT_LE(manager()->GetNextHeartbeatTime() - base::TimeTicks::Now(),
            base::Milliseconds(kIntervalMs));
}

// Updating the heartbeat interval after starting should only use the new
// interval on the next heartbeat.
TEST_F(HeartbeatManagerTest, StartThenUpdateInterval) {
  const int kIntervalMs = 60 * 1000;  // 60 seconds.
  StartManager();
  base::TimeTicks heartbeat = manager()->GetNextHeartbeatTime();
  EXPECT_GT(heartbeat - base::TimeTicks::Now(),
            base::Milliseconds(kIntervalMs));

  // Updating the interval should not affect an outstanding heartbeat.
  manager()->UpdateHeartbeatConfig(BuildHeartbeatConfig(kIntervalMs));
  EXPECT_EQ(heartbeat, manager()->GetNextHeartbeatTime());

  // Triggering and acking the heartbeat should result in a heartbeat being
  // posted with the new interval.
  manager()->TriggerHearbeat();
  manager()->OnHeartbeatAcked();

  EXPECT_LE(manager()->GetNextHeartbeatTime() - base::TimeTicks::Now(),
            base::Milliseconds(kIntervalMs));
  EXPECT_NE(heartbeat, manager()->GetNextHeartbeatTime());
}

// Stopping the manager should reset the heartbeat timer.
TEST_F(HeartbeatManagerTest, Stop) {
  StartManager();
  EXPECT_GT(manager()->GetNextHeartbeatTime(), base::TimeTicks::Now());

  manager()->Stop();
  EXPECT_TRUE(manager()->GetNextHeartbeatTime().is_null());
}

// Simulate missing a heartbeat by manually invoking the check method. The
// heartbeat should only be triggered once, and only if the heartbeat timer
// is running. Because the period is several minutes, none should fire.
TEST_F(HeartbeatManagerTest, MissedHeartbeat) {
  // Do nothing while stopped.
  manager()->TriggerMissedHeartbeatCheck();
  StartManager();
  EXPECT_EQ(0, heartbeats_sent());

  // Do nothing before the period is reached.
  manager()->TriggerMissedHeartbeatCheck();
  EXPECT_EQ(0, heartbeats_sent());
}

// Sets the client hearbeat interval and checks that it is picked up by the
// manager.
TEST_F(HeartbeatManagerTest, SetClientHeartbeatInterval) {
  const int kIntervalMs = 180 * 1000;  // 180 seconds.
  StartManager();
  manager()->TriggerHearbeat();
  manager()->OnHeartbeatAcked();

  base::TimeTicks heartbeat = manager()->GetNextHeartbeatTime();
  EXPECT_GT(heartbeat - base::TimeTicks::Now(),
            base::Milliseconds(kIntervalMs));

  manager()->SetClientHeartbeatIntervalMs(kIntervalMs);
  EXPECT_EQ(1, reconnects_triggered());

  // Triggering and acking the heartbeat should result in a heartbeat being
  // posted with the new interval.
  manager()->TriggerHearbeat();
  manager()->OnHeartbeatAcked();

  EXPECT_LE(manager()->GetNextHeartbeatTime() - base::TimeTicks::Now(),
            base::Milliseconds(kIntervalMs));
  EXPECT_GT(heartbeat, manager()->GetNextHeartbeatTime());

  const int kLongerIntervalMs = 2 * kIntervalMs;
  // Updating the interval should not affect an outstanding heartbeat.
  manager()->SetClientHeartbeatIntervalMs(kLongerIntervalMs);
  // No extra reconnects happen here, because the heartbeat is longer.
  EXPECT_EQ(1, reconnects_triggered());

  // Triggering and acking the heartbeat should result in a heartbeat being
  // posted with the old, shorter interval.
  manager()->TriggerHearbeat();
  manager()->OnHeartbeatAcked();

  EXPECT_LE(manager()->GetNextHeartbeatTime() - base::TimeTicks::Now(),
            base::Milliseconds(kIntervalMs));
}

// Verifies that setting the client interval too low or too high will set it to
// a value within a reasonable scope.
TEST_F(HeartbeatManagerTest, ClientIntervalInvalid) {
  // Less than min value.
  int interval_ms = manager()->GetMinClientHeartbeatIntervalMs() - 60 * 1000;
  manager()->SetClientHeartbeatIntervalMs(interval_ms);
  EXPECT_TRUE(manager()->GetNextHeartbeatTime().is_null());
  StartManager();
  base::TimeDelta till_heartbeat = manager()->GetNextHeartbeatTime() -
      base::TimeTicks::Now();
  EXPECT_GT(till_heartbeat,
            base::Milliseconds(manager()->GetMinClientHeartbeatIntervalMs()));
  EXPECT_LE(till_heartbeat,
            base::Milliseconds(manager()->GetMaxClientHeartbeatIntervalMs()));

  // More than max value.
  interval_ms = manager()->GetMaxClientHeartbeatIntervalMs() + 60 * 1000;
  // Triggering and acking the heartbeat should result in a heartbeat being
  // posted with the new interval.
  manager()->TriggerHearbeat();
  manager()->OnHeartbeatAcked();

  till_heartbeat = manager()->GetNextHeartbeatTime() - base::TimeTicks::Now();
  EXPECT_LE(till_heartbeat,
            base::Milliseconds(manager()->GetMaxClientHeartbeatIntervalMs()));
}

// Verifies that client interval is reset appropriately after the heartbeat is
// triggered. See http://crbug.com/591490 for details.
TEST_F(HeartbeatManagerTest, ClientIntervalAfterHeartbeatTriggered) {
  const int kCustomIntervalMs = 180 * 1000;  // 180 seconds.
  manager()->SetClientHeartbeatIntervalMs(kCustomIntervalMs);
  StartManager();

  // This changes the interval as manager awaits a heartbeat ack.
  manager()->TriggerHearbeat();
  const int kDefaultAckIntervalMs = 60 * 1000;  // 60 seconds.
  base::TimeTicks heartbeat = manager()->GetNextHeartbeatTime();
  EXPECT_LE(heartbeat - base::TimeTicks::Now(),
            base::Milliseconds(kDefaultAckIntervalMs));

  // This should reset the interval to the custom interval.
  manager()->OnHeartbeatAcked();
  heartbeat = manager()->GetNextHeartbeatTime();
  EXPECT_GT(heartbeat - base::TimeTicks::Now(),
            base::Milliseconds(kDefaultAckIntervalMs));
  EXPECT_LE(heartbeat - base::TimeTicks::Now(),
            base::Milliseconds(kCustomIntervalMs));
}

}  // namespace

}  // namespace gcm
