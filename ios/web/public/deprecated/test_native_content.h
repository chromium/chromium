// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_TEST_NATIVE_CONTENT_H_
#define IOS_WEB_PUBLIC_DEPRECATED_TEST_NATIVE_CONTENT_H_

#import "ios/web/public/deprecated/crw_native_content.h"

// A test class that implement CRWNativeContent.
@interface TestNativeContent : NSObject <CRWNativeContent>
// Inits the CRWNativeContent.
// |URL| will be returned by the |url| method of the object.
// If |virtualURL| is valid, it will be returned by the |virtualURL| method.
// If not, the object will pretend it does not implement |virtualURL|.
- (instancetype)initWithURL:(const GURL&)URL
                 virtualURL:(const GURL&)virtualURL NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_TEST_NATIVE_CONTENT_H_
