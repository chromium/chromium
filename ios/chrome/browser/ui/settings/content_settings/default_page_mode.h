// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_H_

#import <Foundation/Foundation.h>

// The mode in which pages should be loaded.
typedef NS_ENUM(NSUInteger, DefaultPageMode) {
  DefaultPageModeMobile,
  DefaultPageModeDesktop,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_H_
