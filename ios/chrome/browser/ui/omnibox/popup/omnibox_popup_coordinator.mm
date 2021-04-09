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
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"
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

@property(nonatomic, strong) OmniboxPopupViewController* popupViewController;
@property(nonatomic, strong) OmniboxPopupMediator* mediator;

@end

@implementation OmniboxPopupCoordinator

@synthesize mediator = _mediator;
@synthesize popupViewController = _popupViewController;

#pragma mark - Public

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                     popupView:(std::unique_ptr<OmniboxPopupViewIOS>)popupView {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _popupView = std::move(popupView);
  }
  return self;
}

- (void)start {
  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> imageFetcher =
      std::make_unique<image_fetcher::IOSImageDataFetcherWrapper>(
          self.browser->GetBrowserState()->GetSharedURLLoaderFactory());

  self.mediator = [[OmniboxPopupMediator alloc]
      initWithFetcher:std::move(imageFetcher)
        faviconLoader:IOSChromeFaviconLoaderFactory::GetForBrowserState(
                          self.browser->GetBrowserState())
             delegate:_popupView.get()];
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.mediator.dispatcher =
      static_cast<id<BrowserCommands>>(self.browser->GetCommandDispatcher());
  self.mediator.webStateList = self.browser->GetWebStateList();
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator.defaultSearchEngineIsGoogle =
      templateURLService && templateURLService->GetDefaultSearchProvider() &&
      templateURLService->GetDefaultSearchProvider()->GetEngineType(
          templateURLService->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

  self.popupViewController = [[OmniboxPopupViewController alloc] init];
  self.popupViewController.incognito =
      self.browser->GetBrowserState()->IsOffTheRecord();

  BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();
  self.mediator.incognito = isIncognito;
  self.mediator.consumer = self.popupViewController;
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  self.mediator.promoScheduler =
      [DefaultBrowserSceneAgent agentFromScene:sceneState].nonModalScheduler;
  self.mediator.presenter = [[OmniboxPopupPresenter alloc]
      initWithPopupPresenterDelegate:self.presenterDelegate
                 popupViewController:self.popupViewController
                           incognito:isIncognito];
  self.popupViewController.imageRetriever = self.mediator;
  self.popupViewController.faviconRetriever = self.mediator;
  self.popupViewController.delegate = self.mediator;
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self.popupViewController
                   forProtocol:@protocol(OmniboxSuggestionCommands)];

  _popupView->SetMediator(self.mediator);
}

- (void)stop {
  _popupView.reset();
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(OmniboxSuggestionCommands)];
}

- (BOOL)isOpen {
  return self.mediator.isOpen;
}

#pragma mark - Property accessor

- (BOOL)hasResults {
  return self.mediator.hasResults;
}

@end
