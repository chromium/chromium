// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_POLICY_H_
#define IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_POLICY_H_

// Values for the ContextMenuPhotoSharingSettings policy.
// VALUES MUST COINCIDE WITH THE ContextMenuPhotoSharingSettings POLICY
// DEFINITION.
enum class SaveToPhotosPolicySettings {
  // Enabled. The context menu will have a menu item to share images to Google
  // Photos.
  kEnabled,
  // Disabled. The context menu will not have a menu item to share images to
  // Google Photos.
  kDisabled,
};

#endif  // IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_POLICY_H_
