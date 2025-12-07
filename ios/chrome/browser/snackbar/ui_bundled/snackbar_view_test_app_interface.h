// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNACKBAR_UI_BUNDLED_SNACKBAR_VIEW_TEST_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SNACKBAR_UI_BUNDLED_SNACKBAR_VIEW_TEST_APP_INTERFACE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// App interface for testing SnackbarView.
@interface SnackbarViewTestAppInterface : NSObject

// Presents a snackbar for testing with the specified properties.
// Unused properties can be nil.
// Presents a snackbar for testing with the specified properties.
// Unused properties can be nil.
+ (void)showSnackbarWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
            secondarySubtitle:(NSString*)secondarySubtitle
                   buttonText:(NSString*)buttonText
          hasLeadingAccessory:(BOOL)hasLeadingAccessory
         hasTrailingAccessory:(BOOL)hasTrailingAccessory;

// Presents a snackbar for testing with the specified title after dismissing
// the keyboard.
+ (void)showSnackbarMessageAfterDismissingKeyboardWithTitle:(NSString*)title;

// Makes a dummy text field first responder to show the keyboard.
+ (void)makeTextFieldFirstResponder;

// Removes the dummy text field from the view hierarchy.
+ (void)removeDummyTextField;

// Returns the dummy text field.
+ (UITextField*)dummyTextField;

@end

#endif  // IOS_CHROME_BROWSER_SNACKBAR_UI_BUNDLED_SNACKBAR_VIEW_TEST_APP_INTERFACE_H_
