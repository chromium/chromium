// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_TEST_NATIVE_CONTENT_PROVIDER_H_
#define IOS_WEB_PUBLIC_DEPRECATED_TEST_NATIVE_CONTENT_PROVIDER_H_

#import "ios/web/public/deprecated/crw_native_content_provider.h"

@protocol CRWNativeContent;

// A test class that will return CRWNativeContent for specified URLs.
@interface TestNativeContentProvider : NSObject <CRWNativeContentProvider>

// Add a |CRWNativeContent| for |URL|.
- (void)setController:(id<CRWNativeContent>)controller forURL:(const GURL&)URL;

@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_TEST_NATIVE_CONTENT_PROVIDER_H_
