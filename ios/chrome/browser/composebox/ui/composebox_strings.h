// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_STRINGS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_STRINGS_H_

#import <Foundation/Foundation.h>

#include <unordered_map>

#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_option.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"

// Bundles a group of strings of the same type.
@interface ComposeboxStringBundle : NSObject

// The label for the menu entry.
@property(nonatomic, readonly, copy) NSString* menuLabel;
// The label for the chip.
@property(nonatomic, readonly, copy) NSString* chipLabel;
// The label for the hint text
@property(nonatomic, readonly, copy) NSString* hintText;

// Creates a new instance.
- (instancetype)initWithMenuLabel:(NSString*)menuLabel
                        chipLabel:(NSString*)chipLabel
                         hintText:(NSString*)hintText;

@end

// The composebox strings, handling both server-provided strings and local
// fallbacks. All returned strings are already localized.
@interface ComposeboxStrings : NSObject

// The title of the tools section.
@property(nonatomic, readonly, copy) NSString* toolsSectionHeader;

// The title of the model section.
@property(nonatomic, readonly, copy) NSString* modelSectionHeader;

// Creates a new instance with local fallback strings.
+ (instancetype)localFallbackStrings;

// Creates a new instance based on the given mappings.
- (instancetype)
    initWithToolMapping:
        (std::unordered_map<ComposeboxMode, ComposeboxStringBundle*>)
            controlMapping
           modelMapping:
               (std::unordered_map<ComposeboxModelOption,
                                   ComposeboxStringBundle*>)modelMapping
     modelSectionHeader:(NSString*)modelSectionHeader
     toolsSectionHeader:(NSString*)toolsSectionHeader;

// Returns the menu label for the given tool.
- (NSString*)menuLabelForTool:(ComposeboxMode)tool;

// Returns the chip label for the given tool.
- (NSString*)chipLabelForTool:(ComposeboxMode)tool;

// Returns the hint text for the given tool.
- (NSString*)hintTextForTool:(ComposeboxMode)tool;

// Returns the menu label for the given model.
- (NSString*)menuLabelForModel:(ComposeboxModelOption)model;

// Returns the hint text for the given model.
- (NSString*)hintTextForModel:(ComposeboxModelOption)model;

// Returns the string for the given attachment option.
- (NSString*)stringForAttachmentOption:(ComposeboxAttachmentOption)option;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_STRINGS_H_
