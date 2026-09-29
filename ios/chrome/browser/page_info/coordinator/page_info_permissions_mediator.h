// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_COORDINATOR_PAGE_INFO_PERMISSIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_COORDINATOR_PAGE_INFO_PERMISSIONS_MEDIATOR_H_

#import "ios/chrome/browser/permissions/ui_bundled/permissions_delegate.h"

@protocol PermissionsConsumer;
class HostContentSettingsMap;

namespace web {
class WebState;
}

// Mediator for the page info permissions.
@interface PageInfoPermissionsMediator : NSObject <PermissionsDelegate>

// Consumer that is configured by this mediator.
@property(nonatomic, weak) id<PermissionsConsumer> consumer;

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer that reads information from `webState` and
// `hostContentSettingsMap` to establish the permissions state.
- (instancetype)initWithWebState:(web::WebState*)webState
          hostContentSettingsMap:(HostContentSettingsMap*)hostContentSettingsMap
    NS_DESIGNATED_INITIALIZER;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_COORDINATOR_PAGE_INFO_PERMISSIONS_MEDIATOR_H_
