// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_INCOGNITO_LOCK_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_INCOGNITO_LOCK_STATE_H_

// Enum that captures the type of overlay, if any, that is displayed over
// incognito content.
enum class IncognitoLockState {
  // No overlay should be displayed over the incognito content.
  kNone,
  // An overlay is displayed over incognito content that requires
  // reauthentication in order to dismiss.
  kReauth,
  // An overlay is displayed over incognito content that requires a tap in order
  // to dismiss.
  kSoftLock,
};

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_INCOGNITO_LOCK_STATE_H_
