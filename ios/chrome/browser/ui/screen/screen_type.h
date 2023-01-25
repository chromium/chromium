// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCREEN_SCREEN_TYPE_H_
#define IOS_CHROME_BROWSER_UI_SCREEN_SCREEN_TYPE_H_

// The types of the start up screens.
typedef NS_ENUM(NSInteger, ScreenType) {
  kSignIn,
  kTangibleSync,
  kDefaultBrowserPromo,

  // Deprecated.
  //
  // TODO(crbug.com/1407658) Remove the following entries and their
  // corresponding screens as they are not obsolete.
  kWelcomeAndConsent_DEPRECATED,

  // It isn't a screen, but a signal that no more screen should be
  // presented.
  kStepsCompleted,
};

#endif  // IOS_CHROME_BROWSER_UI_SCREEN_SCREEN_TYPE_H_
