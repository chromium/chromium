// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_QUOTA_CHECKER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_QUOTA_CHECKER_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
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
// Also note that messages posted to a different sequence with
// |base::PostTask()| and the like, need to be treated as locally queued. Task
// queues can grow arbitrarily long, and it's ideal to perform unread quota
// checks before posting.
//
// Either |BeforeMessagesEnqueued()| or |BeforeWrite()| may cause the quota
// to be exceeded, thus invoking the |maybe_crash_function| set in this
// object's Configuration.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) MessageQuotaChecker
    : public base::RefCountedThreadSafe<MessageQuotaChecker> {
 public:
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

  // Locks all local state.
  base::Lock lock_;
  // The locally consumed quota, e.g. the difference between the counts passed
  // to |BeforeMessagesEnqueued()| and |BeforeMessagesDequeued()|.
  size_t consumed_quota_ = 0u;
  // The high watermark consumed quota observed.
  size_t max_consumed_quota_ = 0u;
  // The quota level that triggers a crash dump, or zero to disable crashing.
  size_t crash_threshold_ = 0u;
  // The message pipe this instance observes, if any.
  MessagePipeHandle message_pipe_;
};

struct MessageQuotaChecker::Configuration {
  bool is_enabled = false;
  size_t sample_rate = 0u;
  size_t unread_message_count_quota = 0u;
  size_t crash_threshold = 0u;
  void (*maybe_crash_function)(size_t quota_used,
                               base::Optional<size_t> message_pipe_quota_used);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_QUOTA_CHECKER_H_
