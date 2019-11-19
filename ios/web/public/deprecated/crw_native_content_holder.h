// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_NATIVE_CONTENT_HOLDER_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_NATIVE_CONTENT_HOLDER_H_

#import <Foundation/Foundation.h>

@protocol CRWNativeContent;
@protocol CRWNativeContentProvider;

// Protocol describing an object having ownership of a native controller.
@protocol CRWNativeContentHolder

// The provider for the native content.
@property(nonatomic, weak) id<CRWNativeContentProvider> nativeProvider;

// The native controller owned by this object.
- (id<CRWNativeContent>)nativeController;

@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_NATIVE_CONTENT_HOLDER_H_
