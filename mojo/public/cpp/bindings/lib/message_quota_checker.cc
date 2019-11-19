// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"

#include <algorithm>

#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "mojo/public/c/system/quota.h"
#include "mojo/public/cpp/bindings/features.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"

namespace mojo {
namespace internal {
namespace {

const base::FeatureParam<int> kMojoRecordUnreadMessageCountSampleRate = {
    &features::kMojoRecordUnreadMessageCount, "SampleRate",
    100  // Sample 1% of Connectors by default. */
};

const base::FeatureParam<int> kMojoRecordUnreadMessageCountQuotaValue = {
    &features::kMojoRecordUnreadMessageCount, "QuotaValue",
    100  // Use a 100 message quote by default.
};

const base::FeatureParam<int> kMojoRecordUnreadMessageCountCrashThreshold = {
    &features::kMojoRecordUnreadMessageCount, "CrashThreshold",
    0  // Set to zero to disable crash dumps by default.
};

NOINLINE void MaybeDumpWithoutCrashing(
    size_t total_quota_used,
    base::Optional<size_t> message_pipe_quota_used) {
  static bool have_crashed = false;
  if (have_crashed)
    return;

  // Only crash once per process/per run. Note that this is slightly racy
  // against concurrent quota overruns on multiple threads, but that's fine.
  have_crashed = true;

  // This is happening because the user of the interface implicated on the crash
  // stack has queued up an unreasonable number of messages, namely
  // |quota_used|.
  base::debug::DumpWithoutCrashing();

  size_t local_quota_used = total_quota_used;
  bool had_message_pipe = false;
  if (message_pipe_quota_used.has_value()) {
    had_message_pipe = true;
    local_quota_used -= message_pipe_quota_used.value();
  }

  base::debug::Alias(&total_quota_used);
  base::debug::Alias(&local_quota_used);
  base::debug::Alias(&had_message_pipe);
}

}  // namespace

// static
scoped_refptr<MessageQuotaChecker> MessageQuotaChecker::MaybeCreate() {
  static const Configuration config = GetConfiguration();
  return MaybeCreateImpl(config);
}

void MessageQuotaChecker::BeforeWrite() {
  QuotaCheckImpl(0u);
}

void MessageQuotaChecker::BeforeMessagesEnqueued(size_t num) {
  DCHECK_NE(num, 0u);
  QuotaCheckImpl(num);
}

void MessageQuotaChecker::AfterMessagesDequeued(size_t num) {
  base::AutoLock hold(lock_);
  DCHECK_LE(num, consumed_quota_);
  DCHECK_NE(num, 0u);

  consumed_quota_ -= num;
}

size_t MessageQuotaChecker::GetMaxQuotaUsage() {
  base::AutoLock hold(lock_);
  return max_consumed_quota_;
}

void MessageQuotaChecker::SetMessagePipe(MessagePipeHandle message_pipe) {
  base::AutoLock hold(lock_);
  message_pipe_ = message_pipe;
  if (!message_pipe_)
    return;

  MojoResult rv =
      MojoSetQuota(message_pipe.value(), MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT,
                   config_->unread_message_count_quota, nullptr);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
}

size_t MessageQuotaChecker::GetCurrentQuotaStatusForTesting() {
  base::AutoLock hold(lock_);
  size_t quota_used = consumed_quota_;
  base::Optional<size_t> message_pipe_quota_used = GetCurrentMessagePipeQuota();
  if (message_pipe_quota_used.has_value())
    quota_used += message_pipe_quota_used.value();

  return quota_used;
}

// static
MessageQuotaChecker::Configuration
MessageQuotaChecker::GetConfigurationForTesting() {
  return GetConfiguration();
}

// static
scoped_refptr<MessageQuotaChecker> MessageQuotaChecker::MaybeCreateForTesting(
    const Configuration& config) {
  return MaybeCreateImpl(config);
}

MessageQuotaChecker::MessageQuotaChecker(const Configuration* config)
    : config_(config) {}
MessageQuotaChecker::~MessageQuotaChecker() = default;

// static
MessageQuotaChecker::Configuration MessageQuotaChecker::GetConfiguration() {
  Configuration ret;

  ret.is_enabled =
      base::FeatureList::IsEnabled(features::kMojoRecordUnreadMessageCount);
  ret.sample_rate = kMojoRecordUnreadMessageCountSampleRate.Get();

  // Lower-bound the quota value to 100, which implies roughly 2% message
  // overhead for sampled pipes.
  constexpr int kMinQuotaValue = 100;
  ret.unread_message_count_quota =
      std::max(kMinQuotaValue, kMojoRecordUnreadMessageCountQuotaValue.Get());
  ret.crash_threshold = kMojoRecordUnreadMessageCountCrashThreshold.Get();
  ret.maybe_crash_function = &MaybeDumpWithoutCrashing;
  return ret;
}

// static
scoped_refptr<MessageQuotaChecker> MessageQuotaChecker::MaybeCreateImpl(
    const Configuration& config) {
  if (!config.is_enabled)
    return nullptr;

  if (base::RandInt(0, config.sample_rate - 1) != 0)
    return nullptr;

  return new MessageQuotaChecker(&config);
}

base::Optional<size_t> MessageQuotaChecker::GetCurrentMessagePipeQuota() {
  lock_.AssertAcquired();

  if (!message_pipe_)
    return base::nullopt;

  uint64_t limit = 0;
  uint64_t usage = 0;
  MojoResult rv = MojoQueryQuota(message_pipe_.value(),
                                 MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, nullptr,
                                 &limit, &usage);
  return rv == MOJO_RESULT_OK ? usage : 0u;
}

void MessageQuotaChecker::QuotaCheckImpl(size_t num_enqueued) {
  bool new_max = false;

  // By the time a crash is reported, another thread might have consumed some of
  // the locally queued messages, and/or the message pipe might have been unset.
  // To make the crash reports as useful as possible, grab the state of the
  // local and the message pipe queues into individual variables, then pass them
  // into the crashing function.
  size_t total_quota_used = 0u;
  base::Optional<size_t> message_pipe_quota_used;
  {
    base::AutoLock hold(lock_);

    message_pipe_quota_used = GetCurrentMessagePipeQuota();

    if (num_enqueued) {
      consumed_quota_ += num_enqueued;
    } else {
      // BeforeWrite passes num_enqueued zero, as the message won't be locally
      // enqueued. The assumption is that there's already a message pipe in
      // play, and that the caller is keeping it alive somehow.
      DCHECK(message_pipe_);
      DCHECK(message_pipe_quota_used.has_value());

      // Account for the message about to be written to the message pipe in the
      // the full tally.
      ++message_pipe_quota_used.value();
    }

    total_quota_used += consumed_quota_;
    if (message_pipe_quota_used.has_value())
      total_quota_used += message_pipe_quota_used.value();

    if (total_quota_used > max_consumed_quota_) {
      max_consumed_quota_ = total_quota_used;
      new_max = true;
    }
  }

  if (new_max && config_->crash_threshold != 0 &&
      total_quota_used >= config_->crash_threshold) {
    config_->maybe_crash_function(total_quota_used, message_pipe_quota_used);
  }
}

}  // namespace internal
}  // namespace mojo
