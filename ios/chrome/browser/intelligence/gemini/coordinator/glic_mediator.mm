// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_mediator.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/gemini/metrics/glic_metrics.h"
#import "ios/chrome/browser/intelligence/gemini/model/glic_service.h"
#import "ios/chrome/browser/intelligence/gemini/model/glic_service_factory.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@interface GLICMediator ()

// The base view controller to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

@end

@implementation GLICMediator {
  // Browser instance.
  raw_ptr<Browser> _browser;

  // Pref service to check if user flows were previously triggered.
  raw_ptr<PrefService> _prefService;

  // The PageContext wrapper used to provide context about a page.
  PageContextWrapper* _pageContextWrapper;
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

- (void)presentGlicFlow {
  if (GLICPromoConsentVariationsParam() ==
      GLICPromoConsentVariations::kSkipConsent) {
    [self prepareGLICOverlay];
    return;
  }

  // TODO(crbug.com/419064727): Determine what flow should be shown.
  [self.delegate presentGlicFRE];
}

#pragma mark - GLICConsentMutator

// Did consent to GLIC.
- (void)didConsentGLIC {
  base::UmaHistogramEnumeration(kGLICConsentTypeHistogram,
                                GLICConsentType::kAccept);
  _prefService->SetBoolean(prefs::kIOSGLICConsent, YES);
  [_delegate dismissGLICConsentUI];
}

// Did dismisses the Consent UI.
- (void)didRefuseGLICConsent {
  base::UmaHistogramEnumeration(kGLICConsentTypeHistogram,
                                GLICConsentType::kCancel);
  [_delegate dismissGLICConsentUI];
}

// Did close GLIC Promo UI.
- (void)didCloseGLICPromo {
  [_delegate dismissGLICConsentUI];
}

#pragma mark - Private

// Prepares GLIC overlay.
// TODO(crbug.com/419064727): Add entry point to call this function.
- (void)prepareGLICOverlay {
  // Cancel any ongoing page context operation.
  if (_pageContextWrapper) {
    _pageContextWrapper = nil;
  }

  // Configure the callback to be executed once the page context is ready.
  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(
      std::unique_ptr<optimization_guide::proto::PageContext>)>
      page_context_completion_callback = base::BindOnce(^void(
          std::unique_ptr<optimization_guide::proto::PageContext>
              page_context) {
        GLICMediator* strongSelf = weakSelf;
        [strongSelf openGLICOverlayForPage:std::move(page_context)];
        strongSelf->_pageContextWrapper = nil;
      });

  // Collect the PageContext and execute the callback once it's ready.
  _pageContextWrapper = [[PageContextWrapper alloc]
        initWithWebState:_browser->GetWebStateList()->GetActiveWebState()
      completionCallback:std::move(page_context_completion_callback)];
  [_pageContextWrapper setShouldGetInnerText:YES];
  [_pageContextWrapper setShouldGetSnapshot:YES];
  [_pageContextWrapper populatePageContextFieldsAsync];
}

// Opens the GLIC overlay with a given page context.
- (void)openGLICOverlayForPage:
    (std::unique_ptr<optimization_guide::proto::PageContext>)pageContext {
  GlicService* glicService =
      GlicServiceFactory::GetForProfile(_browser->GetProfile());
  glicService->PresentOverlayOnViewController(self.baseViewController,
                                              std::move(pageContext));

  // TODO(crbug.com/419064727): Dismiss glic promo/consent.
}

@end
