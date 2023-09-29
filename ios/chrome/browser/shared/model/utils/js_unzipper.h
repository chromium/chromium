// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_JS_UNZIPPER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_JS_UNZIPPER_H_

#import <Foundation/Foundation.h>

extern const NSErrorDomain kJSUnzipperErrorDomain;

/// Helper to unzip data using JavaScript in a separate process.
@interface JSUnzipper : NSObject

/// Unzip `data` asynchronously. `callback` will be called with the results.
/// Calling this again will cancel ongoing requests.
- (void)unzipData:(NSData*)data
    completionCallback:(void (^)(NSArray<NSData*>*, NSError*))callback;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_JS_UNZIPPER_H_
