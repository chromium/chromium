// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_ACCESSIBILITY_CONFIGURATION_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_ACCESSIBILITY_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Accessibility related configuration for Lens.
@interface ChromeLensAccessibilityConfiguration : NSObject

// Accessibility label for the close button.
@property(nonatomic, copy) NSString* closeButtonAccessibilityLabel;

// Accessibility hint for the close button.
@property(nonatomic, copy) NSString* closeButtonAccessibilityHint;

// Accessibility label for the menu button.
@property(nonatomic, copy) NSString* menuButtonAccessibilityLabel;

// Accessibility hint for the menu button.
@property(nonatomic, copy) NSString* menuButtonAccessibilityHint;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_ACCESSIBILITY_CONFIGURATION_H_
