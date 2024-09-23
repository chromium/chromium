// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/features.h"

namespace mojo {
namespace features {

// Enables a task to be scheduled for each individual message dispatched to a
// Mojo binding endpoint (or reply to an InterfacePtr).
//
// When disabled, dispatch happens eagerly in batch, so when a binding is
// scheduled to dispatch messages, it fully flushes and dispatches all queued
// messages within the extent of a single scheduler task.
//
// Enabling this feature allows for more fine-grained performance control
// through the scheduler, but may initially cause some important edge cases to
// regress in performance due to high-priority messages seeing increased
// latency. Ideally we'd address these cases by giving the affected bindings
// higher-priority TaskRunners.
BASE_FEATURE(kTaskPerMessage,
             "MojoTaskPerMessage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables measurement of MessageChannel unread message counts. When enabled, a
// small random selection of Connectors enable the unread message count quota
// on their associated message pipe, and record the highest unread message count
// seen during the Connector's lifetime.
BASE_FEATURE(kMojoRecordUnreadMessageCount,
             "MojoRecordUnreadMessageCount",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables predictive allocation for Mojo serialization.
BASE_FEATURE(kMojoPredictiveAllocation,
             "MojoPredictiveAllocation",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
}  // namespace mojo
