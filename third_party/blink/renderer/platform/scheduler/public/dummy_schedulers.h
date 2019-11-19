// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_DUMMY_SCHEDULERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_DUMMY_SCHEDULERS_H_

#include <memory>

#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
class FrameScheduler;
class PageScheduler;
class ThreadScheduler;

namespace scheduler {
class WebThreadScheduler;

// These methods create a simple implementations of the core scheduler classes.
// They are useful in the situation when you want to return a non-null scheduler
// (to ensure that your callers don't have to check for this) but don't have one
// available.
//
// The actual implementation is no-op, with two exceptions:
// - It returns a valid task runner (default one).
// - Creating new schedulers (e.g. DummyPageScheduler's CreateFrameScheduler
//   method returns a DummyFrameScheduler.
//
// Overall, their usage is discouraged except in the following two cases:
// - Detached frames (should be fixed with frame:document lifetime refactoring).
// - Tests

PLATFORM_EXPORT std::unique_ptr<FrameScheduler> CreateDummyFrameScheduler();
PLATFORM_EXPORT std::unique_ptr<PageScheduler> CreateDummyPageScheduler();
PLATFORM_EXPORT std::unique_ptr<ThreadScheduler> CreateDummyThreadScheduler();
PLATFORM_EXPORT std::unique_ptr<WebThreadScheduler>
CreateDummyWebThreadScheduler();

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_DUMMY_SCHEDULERS_H_
