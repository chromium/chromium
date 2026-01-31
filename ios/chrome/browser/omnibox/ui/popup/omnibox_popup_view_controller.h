// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_keyboard_delegate.h"
#import "ios/chrome/browser/omnibox/ui/popup/carousel/carousel_item.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_consumer.h"

@protocol AutocompleteSuggestionGroup;
@protocol CarouselItemMenuProvider;
@protocol FaviconRetriever;
class LargeIconCache;
namespace favicon {
class LargeIconService;
}
@class LayoutGuideCenter;
@protocol OmniboxPopupMutator;
@protocol ImageRetriever;

/// View controller used to display a list of omnibox autocomplete matches in
/// the omnibox popup. It implements up/down arrow handling to highlight
/// autocomplete results. Ideally, that should be implemented as key commands in
/// this view controller, but UITextField has standard handlers for up/down
/// arrows, so when the omnibox is the first responder, this view controller
/// cannot receive these events. Hence the delegation.
@interface OmniboxPopupViewController
    : UIViewController <OmniboxPopupConsumer,
                        CarouselItemConsumer,
                        OmniboxKeyboardDelegate,
                        UIScrollViewDelegate>

@property(nonatomic, assign) BOOL incognito;
@property(nonatomic, weak) id<OmniboxPopupMutator> mutator;
@property(nonatomic, weak) id<ImageRetriever> imageRetriever;
@property(nonatomic, weak) id<FaviconRetriever> faviconRetriever;
@property(nonatomic, assign) favicon::LargeIconService* largeIconService;
@property(nonatomic, assign) LargeIconCache* largeIconCache;
@property(nonatomic, weak) id<CarouselItemMenuProvider> carouselMenuProvider;

/// View controller that displays debug information.
/// Must only be set when OmniboxDebuggingEnabled flag is set.
/// When set, this view controller will present `debugInfoViewController` when a
/// special debug gesture is executed.
@property(nonatomic, strong) UIViewController* debugInfoViewController;

/// The layout guide center to use to refer to the omnibox leading image.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Whether the contained table view has content.
@property(nonatomic, readonly) BOOL hasContent;

@property(nonatomic, strong)
    NSArray<id<AutocompleteSuggestionGroup>>* currentResult;

- (instancetype)initWithPresentationContext:
    (OmniboxPresentationContext)presentationContext NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Sets the additional vertical content inset for the scroll view.
- (void)setAdditionalVerticalContentInset:
    (UIEdgeInsets)additionalVerticalContentInset;

/// Toggle visibility of the omnibox debugger view.
- (void)toggleOmniboxDebuggerView;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_VIEW_CONTROLLER_H_
