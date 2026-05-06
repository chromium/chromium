// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_fre_view_controller_protocol.h"

enum class GeminiFREType;

@protocol GeminiConsentMutator;

// Gemini consent View Controller (VC).
@interface GeminiConsentViewController
    : UIViewController <GeminiFREViewControllerProtocol>

// Initializer for the consent VC with the properties needed to determine what
// UI to present. `country` is an optional field, leaving it as nil will show
// the default consent UI.
- (instancetype)initWithIsAccountManaged:(BOOL)isAccountManaged
                                 FREType:(GeminiFREType)FREType
                                 country:(NSString*)country;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<GeminiConsentMutator> mutator;

// The delegate to handle height changes and accordion toggles.
@property(nonatomic, weak) id<GeminiConsentViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_VIEW_CONTROLLER_H_
