// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

@interface BWGMediator ()

// The base view controller to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

@end

@implementation BWGMediator {
  // Browser instance.
  raw_ptr<Browser> _browser;

  // Pref service to check if user flows were previously triggered.
  raw_ptr<PrefService> _prefService;

  // The PageContext wrapper used to provide context about a page.
  PageContextWrapper* _pageContextWrapper;

  // Start time for the preparation of the presentation of BWG overlay.
  base::TimeTicks _BWGOverlayPreparationStartTime;

  // Whether the FRE was presented for the current BWG instance.
  BOOL _didPresentBWGFRE;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                            browser:(Browser*)browser
                 baseViewController:(UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    _browser = browser;
    _prefService = prefService;
    _baseViewController = baseViewController;
  }
  return self;
}

- (void)presentBWGFlow {
  _BWGOverlayPreparationStartTime = base::TimeTicks::Now();

  switch (BWGPromoConsentVariationsParam()) {
    case BWGPromoConsentVariations::kSkipConsent:
      [self prepareBWGOverlay];
      return;
    case BWGPromoConsentVariations::kForceFRE:
      // Resetting the consent pref will allow the BWG flow to act as if consent
      // was never given.
      _prefService->SetBoolean(prefs::kIOSBwgConsent, NO);
      break;
    default:
      break;
  }

  _didPresentBWGFRE = [self.delegate maybePresentBWGFRE];
  // Not presenting the FRE implies that the promo was shown and user consent
  // was given which means we can navigate to the BWG overlay immediately.
  if (!_didPresentBWGFRE) {
    [self prepareBWGOverlay];
  }
}

#pragma mark - BWGConsentMutator

// Did consent to BWG.
- (void)didConsentBWG {
  _prefService->SetBoolean(prefs::kIOSBwgConsent, YES);
  __weak __typeof(self) weakSelf = self;
  [_delegate dismissBWGConsentUIWithCompletion:^{
    [weakSelf prepareBWGOverlay];
  }];
}

// Did dismisses the Consent UI.
- (void)didRefuseBWGConsent {
  [_delegate dismissBWGFlow];
}

// Did close BWG Promo UI.
- (void)didCloseBWGPromo {
  [_delegate dismissBWGFlow];
}

// Open a new tab page given a URL.
- (void)openNewTabWithURL:(const GURL&)URL {
  [self FREWillBeBackgrounded];
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [HandlerForProtocol(_browser->GetCommandDispatcher(), ApplicationCommands)
      openURLInNewTab:command];
}

#pragma mark - Private

// Prepares BWG overlay.
- (void)prepareBWGOverlay {
  // Cancel any ongoing page context operation.
  if (_pageContextWrapper) {
    _pageContextWrapper = nil;
  }

  // Configure the callback to be executed once the page context is ready.
  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(PageContextWrapperCallbackResponse)>
      page_context_completion_callback =
          base::BindOnce(^void(PageContextWrapperCallbackResponse response) {
            BWGMediator* strongSelf = weakSelf;
            if (!strongSelf) {
              return;
            }

            [strongSelf openBWGOverlayForPage:std::move(response)];
            strongSelf->_pageContextWrapper = nil;
          });

  // Collect the PageContext and execute the callback once it's ready.
  _pageContextWrapper = [[PageContextWrapper alloc]
        initWithWebState:_browser->GetWebStateList()->GetActiveWebState()
      completionCallback:std::move(page_context_completion_callback)];
  [_pageContextWrapper setShouldGetAnnotatedPageContent:YES];
  [_pageContextWrapper setShouldGetSnapshot:YES];
  [_pageContextWrapper populatePageContextFieldsAsync];
}

// Opens the BWG overlay with a given PageContextWrapperCallbackResponse.
- (void)openBWGOverlayForPage:
    (PageContextWrapperCallbackResponse)pageContextWrapperResponse {
  BwgBrowserAgent* BWGBrowserAgent = BwgBrowserAgent::FromBrowser(_browser);
  BWGBrowserAgent->PresentBwgOverlay(self.baseViewController,
                                     std::move(pageContextWrapperResponse));

  base::UmaHistogramTimes(
      _didPresentBWGFRE ? kStartupTimeWithFREHistogram
                        : kStartupTimeNoFREHistogram,
      base::TimeTicks::Now() - _BWGOverlayPreparationStartTime);

  // TODO(crbug.com/419064727): Dismiss bwg promo/consent.
}

// Notifies the currently active WebState's BWG tab helper that the FRE will be
// backgrounded.
- (void)FREWillBeBackgrounded {
  BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
  if (!BWGTabHelper) {
    return;
  }

  BWGTabHelper->SetBwgUiShowing(false);
  BWGTabHelper->PrepareBwgFreBackgrounding();
}

// Returns the currently active WebState's BWG tab helper.
- (BwgTabHelper*)activeWebStateBWGTabHelper {
  web::WebState* activeWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!activeWebState) {
    return nil;
  }

  return BwgTabHelper::FromWebState(activeWebState);
}

@end
