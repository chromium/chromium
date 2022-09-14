// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/c/system/quota.h"
#include "mojo/public/cpp/bindings/features.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {
namespace test {
namespace {

class MessageQuotaCheckerTest : public testing::Test {
 public:
  MessageQuotaCheckerTest() {
    EXPECT_EQ(nullptr, instance_);
    instance_ = this;
  }
  ~MessageQuotaCheckerTest() override {
    EXPECT_EQ(this, instance_);
    instance_ = nullptr;
  }

 protected:
  using MessageQuotaChecker = internal::MessageQuotaChecker;
  using Configuration = MessageQuotaChecker::Configuration;

  static constexpr base::TimeDelta kOneSamplingInterval =
      MessageQuotaChecker::DecayingRateAverage::kSamplingInterval;

  void Advance(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  static void RecordDumpAttempt(size_t total_quota_used,
                                absl::optional<size_t> message_pipe_quota_used,
                                int64_t seconds_since_construction,
                                double average_write_rate,
                                uint64_t messages_enqueued,
                                uint64_t messages_dequeued,
                                uint64_t messages_written) {
    ++instance_->num_dumps_;
    instance_->last_dump_total_quota_used_ = total_quota_used;
    instance_->last_dump_message_pipe_quota_used_ = message_pipe_quota_used;
    instance_->last_seconds_since_construction_ = seconds_since_construction;
    instance_->last_average_write_rate_ = average_write_rate;
    instance_->last_messages_enqueued_ = messages_enqueued;
    instance_->last_messages_dequeued_ = messages_dequeued;
    instance_->last_messages_written_ = messages_written;
  }

  // Mock time to allow testing
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  size_t num_dumps_ = false;
  size_t last_dump_total_quota_used_ = 0u;
  absl::optional<size_t> last_dump_message_pipe_quota_used_;
  int64_t last_seconds_since_construction_ = 0;
  double last_average_write_rate_ = 0.0;
  uint64_t last_messages_enqueued_ = 0u;
  uint64_t last_messages_dequeued_ = 0u;
  uint64_t last_messages_written_ = 0u;

  static const Configuration enabled_config_;

  static MessageQuotaCheckerTest* instance_;
};

constexpr base::TimeDelta MessageQuotaCheckerTest::kOneSamplingInterval;
const MessageQuotaCheckerTest::Configuration
    MessageQuotaCheckerTest::enabled_config_ = {true, 1, 100, 200,
                                                &RecordDumpAttempt};
MessageQuotaCheckerTest* MessageQuotaCheckerTest::instance_ = nullptr;

TEST_F(MessageQuotaCheckerTest, ReadsConfigurationFromFeatures) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Mojo quota APIs are not supported by MojoIpcz.";
  }

  base::FieldTrialParams params;
  params["SampleRate"] = "19";
  // Quota value parameter below the minimum the checker will allow.
  params["QuotaValue"] = "57";
  params["CrashThreshold"] = "225";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kMojoRecordUnreadMessageCount, params);

  // Validate that the configuration reads from the feature configuration.
  const MessageQuotaChecker::Configuration config =
      MessageQuotaChecker::GetConfigurationForTesting();

  EXPECT_TRUE(config.is_enabled);
  EXPECT_EQ(19u, config.sample_rate);
  EXPECT_EQ(100u, config.unread_message_count_quota);
  EXPECT_EQ(225u, config.crash_threshold);
  EXPECT_NE(nullptr, config.maybe_crash_function);
}

TEST_F(MessageQuotaCheckerTest, DisabledByDefault) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Mojo quota APIs are not supported by MojoIpcz.";
  }

  const MessageQuotaChecker::Configuration config =
      MessageQuotaChecker::GetConfigurationForTesting();
  EXPECT_FALSE(config.is_enabled);

  // Validate that no MessageQuoteCheckers are created in the default feature
  // configuration. Run a bunch of iterations, as this function returns an
  // instance randomly.
  for (size_t i = 0; i < 1000; ++i)
    ASSERT_EQ(nullptr, MessageQuotaChecker::MaybeCreate());
}

TEST_F(MessageQuotaCheckerTest, CreatesWhenEnabled) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Mojo quota APIs are not supported by MojoIpcz.";
  }

  // Run a bunch of iterations, as this function returns an instance randomly.
  for (size_t i = 0; i < 1000; ++i)
    EXPECT_NE(nullptr,
              MessageQuotaChecker::MaybeCreateForTesting(enabled_config_));
}

