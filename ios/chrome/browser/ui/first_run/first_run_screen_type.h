// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_TYPE_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_TYPE_H_

// The types of the first run screens.
typedef NS_ENUM(NSInteger, FirstRunScreenType) {
  kWelcomeAndConsent,
  kSignIn,
  kSync,
  kDefaultBrowserPromo,
  // It isn't a screen, but a signal that no more screen should be
  // presented.
  kFirstRunCompleted,
};

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_TYPE_H_
