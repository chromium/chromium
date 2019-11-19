// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"

#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/c/system/quota.h"
#include "mojo/public/cpp/bindings/features.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  static void RecordDumpAttempt(
      size_t total_quota_used,
      base::Optional<size_t> message_pipe_quota_used) {
    ++instance_->num_dumps_;
    instance_->last_dump_total_quota_used_ = total_quota_used;
    instance_->last_dump_message_pipe_quota_used_ = message_pipe_quota_used;
  }

  size_t num_dumps_ = false;
  size_t last_dump_total_quota_used_ = 0u;
  base::Optional<size_t> last_dump_message_pipe_quota_used_;

  static const Configuration enabled_config_;

  static MessageQuotaCheckerTest* instance_;
};

const MessageQuotaCheckerTest::Configuration
    MessageQuotaCheckerTest::enabled_config_ = {true, 1, 100, 200,
                                                &RecordDumpAttempt};
MessageQuotaCheckerTest* MessageQuotaCheckerTest::instance_ = nullptr;

TEST_F(MessageQuotaCheckerTest, ReadsConfigurationFromFeatures) {
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
  // Run a bunch of iterations, as this function returns an instance randomly.
  for (size_t i = 0; i < 1000; ++i)
    EXPECT_NE(nullptr,
              MessageQuotaChecker::MaybeCreateForTesting(enabled_config_));
}

TEST_F(MessageQuotaCheckerTest, CountsRight) {
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
  MessagePipe pipe;
  scoped_refptr<MessageQuotaChecker> checker =
      MessageQuotaChecker::MaybeCreateForTesting(enabled_config_);

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

  checker->BeforeMessagesEnqueued(50);
  ASSERT_EQ(0u, num_dumps_);

  checker->BeforeMessagesEnqueued(50);
  ASSERT_EQ(1u, num_dumps_);
  ASSERT_EQ(200u, last_dump_total_quota_used_);
  ASSERT_EQ(100u, last_dump_message_pipe_quota_used_);

  checker->BeforeWrite();
  ASSERT_EQ(MOJO_RESULT_OK,
            WriteMessageRaw(pipe.handle0.get(), kMessage, sizeof(kMessage),
                            nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));

  ASSERT_EQ(2u, num_dumps_);
  ASSERT_EQ(201u, last_dump_total_quota_used_);
  ASSERT_EQ(101u, last_dump_message_pipe_quota_used_);

  checker->SetMessagePipe(mojo::MessagePipeHandle());
  checker->BeforeMessagesEnqueued(250);
  ASSERT_EQ(3u, num_dumps_);
  ASSERT_EQ(350u, last_dump_total_quota_used_);
  ASSERT_FALSE(last_dump_message_pipe_quota_used_.has_value());
}

}  // namespace
}  // namespace test
}  // namespace mojo
