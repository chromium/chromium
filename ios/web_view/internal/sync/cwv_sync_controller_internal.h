// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_

#import "ios/web_view/public/cwv_sync_controller.h"

NS_ASSUME_NONNULL_BEGIN

namespace syncer {
class SyncService;
}  // namespace syncer

namespace signin {
class IdentityManager;
}  // namespace signin

class PrefService;

@interface CWVSyncController ()

// All dependencies must out live this class.
- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
                        prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_CWV_SYNC_CONTROLLER_INTERNAL_H_
