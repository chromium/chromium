// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_WEAK_CHECK_UTILS_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_WEAK_CHECK_UTILS_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVPassword;

CWV_EXPORT
@interface CWVWeakCheckUtils : NSObject

// Checks if the supplied password is weak. Do not call from the main thread,
// depending on password length, this method could block.
+ (BOOL)isPasswordWeak:(NSString*)password;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_WEAK_CHECK_UTILS_H_
