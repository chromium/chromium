// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_LABEL_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_LABEL_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"

// Content configuration for a view with a single label.
@interface LabelContentConfiguration : NSObject <ChromeContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The text to display.
@property(nonatomic, copy) NSString* text;

// LINT.ThenChange(label_content_configuration.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_LABEL_CONTENT_CONFIGURATION_H_
