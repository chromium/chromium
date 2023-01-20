// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/link_preview/link_preview_coordinator.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_tab_helper.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/ui/context_menu/link_preview/link_preview_mediator.h"
#import "ios/chrome/browser/ui/context_menu/link_preview/link_preview_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LinkPreviewCoordinator () {
  // The WebState used for loading the preview.
  std::unique_ptr<web::WebState> _previewWebState;
}

// Mediator that updates the UI when loading the preview.
@property(nonatomic, strong) LinkPreviewMediator* mediator;

// View controller of the preview.
@property(nonatomic, strong) LinkPreviewViewController* viewController;

// URL of the preview.
@property(nonatomic, assign) GURL URL;

@end

@implementation LinkPreviewCoordinator

- (instancetype)initWithBrowser:(Browser*)browser URL:(const GURL&)URL {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _URL = URL;
  }
  return self;
}

- (void)start {
  [self configureWebState];

  // Get the origin of the preview.
  NSString* origin = base::SysUTF16ToNSString(
      url_formatter::FormatUrl(self.URL.DeprecatedGetOriginAsURL()));

  self.viewController = [[LinkPreviewViewController alloc]
      initWithView:_previewWebState->GetView()
            origin:origin];
  self.mediator =
      [[LinkPreviewMediator alloc] initWithWebState:_previewWebState.get()
                                         previewURL:self.URL
                                           referrer:self.referrer];
  self.mediator.consumer = self.viewController;
  _previewWebState->GetNavigationManager()->LoadIfNecessary();
}

- (void)stop {
  self.viewController = nil;
  _previewWebState.reset();
}

- (UIViewController*)linkPreviewViewController {
  return self.viewController;
}

- (void)handlePreviewAction {
  WebStateList* web_state_list = self.browser->GetWebStateList();
  DCHECK_NE(WebStateList::kInvalidIndex, web_state_list->active_index());
  DCHECK(_previewWebState);

  // The WebState will be converted to a proper tab. Record navigations to the
  // HistoryService.
  HistoryTabHelper::FromWebState(_previewWebState.get())
      ->SetDelayHistoryServiceNotification(false);

  // Reset auto layout for preview before expanding it to a tab.
  [self.viewController resetAutoLayoutForPreview];

  web_state_list->ReplaceWebStateAt(web_state_list->active_index(),
                                    std::move(_previewWebState));
}

#pragma mark - Private

// Configures the web state that used to load the preview.
- (void)configureWebState {
  // Make a copy of the active web state.
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  ChromeBrowserState* browserState =
      ChromeBrowserState::FromBrowserState(currentWebState->GetBrowserState());

  // Use web::WebState::CreateWithStorageSession to clone the
  // currentWebState navigation history. This may create an
  // unrealized WebState, however, LinkPreview needs a realized
  // one, so force the realization.
  // TODO(crbug.com/1291626): remove when there is a way to
  // clone a WebState navigation history.
  web::WebState::CreateParams createParams(browserState);
  createParams.last_active_time = base::Time::Now();
  _previewWebState = web::WebState::CreateWithStorageSession(
      createParams, currentWebState->BuildSessionStorage());
  _previewWebState->ForceRealized();

  // Attach tab helpers to use _previewWebState as a browser tab. It ensures
  // _previewWebState has all the expected tab helpers, including the
  // history tab helper which adding the history entry of the preview.
  AttachTabHelpers(_previewWebState.get(), /*for_prerender=*/true);
  _previewWebState->SetWebUsageEnabled(true);

  // Delay the history record when showing the preview. (The history entry will
  // be added when the user tapping on the preview.)
  HistoryTabHelper::FromWebState(_previewWebState.get())
      ->SetDelayHistoryServiceNotification(true);
}

@end
