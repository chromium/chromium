// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_QUOTA_CHECKER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_QUOTA_CHECKER_H_

#include <stdint.h>
#include <memory>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace internal {

// This class keeps track of how many messages are in-flight for a message pipe,
// including messages that are posted or locally queued.
//
// Message pipe owners may have reason to implement their own mechanism for
// queuing outgoing messages before writing them to a pipe. This class helps
// with unread message quota monitoring in such cases, since Mojo's own
// quota monitoring on the pipe cannot account for such external queues.
// Callers are responsible for invoking  |BeforeMessagesEnqueued()| and
// |AfterMessagesDequeued()| when making respective changes to their local
// outgoing queue. Additionally, |BeforeWrite()| should be called immediately
// before writing each message to the corresponding message pipe.
//
// Also note that messages posted to a different sequence with base::ThreadPool
// and the like, need to be treated as locally queued. Task queues can grow
// arbitrarily long, and it's ideal to perform unread quota checks before
// posting.
//
// Either |BeforeMessagesEnqueued()| or |BeforeWrite()| may cause the quota
// to be exceeded, thus invoking the |maybe_crash_function| set in this
// object's Configuration.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) MessageQuotaChecker
    : public base::RefCountedThreadSafe<MessageQuotaChecker> {
 public:
  // A helper class to maintain a decaying average for the rate of events per
  // sampling interval over time.
  class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) DecayingRateAverage {
   public:
    DecayingRateAverage();

    // Accrues one event at time |when|. Note that |when| must increase
    // monotonically from one event to the next.
    void AccrueEvent(base::TimeTicks when);
    // Retrieves the current rate average, decayed to |when|.
    double GetDecayedRateAverage(base::TimeTicks when) const;

    // The length of a sampling interval in seconds.
    static constexpr base::TimeDelta kSamplingInterval =
        base::TimeDelta::FromSeconds(5);

    // Returns the start of the sampling interval after the interval that
    // |when| falls into.
    static base::TimeTicks GetNextSamplingIntervalForTesting(
        base::TimeTicks when);

   private:
    // A new sample is weighed at this rate into the average, whereas the old
    // average is weighed at kDecayFactor^age; Note that
    // (kSampleWeight + kDecayFactor) == 1.0.
    static constexpr double kSampleWeight = 0.5;
    static constexpr double kDecayFactor = (1 - kSampleWeight);

    // The event count for the current or most recent sampling interval and
    // the ordinal sampling interval they correspond to.
    size_t events_ = 0;
    int64_t events_sampling_interval_;

    // The so-far accrued average to |events_sampling_interval_|.
    double decayed_average_ = 0.0;
  };

  // Returns a new instance if this invocation has been sampled for quota
  // checking.
  static scoped_refptr<MessageQuotaChecker> MaybeCreate();

  // Call before writing a message to |message_pipe_|.
  void BeforeWrite();

  // Call before queueing |num| messages.
  void BeforeMessagesEnqueued(size_t num);
  // Call after de-queueing |num| messages.
  void AfterMessagesDequeued(size_t num);

  // Returns the high watermark of quota usage observed by this instance.
  size_t GetMaxQuotaUsage();

  // Set or unset the message pipe associated with this quota checker.
  void SetMessagePipe(MessagePipeHandle message_pipe);

  // Test support.
  size_t GetCurrentQuotaStatusForTesting();
  struct Configuration;
  static Configuration GetConfigurationForTesting();
  static scoped_refptr<MessageQuotaChecker> MaybeCreateForTesting(
      const Configuration& config);

 private:
  friend class base::RefCountedThreadSafe<MessageQuotaChecker>;
  explicit MessageQuotaChecker(const Configuration* config);
  ~MessageQuotaChecker();
  static Configuration GetConfiguration();
  static scoped_refptr<MessageQuotaChecker> MaybeCreateImpl(
      const Configuration& config);

  // Returns the amount of unread message quota currently used if there is
  // an associated message pipe.
  base::Optional<size_t> GetCurrentMessagePipeQuota();
  void QuotaCheckImpl(size_t num_enqueued);

  const Configuration* config_;

  // The time ticks when this instance was created.
  const base::TimeTicks creation_time_;


  // Cumulative counts for the number of messages enqueued with
  // |BeforeMessagesEnqueued()| and dequeued with |BeforeMessagesDequeued()|.
  std::atomic<uint64_t> messages_enqueued_{0};
  std::atomic<uint64_t> messages_dequeued_{0};
  std::atomic<uint64_t> messages_written_{0};

  // Guards all state below here.
  base::Lock lock_;

  // A decaying average of the rate of call to BeforeWrite per second.
  DecayingRateAverage write_rate_average_ GUARDED_BY(lock_);

  // The locally consumed quota, e.g. the difference between the counts passed
  // to |BeforeMessagesEnqueued()| and |BeforeMessagesDequeued()|.
  size_t consumed_quota_ GUARDED_BY(lock_) = 0u;
  // The high watermark consumed quota observed.
  size_t max_consumed_quota_ GUARDED_BY(lock_) = 0u;
  // The message pipe this instance observes, if any.
  MessagePipeHandle message_pipe_ GUARDED_BY(lock_);
};

struct MessageQuotaChecker::Configuration {
  bool is_enabled = false;
  size_t sample_rate = 0u;
  size_t unread_message_count_quota = 0u;
  size_t crash_threshold = 0u;
  void (*maybe_crash_function)(size_t quota_used,
                               base::Optional<size_t> message_pipe_quota_used,
                               int64_t seconds_since_construction,
                               double average_write_rate,
                               uint64_t messages_enqueued,
                               uint64_t messages_dequeued,
                               uint64_t messages_written);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_QUOTA_CHECKER_H_
