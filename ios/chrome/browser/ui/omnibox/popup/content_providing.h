// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CONTENT_PROVIDING_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CONTENT_PROVIDING_H_

#import <Foundation/Foundation.h>

@protocol ContentProviding

@property(nonatomic, readonly) BOOL hasContent;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_CONTENT_PROVIDING_H_
