// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/web/public/navigation/navigation_manager.h"

using base::UserMetricsAction;

@interface OmniboxMediator () <SearchEngineObserving>

// Is Browser incognito.
@property(nonatomic, assign, readonly) BOOL isIncognito;

// FET reference.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

// Whether the current default search engine supports search-by-image.
@property(nonatomic, assign) BOOL searchEngineSupportsSearchByImage;

// Whether the current default search engine supports Lens.
@property(nonatomic, assign) BOOL searchEngineSupportsLens;

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

  // Whether it's the lens overlay omnibox.
  BOOL _isLensOverlay;
}

- (instancetype)initWithIncognito:(BOOL)isIncognito
                          tracker:(feature_engagement::Tracker*)tracker
                    isLensOverlay:(BOOL)isLensOverlay {
  self = [super init];
  if (self) {
    _searchEngineSupportsSearchByImage = NO;
    _searchEngineSupportsLens = NO;
    _isIncognito = isIncognito;
    _tracker = tracker;
    _isLensOverlay = isLensOverlay;
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
  self.searchEngineSupportsLens =
      search_engines::SupportsSearchImageWithLens(templateURLService);
  if (_templateURLService) {
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
  } else {
    _searchEngineObserver.reset();
  }
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

- (void)setSearchEngineSupportsLens:(BOOL)searchEngineSupportsLens {
  BOOL supportChanged =
      self.searchEngineSupportsLens != searchEngineSupportsLens;
  _searchEngineSupportsLens = searchEngineSupportsLens;
  if (supportChanged) {
    [self.consumer updateLensImageSupported:searchEngineSupportsLens];
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  TemplateURLService* templateUrlService = self.templateURLService;
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(templateUrlService);
  self.searchEngineSupportsLens =
      search_engines::SupportsSearchImageWithLens(templateUrlService);
  self.currentDefaultSearchEngineFavicon = nil;
  [self updateConsumerEmptyTextImage];
}

#pragma mark - PopupMatchPreviewDelegate

- (void)setPreviewSuggestion:(id<AutocompleteSuggestion>)suggestion
               isFirstUpdate:(BOOL)isFirstUpdate {
  // On first update, don't set the preview text, as omnibox will automatically
  // receive the suggestion as inline autocomplete through OmniboxViewIOS.
  if (!isFirstUpdate) {
    // Remove additional text when previewing suggestions.
    if (IsRichAutocompletionEnabled()) {
      [self.consumer updateAdditionalText:nil];
    }
    [self.consumer updateText:suggestion.omniboxPreviewText];
  }

  // When no suggestion is previewed, just show the default image.
  if (!suggestion) {
    [self setDefaultLeftImage];
    return;
  }

  // Set the suggestion image, or load it if necessary.
  [self.consumer updateAutocompleteIcon:suggestion.matchTypeIcon
            withAccessibilityIdentifier:
                kOmniboxLeadingImageSuggestionImageAccessibilityIdentifier];

  __weak OmniboxMediator* weakSelf = self;
  if ([suggestion isMatchTypeSearch]) {
    // Show Default Search Engine favicon.
    [self loadDefaultSearchEngineFaviconWithCompletion:^(UIImage* image) {
      [weakSelf.consumer updateAutocompleteIcon:image
                    withAccessibilityIdentifier:
                        kOmniboxLeadingImageDefaultAccessibilityIdentifier];
    }];
  } else if (suggestion.destinationUrl.gurl.is_valid()) {
    // Show url favicon when it's valid.
    [self loadFaviconByPageURL:suggestion.destinationUrl.gurl
                    completion:^(UIImage* image) {
                      NSString* webPageUrl = base::SysUTF8ToNSString(
                          suggestion.destinationUrl.gurl.spec());
                      [weakSelf.consumer updateAutocompleteIcon:image
                                    withAccessibilityIdentifier:webPageUrl];
                    }];
  } else if (isFirstUpdate) {
    // When no suggestion is highlighted (aka. isFirstUpdate) show the default
    // browser icon.
    [self setDefaultLeftImage];
  } else {
    // When a suggestion is highlighted, show the same icon as in the popup.
    [self.consumer
             updateAutocompleteIcon:suggestion.matchTypeIcon
        withAccessibilityIdentifier:suggestion
                                        .matchTypeIconAccessibilityIdentifier];
  }
}

- (void)setDefaultLeftImage {
  UIImage* image = GetOmniboxSuggestionIconForAutocompleteMatchType(
      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  [self.consumer updateAutocompleteIcon:image
            withAccessibilityIdentifier:
                kOmniboxLeadingImageDefaultAccessibilityIdentifier];

  __weak OmniboxMediator* weakSelf = self;
  // Show Default Search Engine favicon.
  [self loadDefaultSearchEngineFaviconWithCompletion:^(UIImage* icon) {
    [weakSelf.consumer updateAutocompleteIcon:icon
                  withAccessibilityIdentifier:
                      kOmniboxLeadingImageDefaultAccessibilityIdentifier];
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
  DCHECK(pageURL.is_valid());
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
      pageURL, self.faviconSize, self.faviconSize,
      /*fallback_to_google_server=*/false, handleFaviconResult);
}

// Loads a favicon for the current default search engine.
// `completion` handler might be called multiple times, synchronously
// and asynchronously. It will always be called on the main
// thread.
- (void)loadDefaultSearchEngineFaviconWithCompletion:
    (void (^)(UIImage* image))completion {
  const CGFloat faviconSize = self.faviconSize;
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
    if (_isLensOverlay) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
      bundledLogo = MakeSymbolMulticolor(
          CustomSymbolWithPointSize(kGoogleIconSymbol, faviconSize));
#endif
    }
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
    DCHECK_LE(faviconCacheResult.faviconImage.size.width, faviconSize);
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
        GURL(emptyPageUrl), faviconSize, faviconSize,
        /*fallback_to_google_server=*/YES, handleFaviconResult);
  } else {
    // Download the favicon.
    // The code below mimics that in OmniboxPopupMediator.
    self.faviconLoader->FaviconForIconUrl(defaultProvider->favicon_url(),
                                          faviconSize, faviconSize,
                                          handleFaviconResult);
  }
}

- (void)updateConsumerEmptyTextImage {
  [_consumer
      updateSearchByImageSupported:self.searchEngineSupportsSearchByImage];
  [_consumer updateLensImageSupported:self.searchEngineSupportsLens];

  if (self.templateURLService) {
    if (const TemplateURL* searchProvider =
            self.templateURLService->GetDefaultSearchProvider()) {
      [self.consumer setSearchProviderName:searchProvider->short_name()];
    }
  }

  // Show Default Search Engine favicon.
  // Remember what is the Default Search Engine provider that the icon is
  // for, in case the user changes Default Search Engine while this is being
  // loaded.
  __weak __typeof(self) weakSelf = self;
  [self loadDefaultSearchEngineFaviconWithCompletion:^(UIImage* image) {
    [weakSelf.consumer setEmptyTextLeadingImage:image];
  }];
}

#pragma mark - OmniboxViewControllerPasteDelegate

- (void)didTapPasteToSearchButton:(NSArray<NSItemProvider*>*)itemProviders {
  __weak __typeof(self) weakSelf = self;
  auto textCompletion =
      ^(__kindof id<NSItemProviderReading> providedItem, NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          NSString* text = static_cast<NSString*>(providedItem);
          if (text) {
            [weakSelf.loadQueryCommandsHandler loadQuery:text immediately:YES];
            [weakSelf.omniboxCommandsHandler cancelOmniboxEdit];
          }
        });
      };
  auto imageSearchCompletion =
      ^(__kindof id<NSItemProviderReading> providedItem, NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          UIImage* image = static_cast<UIImage*>(providedItem);
          if (image) {
            [weakSelf loadImageQuery:image];
            [weakSelf.omniboxCommandsHandler cancelOmniboxEdit];
          }
        });
      };
  auto lensCompletion =
      ^(__kindof id<NSItemProviderReading> providedItem, NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          UIImage* image = base::apple::ObjCCast<UIImage>(providedItem);
          if (image) {
            [weakSelf lensImage:image];
          }
        });
      };
  for (NSItemProvider* itemProvider in itemProviders) {
    if ([itemProvider canLoadObjectOfClass:[UIImage class]]) {
      // Either provide a Lens option or a reverse-image-search option.
      if ([self shouldUseLens]) {
        RecordAction(
            UserMetricsAction("Mobile.OmniboxPasteButton.LensCopiedImage"));
        [itemProvider loadObjectOfClass:[UIImage class]
                      completionHandler:lensCompletion];
        break;
      } else if (self.searchEngineSupportsSearchByImage) {
        RecordAction(
            UserMetricsAction("Mobile.OmniboxPasteButton.SearchCopiedImage"));
        [itemProvider loadObjectOfClass:[UIImage class]
                      completionHandler:imageSearchCompletion];
        break;
      }
    } else if ([itemProvider canLoadObjectOfClass:[NSURL class]]) {
      RecordAction(
          UserMetricsAction("Mobile.OmniboxPasteButton.SearchCopiedLink"));
      default_browser::NotifyOmniboxURLCopyPasteAndNavigate(
          self.isIncognito, self.tracker, self.sceneState);
      // Load URL as a NSString to avoid further conversion.
      [itemProvider loadObjectOfClass:[NSString class]
                    completionHandler:textCompletion];
      break;
    } else if ([itemProvider canLoadObjectOfClass:[NSString class]]) {
      RecordAction(
          UserMetricsAction("Mobile.OmniboxPasteButton.SearchCopiedText"));
      default_browser::NotifyOmniboxTextCopyPasteAndNavigate(self.tracker);
      [itemProvider loadObjectOfClass:[NSString class]
                    completionHandler:textCompletion];
      break;
    }
  }
}

