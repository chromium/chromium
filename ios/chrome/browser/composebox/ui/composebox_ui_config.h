// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_CONFIG_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_CONFIG_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <unordered_map>

#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_option.h"
#import "ios/chrome/browser/composebox/public/composebox_mode.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"

// Bundles UI configuration (labels, hint text, and icon) for a single item.
@interface ComposeboxItemUIConfig : NSObject

// The label for the menu entry.
@property(nonatomic, readonly, copy) NSString* menuLabel;
// The label for the chip.
@property(nonatomic, readonly, copy) NSString* chipLabel;
// The label for the hint text.
@property(nonatomic, readonly, copy) NSString* hintText;
// The icon for the menu entry.
@property(nonatomic, readonly, strong) UIImage* icon;

// Creates a new instance without an icon.
- (instancetype)initWithMenuLabel:(NSString*)menuLabel
                        chipLabel:(NSString*)chipLabel
                         hintText:(NSString*)hintText;

// Creates a new instance.
- (instancetype)initWithMenuLabel:(NSString*)menuLabel
                        chipLabel:(NSString*)chipLabel
                         hintText:(NSString*)hintText
                             icon:(UIImage*)icon;

@end

// The composebox UI config, handling both server-provided values and
// local fallbacks for strings and icons. All returned strings are already
// localized.
@interface ComposeboxUIConfig : NSObject

// The title of the tools section.
@property(nonatomic, readonly, copy) NSString* toolsSectionHeader;

// The title of the model section.
@property(nonatomic, readonly, copy) NSString* modelSectionHeader;

// Creates a new instance with local fallback strings and icons.
+ (instancetype)localFallbackUIConfig;

// Creates a new instance based on the given mappings.
- (instancetype)
    initWithToolMapping:
        (std::unordered_map<ComposeboxMode, ComposeboxItemUIConfig*>)
            controlMapping
           modelMapping:
               (std::unordered_map<ComposeboxModelOption,
                                   ComposeboxItemUIConfig*>)modelMapping
     modelSectionHeader:(NSString*)modelSectionHeader
     toolsSectionHeader:(NSString*)toolsSectionHeader;

// Returns the menu label for the given tool.
- (NSString*)menuLabelForTool:(ComposeboxMode)tool;

// Returns the chip label for the given tool.
- (NSString*)chipLabelForTool:(ComposeboxMode)tool;

// Returns the accessibility label for removing the given active tool.
- (NSString*)removeToolAccessibilityLabelForTool:(ComposeboxMode)tool;

// Returns the hint text for the given tool.
- (NSString*)hintTextForTool:(ComposeboxMode)tool;

// Returns the menu icon for the given tool.
- (UIImage*)iconForTool:(ComposeboxMode)tool;

// Returns the menu label for the given model.
- (NSString*)menuLabelForModel:(ComposeboxModelOption)model;

// Returns the hint text for the given model.
- (NSString*)hintTextForModel:(ComposeboxModelOption)model;

// Returns the menu icon for the given model.
- (UIImage*)iconForModel:(ComposeboxModelOption)model;

// Returns the string for the given attachment option.
- (NSString*)stringForAttachmentOption:(ComposeboxAttachmentOption)option;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_CONFIG_H_
