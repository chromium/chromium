// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_SERVER_STRINGS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_SERVER_STRINGS_H_

#import <Foundation/Foundation.h>

#include <unordered_map>

#import "ios/chrome/browser/composebox/public/composebox_input_plate_controls.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"

// Bundles a group of strings of the same type.
@interface ComposeboxServerStringBundle : NSObject

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

// The server side composebox strings.
@interface ComposeboxServerStrings : NSObject

// Creates a new insatance based on the given mappings.
- (instancetype)
    initWithToolMapping:
        (std::unordered_map<ComposeboxInputPlateControls,
                            ComposeboxServerStringBundle*>)controlMapping
           modelMapping:
               (std::unordered_map<ComposeboxModelOption,
                                   ComposeboxServerStringBundle*>)modelMapping
     modelSectionHeader:(NSString*)modelSectionHeader
     toolsSectionHeader:(NSString*)toolsSectionHeader;

// The title of the model section.
@property(nonatomic, copy) NSString* modelSectionHeader;

// The title of the tools section.
@property(nonatomic, copy) NSString* toolsSectionHeader;

// Returns the menu label for the given control if present, otherwise `nil`.
- (ComposeboxServerStringBundle*)stringsForControl:
    (ComposeboxInputPlateControls)control;

// Returns the menu label for the given model if present, otherwise `nil`.
- (ComposeboxServerStringBundle*)stringsForModel:
    (ComposeboxModelOption)modelOption;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_SERVER_STRINGS_H_