TEST_F(MessageQuotaCheckerTest, CountsRight) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Mojo quota APIs are not supported by MojoIpcz.";
  }

  scoped_refptr<MessageQuotaChecker> checker =
      MessageQuotaChecker::MaybeCreateForTesting(enabled_config_);

  ASSERT_EQ(0u, checker->GetCurrentQuotaStatusForTesting());
  ASSERT_EQ(0u, checker->GetMaxQuotaUsage());

  checker->BeforeMessagesEnqueued(10);
  ASSERT_EQ(10u, checker->GetCurrentQuotaStatusForTesting());
  ASSERT_EQ(10u, checker->GetMaxQuotaUsage());

  checker->AfterMessagesDequeued(5);
  ASSERT_EQ(5u, checker->GetCurrentQuotaStatusForTesting());
  ASSERT_EQ(10u, checker->GetMaxQuotaUsage());

  ASSERT_EQ(0u, num_dumps_);
}

TEST_F(MessageQuotaCheckerTest, CountsMessagePipeAlso) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Mojo quota APIs are not supported by MojoIpcz.";
  }

  MessagePipe pipe;
  scoped_refptr<MessageQuotaChecker> checker =
      MessageQuotaChecker::MaybeCreateForTesting(enabled_config_);

  uint64_t limit = 0;
  uint64_t usage = 0;
  MojoResult rv = MojoQueryQuota(pipe.handle0.get().value(),
                                 MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, nullptr,
                                 &limit, &usage);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  ASSERT_EQ(MOJO_QUOTA_LIMIT_NONE, limit);

  checker->SetMessagePipe(pipe.handle0.get());

  // Validate that the checker sets an unread message quota on the pipe, and
  // that it clamps to the minimum of 100.
  rv = MojoQueryQuota(pipe.handle0.get().value(),
                      MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, nullptr, &limit,
                      &usage);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  ASSERT_EQ(100u, limit);

  ASSERT_EQ(0u, checker->GetCurrentQuotaStatusForTesting());

  const char kMessage[] = "hello";
  for (size_t i = 0; i < 10; ++i) {
    checker->BeforeWrite();
    ASSERT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(pipe.handle0.get(), kMessage, sizeof(kMessage),
                              nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));
  }

  ASSERT_EQ(10u, checker->GetMaxQuotaUsage());
  ASSERT_EQ(10u, checker->GetCurrentQuotaStatusForTesting());

  checker->BeforeMessagesEnqueued(10);
  ASSERT_EQ(20u, checker->GetMaxQuotaUsage());
  ASSERT_EQ(20u, checker->GetCurrentQuotaStatusForTesting());

  ASSERT_EQ(0u, num_dumps_);
}

