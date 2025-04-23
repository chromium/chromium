// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESENTATION_DELEGATE_H_

#import <UIKit/UIKit.h>

/**
 * The `HomeCustomizationBackgroundPickerPresentationDelegate` protocol is used
 * to inform the delegate when the user interacts with the
 * `HomeCustomizationBackgroundPickerCell` and requests options to customize the
 * background.
 */
@protocol HomeCustomizationBackgroundPickerPresentationDelegate <NSObject>

// Presents a background picker alert with options such as gallery, camera roll,
// or color selection.
- (void)showBackgroundPickerOptions;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESENTATION_DELEGATE_H_
