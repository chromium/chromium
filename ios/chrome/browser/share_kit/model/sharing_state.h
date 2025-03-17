// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARING_STATE_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARING_STATE_H_

namespace tab_groups {

// Represents the sharing state of a saved tab group.
enum class SharingState {
  // The group is not shared.
  kNotShared,
  // The group is shared, but the current user is not the owner.
  kShared,
  // The group is shared, and the current user is the owner.
  kSharedAndOwned,
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARING_STATE_H_
