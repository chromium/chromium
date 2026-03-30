// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_DATE_PICKER_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_DATE_PICKER_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"

// Content configuration for a view with a single date picker.
@interface DatePickerContentConfiguration
    : NSObject <ChromeContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The view's picker is configured to call `selector` on `target`.
@property(nonatomic, weak) id target;
@property(nonatomic, assign) SEL selector;

// Whether the picker allows user interaction.
@property(nonatomic, assign) BOOL userInteractionEnabled;

// The date value to show.
@property(nonatomic, strong) NSDate* date;

// LINT.ThenChange(date_picker_content_configuration.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_DATE_PICKER_CONTENT_CONFIGURATION_H_
