// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCOPED_IPHONE_PORTRAIT_ONLY_SCOPED_IPHONE_PORTRAIT_ONLY_H_
#define IOS_CHROME_BROWSER_UI_SCOPED_IPHONE_PORTRAIT_ONLY_SCOPED_IPHONE_PORTRAIT_ONLY_H_

#import <Foundation/Foundation.h>

@protocol IphonePortraitOnlyManager;

// A helper object that increments AppState's iPhone portrait only counter for
// its entire lifetime. This object can only be used for iPhone.
class ScopedIphonePortraitOnly {
 public:
  explicit ScopedIphonePortraitOnly(id<IphonePortraitOnlyManager> manager);
  ~ScopedIphonePortraitOnly();

 private:
  // The target blocking the portrait only.
  __weak id<IphonePortraitOnlyManager> manager_;

  ScopedIphonePortraitOnly(const ScopedIphonePortraitOnly&) = delete;
  ScopedIphonePortraitOnly& operator=(const ScopedIphonePortraitOnly&) = delete;
};

#endif  // IOS_CHROME_BROWSER_UI_SCOPED_IPHONE_PORTRAIT_ONLY_SCOPED_IPHONE_PORTRAIT_ONLY_H_