- (void)didTapVisitCopiedLink {
  default_browser::NotifyOmniboxURLCopyPasteAndNavigate(
      self.isIncognito, self.tracker, self.sceneState);
  __weak __typeof(self) weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentURLFromClipboard(
      base::BindOnce(^(std::optional<GURL> optionalURL) {
        if (!optionalURL) {
          return;
        }
        NSString* url = base::SysUTF8ToNSString(optionalURL.value().spec());
        dispatch_async(dispatch_get_main_queue(), ^{
          [weakSelf.loadQueryCommandsHandler loadQuery:url immediately:YES];
          [weakSelf.omniboxCommandsHandler cancelOmniboxEdit];
        });
      }));
}

- (void)didTapSearchCopiedText {
  default_browser::NotifyOmniboxTextCopyPasteAndNavigate(self.tracker);
  __weak __typeof(self) weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentTextFromClipboard(
      base::BindOnce(^(std::optional<std::u16string> optionalText) {
        if (!optionalText) {
          return;
        }
        NSString* query = base::SysUTF16ToNSString(optionalText.value());
        dispatch_async(dispatch_get_main_queue(), ^{
          [weakSelf.loadQueryCommandsHandler loadQuery:query immediately:YES];
          [weakSelf.omniboxCommandsHandler cancelOmniboxEdit];
        });
      }));
}

