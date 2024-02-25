// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_ALERT_CONSTANTS_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_ALERT_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for a JavaScript dialog.
extern NSString* const kJavaScriptDialogAccessibilityIdentifier;
// Accessibility identifier added to the text field of JavaScript prompts.
extern NSString* const kJavaScriptDialogTextFieldAccessibilityIdentifier;
// Accessibility identifier for a media permissions dialog.
extern NSString* const kPermissionsDialogAccessibilityIdentifier;
// Accessibility identifier for an insecure form warning.
extern NSString* const kInsecureFormWarningAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_ALERT_CONSTANTS_H_
