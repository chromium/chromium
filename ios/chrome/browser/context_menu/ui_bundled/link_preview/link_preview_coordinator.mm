// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_coordinator.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_mediator.h"
#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_view_controller.h"
#import "ios/chrome/browser/history/model/history_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

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
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  CHECK(activeWebState);

  // To avoid losing the navigation history when the user navigates to
  // the previewed tab, clone the tab that will be replaced, and start
  // the navigation in the new tab.
  _previewWebState = activeWebState->Clone();

  // Attach tab helpers to use _previewWebState as a browser tab. It ensures
  // _previewWebState has all the expected tab helpers, including the
  // history tab helper which adding the history entry of the preview.
  AttachTabHelpers(_previewWebState.get(), TabHelperFilter::kPrerender);
  _previewWebState->SetWebUsageEnabled(true);

  // Delay the history record when showing the preview. (The history entry will
  // be added when the user tapping on the preview.)
  HistoryTabHelper::FromWebState(_previewWebState.get())
      ->SetDelayHistoryServiceNotification(true);
}

@end
