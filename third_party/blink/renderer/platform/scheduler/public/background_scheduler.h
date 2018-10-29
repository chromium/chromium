// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_BACKGROUND_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_BACKGROUND_SCHEDULER_H_

#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace background_scheduler {

// These are a thin wrapper around base::TaskScheduler to accomodate
// Blink's CrossThreadClosure, which only allows background tasks
// (i.e. tasks which are run off the main thread).
//
// Non-background tasks should be posted using another scheduler, e.g.
// FrameShceduler.
PLATFORM_EXPORT void PostOnBackgroundThread(const base::Location&,
                                            CrossThreadClosure);

PLATFORM_EXPORT void PostOnBackgroundThreadWithTraits(const base::Location&,
                                                      const base::TaskTraits&,
                                                      CrossThreadClosure);

// TODO(altimin): Expose CreateBackgroundTaskRunnerWithTraits when the
// need arises.

}  // namespace background_scheduler

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_BACKGROUND_SCHEDULER_H_
