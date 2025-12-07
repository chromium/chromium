// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_POPUP_OMNIBOX_POPUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_POPUP_OMNIBOX_POPUP_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/scoped_refptr.h"
#import "components/history/core/browser/top_sites.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_delegate.h"
#import "ios/chrome/browser/omnibox/ui/popup/carousel/carousel_item_menu_provider.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_mutator.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/favicon_retriever.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/image_retriever.h"
#import "ui/base/window_open_disposition.h"

@protocol ApplicationCommands;
@protocol OmniboxCommands;
@class BrowserActionFactory;
@class CarouselItem;
@protocol CarouselItemConsumer;
@class OmniboxAutocompleteController;
@class OmniboxImageFetcher;
@protocol OmniboxPopupConsumer;
@class OmniboxPopupMediator;
@class OmniboxPopupPresenter;
@class SceneState;
@protocol SnackbarCommands;
@protocol LoadQueryCommands;
class TemplateURLService;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

/// Provider that returns protocols and services that are instantiated after
/// OmniboxPopupCoordinator.
@protocol OmniboxPopupMediatorProtocolProvider

/// Returns the TopSites object to add/remove blocked URLs.
- (scoped_refptr<history::TopSites>)topSites;

/// Returns command handler for SnackbarCommands;
- (id<SnackbarCommands>)snackbarCommandsHandler;

@end

/// Delegate for share purposes, such as sharing URLs from the popup.
@protocol OmniboxPopupMediatorSharingDelegate

/// Called by `popupMediator` to share `URL` with `title`, originating from
/// `originView`.
- (void)popupMediator:(OmniboxPopupMediator*)mediator
             shareURL:(GURL)URL
                title:(NSString*)title
           originView:(UIView*)originView;
@end

@interface OmniboxPopupMediator
    : NSObject <OmniboxPopupMutator,
                OmniboxAutocompleteControllerDelegate,
                CarouselItemMenuProvider,
                ImageRetriever,
                FaviconRetriever>

/// Controller of the omnibox autocomplete.
@property(nonatomic, weak)
    OmniboxAutocompleteController* omniboxAutocompleteController;

@property(nonatomic, weak) id<OmniboxPopupConsumer> consumer;

@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;
@property(nonatomic, weak) id<OmniboxCommands> omniboxCommandsHandler;

/// Browser scene state to notify about events happening in this popup.
@property(nonatomic, weak) SceneState* sceneState;
@property(nonatomic, assign, getter=isIncognito) BOOL incognito;
/// Whether the popup is open.
@property(nonatomic, assign, getter=isOpen) BOOL open;
/// Presenter for the popup, handling the positioning and the presentation
/// animations.
@property(nonatomic, strong) OmniboxPopupPresenter* presenter;
/// Whether the default search engine is Google impacts which icon is used in
/// some cases
@property(nonatomic, assign) BOOL defaultSearchEngineIsGoogle;
/// Flag that marks that incognito actions are available. Those can be disabled
/// by an enterprise policy.
@property(nonatomic, assign) BOOL allowIncognitoActions;
/// Template URL service.
@property(nonatomic, assign) TemplateURLService* templateURLService;
/// Delegate for sharing popup content.
@property(nonatomic, weak) id<OmniboxPopupMediatorSharingDelegate>
    sharingDelegate;
@property(nonatomic, weak) id<OmniboxPopupMediatorProtocolProvider>
    protocolProvider;
@property(nonatomic, strong) BrowserActionFactory* mostVisitedActionFactory;
@property(nonatomic, weak) id<CarouselItemConsumer> carouselItemConsumer;

/// Designated initializer.
- (instancetype)initWithTracker:(feature_engagement::Tracker*)tracker
            omniboxImageFetcher:(OmniboxImageFetcher*)omniboxImageFetcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_POPUP_OMNIBOX_POPUP_MEDIATOR_H_
