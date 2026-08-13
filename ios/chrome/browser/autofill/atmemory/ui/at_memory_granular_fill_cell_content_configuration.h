// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_CELL_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_CELL_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Content configuration for a custom table view cell showing an AtMemory
// attribute name and a selectable value chip button.
@interface AtMemoryGranularFillCellContentConfiguration
    : NSObject <UIContentConfiguration>

// Text to display as the attribute name.
@property(nonatomic, copy) NSString* attributeName;

// Attribute value string to display as a selectable chip button.
@property(nonatomic, copy) NSString* attributeValue;

// Handler block called when the user taps on the value chip button.
@property(nonatomic, copy) void (^selectionHandler)(NSString* value);

// Returns a new default content configuration instance.
+ (instancetype)cellConfiguration;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_GRANULAR_FILL_CELL_CONTENT_CONFIGURATION_H_
