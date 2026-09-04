// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_VCARD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_VCARD_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol VcardMediatorDelegate;
class WebStateList;

// Mediator that monitors the active WebState in WebStateList and instructs
// VcardCoordinator to dismiss any presented vCard UI when navigation or
// WebState switching occurs.
@interface VcardMediator : NSObject

// Delegate for vCard UI actions.
@property(nonatomic, weak) id<VcardMediatorDelegate> delegate;

// Designated initializer.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_VCARD_MEDIATOR_H_
