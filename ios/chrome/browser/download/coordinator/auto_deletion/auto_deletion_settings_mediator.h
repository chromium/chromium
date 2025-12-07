// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_settings_mutator.h"

@protocol AutoDeletionSettingsConsumer;
class PrefService;

// Mediator for the Auto-deletion section of the Download settings menu.
@interface AutoDeletionSettingsMediator : NSObject <AutoDeletionSettingsMutator>

// Consumer for the Auto-deletion settings UI.
@property(nonatomic, weak) id<AutoDeletionSettingsConsumer>
    autoDeletionConsumer;

// Initialization.
- (instancetype)initWithLocalState:(PrefService*)localState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_SETTINGS_MEDIATOR_H_
