// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_EXTENSION_OPEN_URL_H_
#define IOS_CHROME_COMMON_EXTENSION_OPEN_URL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

using BlockWithBoolean = void (^)(BOOL success);

// Open `url` function for extensions. If `pre_open_block` is not nil, it will
// be called just before the actual call to openURL, and hence before the
// application switch is done.
BOOL ExtensionOpenURL(NSURL* url,
                      UIResponder* responder,
                      BlockWithBoolean pre_open_block);

#endif  // IOS_CHROME_COMMON_EXTENSION_OPEN_URL_H_
