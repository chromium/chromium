// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_REUSE_CHECK_SERVICE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_REUSE_CHECK_SERVICE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVPassword;

CWV_EXPORT
@interface CWVReuseCheckService : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Checks for reused passwords in the supplied array of `CWVPassword` objects.
// `completionHandler` will be called upon completion.
- (void)checkReusedPasswords:(NSArray<CWVPassword*>*)passwords
           completionHandler:
               (void (^)(NSSet<NSString*>* reusedPasswords))completionHandler;
@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_REUSE_CHECK_SERVICE_H_
