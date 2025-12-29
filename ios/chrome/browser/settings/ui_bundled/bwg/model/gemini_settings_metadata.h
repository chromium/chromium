// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_METADATA_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_METADATA_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, GeminiSettingsContext);

// Metadata about a Gemini setting item.
@interface GeminiSettingsMetadata : NSObject

// The title of a Gemini setting.
@property(nonatomic, copy, readonly) NSString* title;

// The subtitle for a Gemini setting.
@property(nonatomic, copy, readonly) NSString* subtitle;

// A unique enum value for a Gemini setting.
@property(nonatomic, readonly) GeminiSettingsContext context;

// Designated initializer.
- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                      context:(GeminiSettingsContext)context
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_SETTINGS_METADATA_H_
