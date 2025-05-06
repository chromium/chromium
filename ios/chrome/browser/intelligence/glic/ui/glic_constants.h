// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_UI_GLIC_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_UI_GLIC_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Sheet detents.
extern NSString* const kGLICConsentPartialDetentIdentifier;
extern NSString* const kGLICConsentFullDetentIdentifier;
extern const CGFloat kGLICConsentPartialDetentHeight;
extern const CGFloat kGLICConsentFullDetentHeight;
extern const CGFloat kGLICConsentPreferredCornerRadius;

// Stack view insets and spacing.
extern const CGFloat kGLICConsentMainStackHorizontalInset;
extern const CGFloat kGLICConsentMainStackTopInset;
extern const CGFloat kGLICConsentMainStackSpacing;

// Promo style strings.
extern NSString* const kGLICConsentPrimaryAction;
extern NSString* const kGLICConsentSecondaryAction;

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_UI_GLIC_CONSTANTS_H_