- (void)didTapSearchCopiedImage {
  __weak __typeof(self) weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentImageFromClipboard(
      base::BindOnce(^(std::optional<gfx::Image> optionalImage) {
        if (!optionalImage) {
          return;
        }
        UIImage* image = optionalImage.value().ToUIImage();
        [weakSelf loadImageQuery:image];
        [weakSelf.omniboxCommandsHandler cancelOmniboxEdit];
      }));
}

- (void)didTapLensCopiedImage {
  __weak __typeof(self) weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentImageFromClipboard(
      base::BindOnce(^(std::optional<gfx::Image> optionalImage) {
        if (!optionalImage) {
          return;
        }
        UIImage* image = optionalImage.value().ToUIImage();
        [weakSelf lensImage:image];
      }));
}

#pragma mark - Private methods

// Loads an image-search query with `image`.
- (void)loadImageQuery:(UIImage*)image {
  DCHECK(image);
  web::NavigationManager::WebLoadParams webParams =
      ImageSearchParamGenerator::LoadParamsForImage(image,
                                                    self.templateURLService);
  UrlLoadParams params = UrlLoadParams::InCurrentTab(webParams);
  self.URLLoadingBrowserAgent->Load(params);
}

// Performs a Lens search on the given `image`.
- (void)lensImage:(UIImage*)image {
  DCHECK(image);

  SearchImageWithLensCommand* command = [[SearchImageWithLensCommand alloc]
      initWithImage:image
         entryPoint:LensEntrypoint::OmniboxPostCapture];
  [self.lensCommandsHandler searchImageWithLens:command];
  [self.omniboxCommandsHandler cancelOmniboxEdit];
}

// Returns whether or not to use Lens for copied images.
- (BOOL)shouldUseLens {
  return ios::provider::IsLensSupported() &&
         base::FeatureList::IsEnabled(kEnableLensInOmniboxCopiedImage) &&
         self.searchEngineSupportsLens;
}

// Returns the size of the favicon.
- (CGFloat)faviconSize {
  if (_isLensOverlay) {
    return kDesiredSmallFaviconSizePt;
  } else {
    return kMinFaviconSizePt;
  }
}

@end
