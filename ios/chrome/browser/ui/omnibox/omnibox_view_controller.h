// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/orchestrator/ui_bundled/edit_view_animatee.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/location_bar_offset_provider.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_view_consumer.h"

@class LayoutGuideCenter;
@protocol OmniboxKeyboardDelegate;
@protocol OmniboxReturnDelegate;
@class OmniboxViewController;
class OmniboxTextChangeDelegate;
@protocol TextFieldViewContaining;

// Delegate for text input changes in OmniboxViewController.
@protocol OmniboxViewControllerTextInputDelegate

// Called after the text input mode changes in the OmniboxViewController. This
// means that the active keyboard has changed.
- (void)omniboxViewControllerTextInputModeDidChange:
    (OmniboxViewController*)omniboxViewController;

@end

// Delegate for paste actions in OmniboxViewController.
@protocol OmniboxViewControllerPasteDelegate

// User tapped on the keyboard accessory's paste button.
- (void)didTapPasteToSearchButton:(NSArray<NSItemProvider*>*)itemProviders;
// User tapped on the Search Copied Text from the omnibox menu.
- (void)didTapSearchCopiedText;
// User tapped on the Search Copied Image from the omnibox menu.
- (void)didTapSearchCopiedImage;
// User tapped on the Lens Image from the omnibox menu.
- (void)didTapLensCopiedImage;
// User tapped on the Visit Copied Link from the omnibox menu.
- (void)didTapVisitCopiedLink;

@end

@interface OmniboxViewController : UIViewController <EditViewAnimatee,
                                                     LocationBarOffsetProvider,
                                                     OmniboxConsumer,
                                                     OmniboxViewConsumer>

/// Whether the UI is configured for search-only mode.
@property(nonatomic, assign) BOOL isSearchOnlyUI;

// The textfield used by this view controller.
@property(nonatomic, readonly, strong) OmniboxTextFieldIOS* textField;

// The view, which contains a text field view.
@property(nonatomic, readonly)
    UIView<TextFieldViewContaining>* viewContainingTextField;

// The default leading image to be used on omnibox focus before this is updated
// via OmniboxConsumer protocol.
@property(nonatomic, strong) UIImage* defaultLeadingImage;

// The default leading image to be used whenever the omnibox text is empty.
@property(nonatomic, strong) UIImage* emptyTextLeadingImage;

// The current semantic content attribute for the views this view controller
// manages
@property(nonatomic, assign)
    UISemanticContentAttribute semanticContentAttribute;

// The delegate for this object.
@property(nonatomic, weak) id<OmniboxViewControllerTextInputDelegate>
    textInputDelegate;
@property(nonatomic, weak) id<OmniboxViewControllerPasteDelegate> pasteDelegate;
@property(nonatomic, weak) id<OmniboxReturnDelegate> returnKeyDelegate;
@property(nonatomic, weak) id<OmniboxKeyboardDelegate> popupKeyboardDelegate;

// The layout guide center to use to refer to the omnibox leading image.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

- (instancetype)initWithIsLensOverlay:(BOOL)isLensOverlay
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (void)setTextChangeDelegate:(OmniboxTextChangeDelegate*)textChangeDelegate;

// Hides extra chrome, i.e. attributed text, and clears.
- (void)prepareOmniboxForScribble;
// Restores the chrome post-scribble.
- (void)cleanupOmniboxAfterScribble;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_CONTROLLER_H_
