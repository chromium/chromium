// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_COMPOSEBOX_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_COMPOSEBOX_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_animation_context_provider.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_consumer.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_mutator.h"

@protocol AIMPrototypeComposeboxMutator;
@class AIMPrototypeComposeboxViewController;
@protocol TextFieldViewContaining;

// Delegate for the AIM prototype composebox view controller.
@protocol AIMPrototypeComposeboxViewControllerDelegate
- (void)aimPrototypeViewControllerDidTapGalleryButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
- (void)aimPrototypeViewControllerDidTapMicButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
- (void)aimPrototypeViewControllerDidTapCameraButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
- (void)aimPrototypeViewControllerMayShowGalleryPicker:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
- (void)aimPrototypeViewControllerDidTapFileButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
- (void)aimPrototypeViewControllerDidTapAttachTabsButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController;
@end

// View controller for the AIM prototype composebox.
@interface AIMPrototypeComposeboxViewController
    : UIViewController <AIMPrototypeAnimationContextProvider,
                        AIMPrototypeComposeboxConsumer>

@property(nonatomic, weak) id<AIMPrototypeComposeboxViewControllerDelegate>
    delegate;
@property(nonatomic, weak) id<AIMPrototypeComposeboxMutator> mutator;

/// Sets the omnibox edit view.
- (void)setEditView:(UIView<TextFieldViewContaining>*)editView;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_COMPOSEBOX_VIEW_CONTROLLER_H_
