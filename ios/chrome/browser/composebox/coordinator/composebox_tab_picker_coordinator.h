// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_TAB_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_TAB_PICKER_COORDINATOR_H_

#include <set>

#import "ios/chrome/browser/composebox/coordinator/composebox_tab_picker_mediator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/web/public/web_state.h"

@protocol ComposeboxTabPickerCommands;
@protocol ComposeboxDebuggerLogger;
@class ComposeboxTheme;

// Responsible for processing the selection of tab picker.
@protocol ComposeboxTabPickerSelectionDelegate

// Returns the associated IDs for all currently attached tabs.
- (std::set<web::WebStateID>)allAttachedWebStateIDs;

// Returns the associated IDs for currently attached tabs from the current web
// state context. Tabs attached from different web states (not visible in the
// tab picker) will be excluded.
- (std::set<web::WebStateID>)attachedWebStateIDsInCurrentContext;

// Returns the number of non-tab attachments.
- (NSUInteger)nonTabAttachmentCount;

// Returns the max number of tab attachments.
- (NSUInteger)maxTabAttachmentCount;

// Attaches the selected tabs. `cachedWebStateIDs` contains the IDs of the
// tabs that have their content cached.
- (void)attachSelectedTabsWithWebStateIDs:
            (std::set<web::WebStateID>)selectedWebStateIDs
                        cachedWebStateIDs:
                            (std::set<web::WebStateID>)cachedWebStateIDs;

@end

// The tab picker coordinator for AIM.
@interface ComposeboxTabPickerCoordinator
    : ChromeCoordinator <ComposeboxTabsAttachmentDelegate>

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     theme:(ComposeboxTheme*)theme;

// Returns `YES` if the coordinator is started.
@property(nonatomic, readonly) BOOL started;

// Delegate for tab selection actions.
@property(nonatomic, weak) id<ComposeboxTabPickerSelectionDelegate> delegate;

// Delegate for logging events
@property(nonatomic, weak) id<ComposeboxDebuggerLogger> debugLogger;

// Handler for composebox tab picker commands.
@property(nonatomic, weak) id<ComposeboxTabPickerCommands>
    composeboxTabPickerHandler;

@end
#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_TAB_PICKER_COORDINATOR_H_
