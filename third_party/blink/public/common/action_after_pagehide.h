// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_ACTION_AFTER_PAGEHIDE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_ACTION_AFTER_PAGEHIDE_H_

namespace blink {

// This is the enumeration of actions that might happen in a page after pagehide
// is dispatched that might affect the user after we've navigated away from the
// old page, such as modifications to storage, navigations, or postMessage.
// This enum is used for histograms and should not be renumbered.
enum class ActionAfterPagehide {
  kNavigation = 0,
  kSentPostMessage = 1,
  kReceivedPostMessage = 2,
  kLocalStorageModification = 3,
  kSessionStorageModification = 4,
  kMaxValue = kSessionStorageModification
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_ACTION_AFTER_PAGEHIDE_H_
