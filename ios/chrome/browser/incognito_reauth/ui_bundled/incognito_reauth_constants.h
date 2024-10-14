// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSTANTS_H_

#import "base/time/time.h"

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

// TODO(crbug.com/370804664): Make this constant configurable via Finch.
// Time that Chrome should be backgrounded so that the Soft Lock screen is
// displayed.
inline base::TimeDelta const kSoftLockBackgroundThreshold = base::Seconds(10);

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSTANTS_H_
