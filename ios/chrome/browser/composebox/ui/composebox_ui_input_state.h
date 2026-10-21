// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_INPUT_STATE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_INPUT_STATE_H_

#import <UIKit/UIKit.h>

#import <unordered_set>

#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_option.h"

@class ComposeboxMenuSharedTab;
@class ComposeboxModelItem;
@class ComposeboxUIConfig;

// State object containing all UI input state for the composebox.
@interface ComposeboxUIInputState : NSObject

/// The shared tabs that are attached.
@property(nonatomic, copy) NSArray<ComposeboxMenuSharedTab*>* sharedTabs;

/// The favicon of the current tab, if available.
@property(nonatomic, strong) UIImage* currentTabFavicon;

/// The set of attachment options that are allowed to be shown.
@property(nonatomic, assign) std::unordered_set<ComposeboxAttachmentOption>
    allowedAttachments;

/// The set of attachment options that should be disabled.
@property(nonatomic, assign) std::unordered_set<ComposeboxAttachmentOption>
    disabledAttachments;

/// The set of tools that are allowed to be shown.
@property(nonatomic, assign) std::unordered_set<ComposeboxMode> allowedTools;

/// The set of tools that should be disabled.
@property(nonatomic, assign) std::unordered_set<ComposeboxMode> disabledTools;

/// The model items to display in the picker, in server order. Models without a
/// label are excluded.
@property(nonatomic, copy) NSArray<ComposeboxModelItem*>* modelItems;

/// The composebox UI config (strings, headers, and icons).
@property(nonatomic, strong) ComposeboxUIConfig* uiConfig;

/// The remaining capacity for attachments.
@property(nonatomic, assign) NSUInteger remainingAttachmentCapacity;

/// The remaining number of images allowed.
@property(nonatomic, assign) NSUInteger remainingNumberOfImagesAllowed;

/// The absolute maximum number of tabs allowed to be attached.
@property(nonatomic, assign) NSUInteger maxTabAttachmentCount;

/// Whether the model picker is allowed to be shown.
@property(nonatomic, assign) BOOL allowModelPicker;

/// The currently active tool.
@property(nonatomic, assign) ComposeboxMode activeTool;

/// Whether the given attachment option should be hidden.
- (BOOL)isAttachmentHidden:(ComposeboxAttachmentOption)option;

/// Whether the given attachment option should be disabled.
- (BOOL)isAttachmentDisabled:(ComposeboxAttachmentOption)option;

/// Whether the given attachment option is available (allowed and not
/// disabled).
- (BOOL)isAttachmentAvailable:(ComposeboxAttachmentOption)option;

/// Whether the given tool should be hidden.
- (BOOL)isToolHidden:(ComposeboxMode)option;

/// Whether the given tool should be disabled.
- (BOOL)isToolDisabled:(ComposeboxMode)option;

/// Whether the given tool is available (allowed and not disabled).
- (BOOL)isToolAvailable:(ComposeboxMode)option;

/// Returns whether global testing mode for making all tools accessible is
/// active.
+ (BOOL)allToolsEnabled;

/// Switches the global accessibility override status.
+ (void)setAllToolsEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_INPUT_STATE_H_
