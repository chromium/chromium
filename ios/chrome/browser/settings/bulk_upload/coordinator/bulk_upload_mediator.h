// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_BULK_UPLOAD_COORDINATOR_BULK_UPLOAD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_BULK_UPLOAD_COORDINATOR_BULK_UPLOAD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/bulk_upload/ui/bulk_upload_mutator.h"

@protocol BulkUploadConsumer;
@protocol BulkUploadMediatorDelegate;
@protocol ReauthenticationProtocol;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// Mediator for BulkUpload
@interface BulkUploadMediator : NSObject <BulkUploadMutator>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
             reauthenticationModule:
                 (id<ReauthenticationProtocol>)reauthenticationModule
    NS_DESIGNATED_INITIALIZER;

// Setting the consumer immediately sends it information on local items.
@property(nonatomic, weak) id<BulkUploadConsumer> consumer;

@property(nonatomic, weak) id<BulkUploadMediatorDelegate> delegate;

- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_BULK_UPLOAD_COORDINATOR_BULK_UPLOAD_MEDIATOR_H_
