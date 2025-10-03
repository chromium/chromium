// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_mediator.h"

#import "base/memory/ptr_util.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/google/core/common/google_util.h"
#import "components/lens/lens_url_utils.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_consumer.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service.h"
#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service_observer_bridge.h"
#import "ios/chrome/browser/omnibox/public/omnibox_util.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "skia/ext/skia_utils_ios.h"

namespace {

// The point size of the entry point's symbol.
const CGFloat kIconPointSize = 16.0;

}  // namespace

@interface LocationBarMediator () <SearchEngineObserving,
                                   WebStateListObserving,
                                   PlaceholderServiceObserving>

// Whether the current default search engine supports search by image.
@property(nonatomic, assign) BOOL searchEngineSupportsSearchByImage;

// Whether the current default search engine supports Lens.
@property(nonatomic, assign) BOOL searchEngineSupportsLens;

@end

@implementation LocationBarMediator {
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<PlaceholderServiceObserverBridge> _placeholderServiceObserver;
  BOOL _isIncognito;
}

- (instancetype)initWithIsIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _searchEngineSupportsSearchByImage = NO;
    _searchEngineSupportsLens = NO;
    _isIncognito = isIncognito;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateList = nullptr;
  }
  _webStateListObserver = nullptr;
  _searchEngineObserver = nullptr;
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdate) ||
      base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)) {
    self.placeholderService = nullptr;
  }
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(self.templateURLService);
  self.searchEngineSupportsLens =
      search_engines::SupportsSearchImageWithLens(self.templateURLService);
  const TemplateURL* defaultSearchProvider =
      self.templateURLService->GetDefaultSearchProvider();
  NSString* providerName =
      defaultSearchProvider
          ? [NSString
                cr_fromString16:defaultSearchProvider
                                    ->AdjustedShortNameForLocaleDirection()]
          : @"";
  [self.consumer setPlaceholderText:providerName];
}

#pragma mark - Setters

- (void)setConsumer:(id<LocationBarConsumer>)consumer {
  _consumer = consumer;
  [consumer setSearchByImageEnabled:self.searchEngineSupportsSearchByImage];
  [consumer setLensImageEnabled:self.searchEngineSupportsLens];
  [self updatePlaceholderType];
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  if (templateURLService) {
    self.searchEngineSupportsSearchByImage =
        search_engines::SupportsSearchByImage(templateURLService);
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
  } else {
    self.searchEngineSupportsSearchByImage = NO;
    _searchEngineObserver.reset();
  }
}

- (void)setPlaceholderService:(PlaceholderService*)placeholderService {
  CHECK((base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdate) ||
         base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)));
  _placeholderService = placeholderService;

  if (!placeholderService) {
    _placeholderServiceObserver.reset();
    return;
  }

  _placeholderServiceObserver =
      std::make_unique<PlaceholderServiceObserverBridge>(self,
                                                         placeholderService);
}

- (void)setSearchEngineSupportsSearchByImage:
    (BOOL)searchEngineSupportsSearchByImage {
  BOOL supportChanged =
      _searchEngineSupportsSearchByImage != searchEngineSupportsSearchByImage;
  _searchEngineSupportsSearchByImage = searchEngineSupportsSearchByImage;
  if (supportChanged) {
    [self.consumer setSearchByImageEnabled:searchEngineSupportsSearchByImage];
  }
}

- (void)setSearchEngineSupportsLens:(BOOL)searchEngineSupportsLens {
  BOOL supportChanged = _searchEngineSupportsLens != searchEngineSupportsLens;
  _searchEngineSupportsLens = searchEngineSupportsLens;
  if (supportChanged) {
    [self.consumer setLensImageEnabled:searchEngineSupportsLens];
    [self updatePlaceholderType];
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    _webStateList->AddObserver(_webStateListObserver.get());
  }
}

- (void)locationUpdated {
  [self updatePlaceholderType];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    [self.consumer defocusOmnibox];
  }
}

#pragma mark - PlaceholderServiceObserving

- (void)placeholderImageUpdated {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  if (self.placeholderService) {
    self.placeholderService->FetchDefaultSearchEngineIcon(
        kIconPointSize, base::BindRepeating(^(UIImage* image) {
          [weakSelf.consumer setPlaceholderDefaultSearchEngineIcon:image];
        }));
  }
}

