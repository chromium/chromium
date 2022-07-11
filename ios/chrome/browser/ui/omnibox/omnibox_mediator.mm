// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/search_engines_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxMediator () <SearchEngineObserving>

// Whether the current default search engine supports search-by-image.
@property(nonatomic, assign) BOOL searchEngineSupportsSearchByImage;

// The latest URL used to fetch the favicon.
@property(nonatomic, assign) GURL latestFaviconURL;

// The latest URL used to fetch the default search engine favicon.
@property(nonatomic, assign) const TemplateURL* latestDefaultSearchEngine;

// The favicon for the current default search engine. Cached to prevent
// needing to load it each time.
@property(nonatomic, strong) UIImage* currentDefaultSearchEngineFavicon;

@end

@implementation OmniboxMediator {
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _searchEngineSupportsSearchByImage = NO;
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<OmniboxConsumer>)consumer {
  _consumer = consumer;

  [self updateConsumerEmptyTextImage];
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(templateURLService);
  _searchEngineObserver =
      std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
}

- (void)setSearchEngineSupportsSearchByImage:
    (BOOL)searchEngineSupportsSearchByImage {
  BOOL supportChanged = self.searchEngineSupportsSearchByImage !=
                        searchEngineSupportsSearchByImage;
  _searchEngineSupportsSearchByImage = searchEngineSupportsSearchByImage;
  if (supportChanged) {
    [self.consumer
        updateSearchByImageSupported:searchEngineSupportsSearchByImage];
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(self.templateURLService);
  self.currentDefaultSearchEngineFavicon = nil;
  [self updateConsumerEmptyTextImage];
}

#pragma mark - PopupMatchPreviewDelegate

- (void)setPreviewSuggestion:(id<AutocompleteSuggestion>)suggestion
               isFirstUpdate:(BOOL)isFirstUpdate {
  // On first update, don't set the preview text, as omnibox will automatically
  // receive the suggestion as inline autocomplete through OmniboxViewIOS.
  if (!isFirstUpdate) {
    [self.consumer updateText:suggestion.omniboxPreviewText];
  }

  // When no suggestion is previewed, just show the default image.
  if (!suggestion) {
    [self setDefaultLeftImage];
    return;
  }

  // Set the suggestion image, or load it if necessary.
  [self.consumer updateAutocompleteIcon:suggestion.matchTypeIcon];
  __weak OmniboxMediator* weakSelf = self;
  if ([suggestion isMatchTypeSearch]) {
    // Show Default Search Engine favicon.
    [self loadDefaultSearchEngineFaviconWithCompletion:^(UIImage* image) {
      [weakSelf.consumer updateAutocompleteIcon:image];
    }];
  } else {
    // Show favicon.
    [self loadFaviconByPageURL:suggestion.destinationUrl.gurl
                    completion:^(UIImage* image) {
                      [weakSelf.consumer updateAutocompleteIcon:image];
                    }];
  }
}

- (void)setDefaultLeftImage {
  UIImage* image = GetOmniboxSuggestionIconForAutocompleteMatchType(
      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, /* is_starred */ false);
  [self.consumer updateAutocompleteIcon:image];

  __weak OmniboxMediator* weakSelf = self;
  // Show Default Search Engine favicon.
  [self loadDefaultSearchEngineFaviconWithCompletion:^(UIImage* image) {
    [weakSelf.consumer updateAutocompleteIcon:image];
  }];
}

// Loads a favicon for a given page URL.
// `pageURL` is url for the page that needs a favicon
// `completion` handler might be called multiple
// times, synchronously and asynchronously. It will always be called on the main
// thread.
- (void)loadFaviconByPageURL:(GURL)pageURL
                  completion:(void (^)(UIImage* image))completion {
  // Can't load favicons without a favicon loader.
  DCHECK(self.faviconLoader);

  // Remember which favicon is loaded in case we start loading a new one
  // before this one completes.
  self.latestFaviconURL = pageURL;
  __weak __typeof(self) weakSelf = self;
  auto handleFaviconResult = ^void(FaviconAttributes* faviconCacheResult) {
    if (weakSelf.latestFaviconURL != pageURL ||
        !faviconCacheResult.faviconImage ||
        faviconCacheResult.usesDefaultImage) {
      return;
    }
    if (completion) {
      completion(faviconCacheResult.faviconImage);
    }
  };

  // Download the favicon.
  // The code below mimics that in OmniboxPopupMediator.
  self.faviconLoader->FaviconForPageUrl(
      pageURL, kMinFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, handleFaviconResult);
}

// Loads a favicon for the current default search engine.
// `completion` handler might be called multiple times, synchronously
// and asynchronously. It will always be called on the main
// thread.
- (void)loadDefaultSearchEngineFaviconWithCompletion:
    (void (^)(UIImage* image))completion {
  // If default search engine image is currently loaded, just use it.
  if (self.currentDefaultSearchEngineFavicon) {
    if (completion) {
      completion(self.currentDefaultSearchEngineFavicon);
    }
  }

  const TemplateURL* defaultProvider =
      self.templateURLService
          ? self.templateURLService->GetDefaultSearchProvider()
          : nullptr;

  if (!defaultProvider) {
    // Service isn't available or default provider is disabled - either way we
    // can't get the icon.
    return;
  }

  // When the DSE is Google, use the bundled icon.
  if (defaultProvider && defaultProvider->GetEngineType(
                             self.templateURLService->search_terms_data()) ==
                             SEARCH_ENGINE_GOOGLE) {
    UIImage* bundledLogo = ios::provider::GetBrandedImage(
        ios::provider::BrandedImage::kOmniboxAnswer);

    if (bundledLogo) {
      self.currentDefaultSearchEngineFavicon = bundledLogo;
      if (completion) {
        completion(bundledLogo);
      }
      return;
    }
  }

  // Can't load favicons without a favicon loader.
  DCHECK(self.faviconLoader);

  __weak __typeof(self) weakSelf = self;
  self.latestDefaultSearchEngine = defaultProvider;
  auto handleFaviconResult = ^void(FaviconAttributes* faviconCacheResult) {
    DCHECK_LE(faviconCacheResult.faviconImage.size.width, kMinFaviconSizePt);
    if (weakSelf.latestDefaultSearchEngine != defaultProvider ||
        !faviconCacheResult.faviconImage ||
        faviconCacheResult.usesDefaultImage) {
      return;
    }
    UIImage* favicon = faviconCacheResult.faviconImage;
    weakSelf.currentDefaultSearchEngineFavicon = favicon;
    if (completion) {
      completion(favicon);
    }
  };

  // Prepopulated search engines don't have a favicon URL, so the favicon is
  // loaded with an empty query search page URL.
  if (defaultProvider->prepopulate_id() != 0) {
    // Fake up a page URL for favicons of prepopulated search engines, since
    // favicons may be fetched from Google server which doesn't suppoprt
    // icon URL.
    std::string emptyPageUrl = defaultProvider->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(std::u16string()),
        _templateURLService->search_terms_data());
    self.faviconLoader->FaviconForPageUrl(
        GURL(emptyPageUrl), kMinFaviconSizePt, kMinFaviconSizePt,
        /*fallback_to_google_server=*/YES, handleFaviconResult);
  } else {
    // Download the favicon.
    // The code below mimics that in OmniboxPopupMediator.
    self.faviconLoader->FaviconForIconUrl(defaultProvider->favicon_url(),
                                          kMinFaviconSizePt, kMinFaviconSizePt,
                                          handleFaviconResult);
  }
}

- (void)updateConsumerEmptyTextImage {
  [_consumer
      updateSearchByImageSupported:self.searchEngineSupportsSearchByImage];

  // Show Default Search Engine favicon.
  // Remember what is the Default Search Engine provider that the icon is
  // for, in case the user changes Default Search Engine while this is being
  // loaded.
  __weak __typeof(self) weakSelf = self;
  [self loadDefaultSearchEngineFaviconWithCompletion:^(UIImage* image) {
    [weakSelf.consumer setEmptyTextLeadingImage:image];
  }];
}

@end
