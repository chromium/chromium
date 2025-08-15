// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"

#import "components/prefs/pref_service.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/web_state.h"

@implementation PageActionMenuMediator {
  // The web state referenced by this mediator.
  raw_ptr<web::WebState> _webState;

  // The `PrefService` used to store reminder data.
  raw_ptr<PrefService> _profilePrefs;

  // The service to retrieve default search engine URL.
  raw_ptr<TemplateURLService> _templateURLService;

  // The service for the Gemini floaty.
  raw_ptr<BwgService> _BWGService;

  // The tab helper for Reader mode.
  raw_ptr<ReaderModeTabHelper> _readerModeTabHelper;
}

- (instancetype)initWithWebState:(web::WebState*)webState
              profilePrefService:(PrefService*)profilePrefs
              templateURLService:(TemplateURLService*)templateURLService
                      BWGService:(BwgService*)BWGService
             readerModeTabHelper:(ReaderModeTabHelper*)readerModeTabHelper {
  self = [super init];
  if (self) {
    _webState = webState;
    _profilePrefs = profilePrefs;
    _templateURLService = templateURLService;
    _BWGService = BWGService;
    _readerModeTabHelper = readerModeTabHelper;
  }
  return self;
}

#pragma mark - Public

- (BOOL)isLensAvailableForProfile {
  return IsLensOverlayAvailable(_profilePrefs);
}

#pragma mark - PageActionMenuMutator

- (BOOL)isLensAvailableForTraitCollection:(UITraitCollection*)traitCollection {
  BOOL isLandscape = IsCompactHeight(traitCollection);
  return [self isLensAvailableForProfile] &&
         search::DefaultSearchProviderIsGoogle(_templateURLService) &&
         !isLandscape;
}

- (BOOL)isGeminiAvailable {
  return _BWGService->IsBwgAvailableForWebState(_webState);
}

- (BOOL)isReaderModeAvailable {
  if (!_readerModeTabHelper) {
    return NO;
  }
  return base::FeatureList::IsEnabled(
             kEnableReaderModePageEligibilityForToolsMenu)
             ? _readerModeTabHelper->CurrentPageIsDistillable()
             : _readerModeTabHelper->CurrentPageIsEligibleForReaderMode();
}

- (BOOL)isReaderModeActive {
  if (!_readerModeTabHelper) {
    return NO;
  }
  return _readerModeTabHelper->IsActive();
}

@end
