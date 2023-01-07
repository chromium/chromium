// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_LEAK_CHECK_SERVICE_OBSERVER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_LEAK_CHECK_SERVICE_OBSERVER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVLeakCheckCredential;

// Protocol to receive change notifications from CWVLeakCheckService.
@protocol CWVLeakCheckServiceObserver <NSObject>

// Called whenever CWVLeakCheckService changes state.
- (void)leakCheckServiceDidChangeState:(CWVLeakCheckService*)leakCheckService;

// Called whenever an individual leak check has been completed.
- (void)leakCheckService:(CWVLeakCheckService*)leakCheckService
      didCheckCredential:(CWVLeakCheckCredential*)credential
                isLeaked:(BOOL)isLeaked;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_LEAK_CHECK_SERVICE_OBSERVER_H_
