// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_MEDIATOR_H_

#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_view_controller_delegate.h"

#import <Foundation/Foundation.h>

@protocol BrandingConsumer;
class PrefService;

// A mediator object that handles the visibility of the autofill branding icon.
@interface BrandingMediator : NSObject <BrandingViewControllerDelegate>

// Consumer object that displays the autofill branding.
@property(nonatomic, weak) id<BrandingConsumer> consumer;

// Designated initializer; Local state should be provided.
- (instancetype)initWithLocalState:(PrefService*)localState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Method to disconnect the mediator from the application.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_MEDIATOR_H_
