// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DATA_SOURCE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVSyncController;

// Data source of CWVSyncController.
@protocol CWVSyncControllerDataSource<NSObject>

// Called when access tokens are requested.
// |scopes| OAuth scopes requested.
// |completionHandler| Use to pass back token information.
// If successful, only |accessToken| and |expirationDate| should be non-nil.
// If unsuccessful, only |error| should be non-nil.
- (void)syncController:(CWVSyncController*)syncController
    getAccessTokenForScopes:(NSArray<NSString*>*)scopes
          completionHandler:
              (void (^)(NSString* _Nullable accessToken,
                        NSDate* _Nullable expirationDate,
                        NSError* _Nullable error))completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DATA_SOURCE_H_
