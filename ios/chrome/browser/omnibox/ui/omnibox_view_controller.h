// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_consumer.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/edit_view_animatee.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/location_bar_offset_provider.h"

@class LayoutGuideCenter;
@protocol OmniboxMutator;
@protocol OmniboxKeyboardDelegate;
@protocol OmniboxTextInput;
@protocol TextFieldViewContaining;
@class OmniboxMetricsRecorder;

@interface OmniboxViewController : UIViewController <EditViewAnimatee,
                                                     LocationBarOffsetProvider,
                                                     OmniboxConsumer>

/// Mutator of the omnibox.
@property(nonatomic, weak) id<OmniboxMutator> mutator;

/// The metrics recorder.
@property(nonatomic, weak) OmniboxMetricsRecorder* metricsRecorder;

/// Whether the UI is configured for search-only mode.
@property(nonatomic, assign) BOOL searchOnlyUI;

// The text input used by this view controller.
@property(nonatomic, readonly, strong) id<OmniboxTextInput> textInput;

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

/// Delegate for keyboard actions.
@property(nonatomic, weak) id<OmniboxKeyboardDelegate> popupKeyboardDelegate;

// The layout guide center to use to refer to the omnibox leading image.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

- (instancetype)initWithPresentationContext:
    (OmniboxPresentationContext)presentationContext NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Hides extra chrome, i.e. attributed text, and clears.
- (void)prepareOmniboxForScribble;
// Restores the chrome post-scribble.
- (void)cleanupOmniboxAfterScribble;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_VIEW_CONTROLLER_H_