#pragma mark - Private

/// Returns whether the Lens overlay is currently available for the web state.
- (BOOL)isLensOverlayAvailable {
  if (IsPageActionMenuEnabled() && IsDirectBWGEntryPoint()) {
    return NO;
  }

  if (_webStateList) {
    web::WebState* webState = _webStateList->GetActiveWebState();
    if (webState) {
      ProfileIOS* profile =
          ProfileIOS::FromBrowserState(webState->GetBrowserState());
      return IsLensOverlayAvailable(profile->GetPrefs());
    }
  }
  return NO;
}

/// Returns whether the AI Hub is currently available for the web state.
- (BOOL)isAIHubAvailable {
  if (!IsPageActionMenuEnabled()) {
    return NO;
  }
  if (_webStateList) {
    web::WebState* webState = _webStateList->GetActiveWebState();
    if (webState) {
      ProfileIOS* profile =
          ProfileIOS::FromBrowserState(webState->GetBrowserState());
      BwgService* BWGService = BwgServiceFactory::GetForProfile(profile);
      if (BWGService) {
        if (IsDirectBWGEntryPoint()) {
          return BWGService->IsBwgAvailableForWebState(webState);
        }

        return BWGService->IsProfileEligibleForBwg();
      }
    }
  }
  return NO;
}

/// Updates the placeholder.
- (void)updatePlaceholderType {
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2) &&
      [self isCurrentPageNTP]) {
    [self.consumer setPlaceholderType:LocationBarPlaceholderType::
                                          kDefaultSearchEngineIcon];
    return;
  } else {
    [self.consumer setPlaceholderType:LocationBarPlaceholderType::kNone];
    // No early return here; allow Lens Overlay to override the placeholder if
    // necessary.
  }

  if ([self isAIHubAvailable]) {
    // If this is the user's first time being eligible for the AI Hub, notify
    // the FET.
    if (!_webStateList || !_webStateList->GetActiveWebState()) {
      return;
    }
    web::WebState* webState = _webStateList->GetActiveWebState();
    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(webState->GetBrowserState());
    PrefService* prefs = profile->GetPrefs();
    if (!prefs->GetBoolean(prefs::kAIHubEligibilityTriggered)) {
      prefs->SetBoolean(prefs::kAIHubEligibilityTriggered, true);
      feature_engagement::TrackerFactory::GetForProfile(profile)->NotifyEvent(
          feature_engagement::events::kIOSGeminiEligiblity);
    }

    // Record Gemini entry point impression when AI Hub is available and shown.
    RecordGeminiEntryPointImpression();
    [self.consumer
        setPlaceholderType:LocationBarPlaceholderType::kPageActionMenu];
    return;
  }
  if (![self isLensOverlayAvailable]) {
    return;
  }
  if ([self isLensOverlayEntrypointAvailable]) {
    [self.consumer setPlaceholderType:LocationBarPlaceholderType::kLensOverlay];
  } else {
    [self.consumer setPlaceholderType:LocationBarPlaceholderType::kNone];
  }
}

/// Whether the lens overlay entrypoint should be available.
- (BOOL)isLensOverlayEntrypointAvailable {
  if (![self isLensOverlayAvailable] ||
      _isIncognito ||
      !search_engines::SupportsSearchImageWithLens(self.templateURLService)) {
    return NO;
  }
  GURL visibleURL = GURL();
  if (_webStateList) {
    web::WebState* webState = _webStateList->GetActiveWebState();
    if (webState) {
      visibleURL = webState->GetVisibleURL();
    }
  }

  if (google_util::IsGoogleSearchUrl(visibleURL) ||
      google_util::IsGoogleHomePageUrl(visibleURL)) {
    return NO;
  }

  return !IsURLNewTabPage(visibleURL) && !lens::IsLensMWebResult(visibleURL);
}

- (BOOL)isCurrentPageNTP {
  GURL visibleURL = GURL();
  if (_webStateList) {
    web::WebState* webState = _webStateList->GetActiveWebState();
    if (webState) {
      visibleURL = webState->GetVisibleURL();
    }
  }

  return IsURLNewTabPage(visibleURL);
}

@end
