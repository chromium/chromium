// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"

#include "base/feature_list.h"
#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_legacy_view_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"
#include "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_coordinator.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxPopupCoordinator () {
  std::unique_ptr<OmniboxPopupViewIOS> _popupView;
}

@property(nonatomic, strong)
    OmniboxPopupBaseViewController* popupViewController;
@property(nonatomic, strong) OmniboxPopupMediator* mediator;
@property(nonatomic, strong) ShortcutsCoordinator* shortcutsCoordinator;

@end

@implementation OmniboxPopupCoordinator

@synthesize browserState = _browserState;
@synthesize mediator = _mediator;
@synthesize popupViewController = _popupViewController;
@synthesize dispatcher = _dispatcher;

#pragma mark - Public

- (instancetype)initWithPopupView:
    (std::unique_ptr<OmniboxPopupViewIOS>)popupView {
  self = [super init];
  if (self) {
    _popupView = std::move(popupView);
  }
  return self;
}

- (void)start {
  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> imageFetcher =
      std::make_unique<image_fetcher::IOSImageDataFetcherWrapper>(
          self.browserState->GetSharedURLLoaderFactory());

  self.mediator = [[OmniboxPopupMediator alloc]
      initWithFetcher:std::move(imageFetcher)
        faviconLoader:IOSChromeFaviconLoaderFactory::GetForBrowserState(
                          self.browserState)
             delegate:_popupView.get()];
  self.mediator.dispatcher = (id<BrowserCommands>)self.dispatcher;
  self.mediator.webStateList = self.webStateList;
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(self.browserState);
  self.mediator.defaultSearchEngineIsGoogle =
      templateURLService && templateURLService->GetDefaultSearchProvider() &&
      templateURLService->GetDefaultSearchProvider()->GetEngineType(
          templateURLService->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

  if (base::FeatureList::IsEnabled(kNewOmniboxPopupLayout)) {
    self.popupViewController = [[OmniboxPopupViewController alloc] init];
  } else {
    self.popupViewController = [[OmniboxPopupLegacyViewController alloc] init];
  }
  self.popupViewController.incognito = self.browserState->IsOffTheRecord();

  BOOL isIncognito = self.browserState->IsOffTheRecord();
  self.mediator.incognito = isIncognito;
  self.mediator.consumer = self.popupViewController;
  self.mediator.presenter = [[OmniboxPopupPresenter alloc]
      initWithPopupPresenterDelegate:self.presenterDelegate
                 popupViewController:self.popupViewController
                           incognito:isIncognito];
  self.popupViewController.imageRetriever = self.mediator;
  self.popupViewController.faviconRetriever = self.mediator;
  self.popupViewController.delegate = self.mediator;
  [self.dispatcher
      startDispatchingToTarget:self.popupViewController
                   forProtocol:@protocol(OmniboxSuggestionCommands)];

  _popupView->SetMediator(self.mediator);
}

- (void)stop {
  [self.shortcutsCoordinator stop];
  _popupView.reset();
  [self.dispatcher
      stopDispatchingForProtocol:@protocol(OmniboxSuggestionCommands)];
}

- (BOOL)isOpen {
  return self.mediator.isOpen;
}

- (void)presentShortcutsIfNecessary {
  // Initialize the shortcuts feature when necessary.
  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxPopupShortcutIconsInZeroState) &&
      !self.browserState->IsOffTheRecord() && !self.shortcutsCoordinator) {
    self.shortcutsCoordinator = [[ShortcutsCoordinator alloc]
        initWithBaseViewController:self.popupViewController
                      browserState:self.browserState];
    self.shortcutsCoordinator.dispatcher =
        (id<ApplicationCommands, BrowserCommands,
            OmniboxFocuser>)(self.dispatcher);
    [self.shortcutsCoordinator start];
    self.popupViewController.shortcutsViewController =
        self.shortcutsCoordinator.viewController;
  }

  // Show shortcuts when the feature is enabled. Don't show them on NTP as they
  // are already part of the NTP.
  if (!IsVisibleURLNewTabPage(self.webStateList->GetActiveWebState()) &&
      base::FeatureList::IsEnabled(
          omnibox::kOmniboxPopupShortcutIconsInZeroState) &&
      !self.browserState->IsOffTheRecord()) {
    self.popupViewController.shortcutsEnabled = YES;
  }

  [self.mediator.presenter updatePopup];
  self.mediator.open = self.mediator.presenter.isOpen;
}

- (void)dismissShortcuts {
  self.popupViewController.shortcutsEnabled = NO;
  [self.mediator.presenter updatePopup];
  self.mediator.open = self.mediator.presenter.isOpen;
}

#pragma mark - Property accessor

- (BOOL)hasResults {
  return self.mediator.hasResults;
}

@end