TEST_F(MessageQuotaCheckerTest, DumpsCoreOnOverrun) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Mojo quota APIs are not supported by MojoIpcz.";
  }

  // Make sure to start the test on an even sampling interval to get consistent
  // average computations below.
  base::TimeTicks t0 = MessageQuotaChecker::DecayingRateAverage::
      GetNextSamplingIntervalForTesting(base::TimeTicks::Now());
  task_environment_.AdvanceClock(t0 - base::TimeTicks::Now());

  MessagePipe pipe;
  scoped_refptr<MessageQuotaChecker> checker =
      MessageQuotaChecker::MaybeCreateForTesting(enabled_config_);

  // Fast forward time by a few sampling intervals.
  Advance(10 * kOneSamplingInterval);

  // Queue up 100 messages.
  checker->SetMessagePipe(pipe.handle0.get());
  const char kMessage[] = "hello";
  for (size_t i = 0; i < 100; ++i) {
    checker->BeforeWrite();
    ASSERT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(pipe.handle0.get(), kMessage, sizeof(kMessage),
                              nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));
  }

  // The crash threshold is at 200 per the config, so shouldn't have attempted
  // a core dump yet.
  ASSERT_EQ(0u, num_dumps_);

  checker->BeforeMessagesEnqueued(99);
  ASSERT_EQ(0u, num_dumps_);

  Advance(kOneSamplingInterval);
  checker->BeforeMessagesEnqueued(1);
  EXPECT_EQ(1u, num_dumps_);
  EXPECT_EQ(200u, last_dump_total_quota_used_);
  EXPECT_EQ(100u, last_dump_message_pipe_quota_used_);
  EXPECT_EQ((11 * kOneSamplingInterval).InSeconds(),
            last_seconds_since_construction_);
  EXPECT_EQ(50, last_average_write_rate_);

  checker->BeforeWrite();
  ASSERT_EQ(MOJO_RESULT_OK,
            WriteMessageRaw(pipe.handle0.get(), kMessage, sizeof(kMessage),
                            nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(2u, num_dumps_);
  EXPECT_EQ(201u, last_dump_total_quota_used_);
  EXPECT_EQ(101u, last_dump_message_pipe_quota_used_);
  EXPECT_EQ((11 * kOneSamplingInterval).InSeconds(),
            last_seconds_since_construction_);
  EXPECT_EQ(50, last_average_write_rate_);

  Advance(kOneSamplingInterval);
  checker->SetMessagePipe(mojo::MessagePipeHandle());
  checker->AfterMessagesDequeued(50);
  checker->BeforeMessagesEnqueued(300);
  EXPECT_EQ(3u, num_dumps_);
  EXPECT_EQ(350u, last_dump_total_quota_used_);
  EXPECT_FALSE(last_dump_message_pipe_quota_used_.has_value());
  EXPECT_EQ((12 * kOneSamplingInterval).InSeconds(),
            last_seconds_since_construction_);
  EXPECT_EQ(25.5, last_average_write_rate_);
  EXPECT_EQ(400u, last_messages_enqueued_);
  EXPECT_EQ(50u, last_messages_dequeued_);
  EXPECT_EQ(101u, last_messages_written_);
}

TEST_F(MessageQuotaCheckerTest, DecayingRateAverage) {
  if (mojo::core::IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Mojo quota APIs are not supported by MojoIpcz.";
  }

  // Make sure to start the test on an even sampling interval to get consistent
  // average computations below.
  base::TimeTicks t0 = MessageQuotaChecker::DecayingRateAverage::
      GetNextSamplingIntervalForTesting(base::TimeTicks::Now());
  task_environment_.AdvanceClock(t0 - base::TimeTicks::Now());

  // Walk time forward a bit from the epoch.
  t0 += 101 * kOneSamplingInterval;

  MessageQuotaChecker::DecayingRateAverage avg;
  EXPECT_EQ(0.0, avg.GetDecayedRateAverage(t0));

  // Tally two events in the same sampling interval.
  avg.AccrueEvent(t0);
  avg.AccrueEvent(t0);

  // The decayed average rate is half of the rate this sampling interval.
  t0 += kOneSamplingInterval;
  EXPECT_EQ(1.0, avg.GetDecayedRateAverage(t0));

  // Tally another two events in a subsequent sampling interval.
  avg.AccrueEvent(t0);
  avg.AccrueEvent(t0);

  t0 += kOneSamplingInterval;
  EXPECT_EQ(1.5, avg.GetDecayedRateAverage(t0));

  // Tally another two events in a subsequent sampling interval.
  avg.AccrueEvent(t0);
  avg.AccrueEvent(t0);
  EXPECT_EQ(1.75, avg.GetDecayedRateAverage(t0 + kOneSamplingInterval));

  // Make sure the average is decayed with time, including within a sampling
  // interval.
  EXPECT_EQ(0.875, avg.GetDecayedRateAverage(t0 + 2 * kOneSamplingInterval));
  EXPECT_NEAR(0.619, avg.GetDecayedRateAverage(t0 + 2.5 * kOneSamplingInterval),
              0.001);
  EXPECT_EQ(0.4375, avg.GetDecayedRateAverage(t0 + 3 * kOneSamplingInterval));

  t0 += 10 * kOneSamplingInterval;
  avg.AccrueEvent(t0);
  avg.AccrueEvent(t0);
  // The previous average should have decayed by 1/1024.
  EXPECT_EQ(1.0 + 1.75 / 1024.0,
            avg.GetDecayedRateAverage(t0 + kOneSamplingInterval));

  // Explicitly test simple interpolation in a sampling interval by setting
  // up an average that's conveniently converged to 4.0.
  MessageQuotaChecker::DecayingRateAverage avg2;
  for (size_t i = 0; i < 16; ++i)
    avg2.AccrueEvent(t0);
  t0 += 2 * kOneSamplingInterval;
  EXPECT_EQ(4.0, avg2.GetDecayedRateAverage(t0));

  // Now test 0/8, 1/8, 1/4, 1/2 and 3/4-way interpolation.
  avg2.AccrueEvent(t0);
  // The special case of adding an event and requesting the average at the
  // start of sampling interval explicitly excludes that event.
  EXPECT_EQ(4.0, avg2.GetDecayedRateAverage(t0));
  EXPECT_NEAR(4.332,
              avg2.GetDecayedRateAverage(t0 + 1.0 / 8.0 * kOneSamplingInterval),
              0.001);
  EXPECT_EQ(4.0,
            avg2.GetDecayedRateAverage(t0 + 1.0 / 4.0 * kOneSamplingInterval));
  avg2.AccrueEvent(t0);
  EXPECT_EQ(4.0,
            avg2.GetDecayedRateAverage(t0 + 2.0 / 4.0 * kOneSamplingInterval));
  avg2.AccrueEvent(t0);
  EXPECT_EQ(4.0,
            avg2.GetDecayedRateAverage(t0 + 3.0 / 4.0 * kOneSamplingInterval));
}

}  // namespace
}  // namespace test
}  // namespace mojo
