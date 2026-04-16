// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_INPUT_STATE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_INPUT_STATE_H_

#import <UIKit/UIKit.h>

#import <unordered_set>

#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_option.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"

@class ComposeboxStrings;

// State object containing all UI input state for the composebox.
@interface ComposeboxUIInputState : NSObject

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

/// The set of models that are allowed to be shown in the picker.
@property(nonatomic, assign) std::unordered_set<ComposeboxModelOption>
    allowedModels;

/// The set of models that should be disabled in the picker.
@property(nonatomic, assign) std::unordered_set<ComposeboxModelOption>
    disabledModels;

/// The localized composebox strings.
@property(nonatomic, strong) ComposeboxStrings* strings;

/// The remaining capacity for attachments.
@property(nonatomic, assign) NSUInteger remainingAttachmentCapacity;

/// Whether the model picker is allowed to be shown.
@property(nonatomic, assign) BOOL allowModelPicker;

/// The currently active tool.
@property(nonatomic, assign) ComposeboxMode activeTool;

/// The currently active model.
@property(nonatomic, assign) ComposeboxModelOption activeModel;

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

/// Whether the given model should be hidden.
- (BOOL)isModelHidden:(ComposeboxModelOption)option;

/// Whether the given model should be disabled.
- (BOOL)isModelDisabled:(ComposeboxModelOption)option;

/// Whether the given model is available (allowed and not disabled).
- (BOOL)isModelAvailable:(ComposeboxModelOption)option;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_INPUT_STATE_H_
