// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_FLAGS_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_FLAGS_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Used to configure feature flags during runtime. Flags are persisted across
// app restarts.
CWV_EXPORT
@interface CWVFlags : NSObject

// Whether or not sync and wallet features are communicating with the sandbox
// servers. This can be used for testing purposes. The app must be restarted for
// changes to take effect. Default value is NO.
@property(nonatomic, assign) BOOL usesSyncAndWalletSandbox;

+ (instancetype)sharedInstance;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_FLAGS_H_
