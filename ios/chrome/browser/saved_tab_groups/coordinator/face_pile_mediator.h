// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_COORDINATOR_FACE_PILE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_COORDINATOR_FACE_PILE_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

namespace signin {
class IdentityManager;
}  // namespace signin

@protocol FacePileConsumer;
@class FacePileConfiguration;
class ShareKitService;

// Mediator for the FacePile feature.
// This class is responsible for fetching data and communicating updates
// to its FacePileView consumer.
@interface FacePileMediator : NSObject

// The consumer for face pile data updates.
@property(nonatomic, weak) id<FacePileConsumer> consumer;

- (instancetype)initWithConfiguration:(FacePileConfiguration*)configuration
                   dataSharingService:
                       (data_sharing::DataSharingService*)dataSharingService
                      shareKitService:(ShareKitService*)shareKitService
                      identityManager:(signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator, cleaning up any observers or ongoing tasks.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_COORDINATOR_FACE_PILE_MEDIATOR_H_
