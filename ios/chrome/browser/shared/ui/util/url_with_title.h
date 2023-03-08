// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_URL_WITH_TITLE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_URL_WITH_TITLE_H_

#include <url/gurl.h>

#import <Foundation/Foundation.h>

// Data object used to represent a URL and an associated page title for sharing.
@interface URLWithTitle : NSObject

// Designated initializer.
- (instancetype)initWithURL:(const GURL&)URL title:(NSString*)title;

// The URL to be shared.
@property(nonatomic, readonly) const GURL& URL;

// Title of the page associated with `URL` to share.
@property(nonatomic, readonly, copy) NSString* title;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_URL_WITH_TITLE_H_
