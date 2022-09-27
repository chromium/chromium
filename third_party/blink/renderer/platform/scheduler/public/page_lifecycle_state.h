// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PAGE_LIFECYCLE_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PAGE_LIFECYCLE_STATE_H_

namespace blink {

// Page lifecycle states, as defined in
// https://github.com/WICG/page-lifecycle/blob/master/README.md. We maintain two
// HIDDEN states, distinguishing between hidden pages that should not be frozen
// because they are still providing a service (e.g. audio), and hidden pages
// that are not providing a service and are eligible to be frozen.
enum class PageLifecycleState {
  // The page state is unknown.
  kUnknown,
  // The page is visible and active.
  kActive,
  // The page is not visible, but is still active, performing a useful service
  // for the user, such as playing audio.
  kHiddenForegrounded,
  // The page is not visible and not active.
  kHiddenBackgrounded,
  // The page is frozen.
  kFrozen,
};

constexpr PageLifecycleState kDefaultPageLifecycleState =
    PageLifecycleState::kUnknown;

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PAGE_LIFECYCLE_STATE_H_
