// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/coordinator/omnibox_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/omnibox/coordinator/omnibox_mediator_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service.h"
#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service_observer_bridge.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/omnibox/public/omnibox_suggestion_icon_util.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/public/omnibox_util.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_consumer.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/web/public/navigation/navigation_manager.h"

using base::UserMetricsAction;

@interface OmniboxMediator () <SearchEngineObserving,
                               PlaceholderServiceObserving>

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

@end

@implementation OmniboxMediator {
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  std::unique_ptr<PlaceholderServiceObserverBridge> _placeholderServiceObserver;

  // The context in which the omnibox is presented.
  OmniboxPresentationContext _presentationContext;
}

- (instancetype)initWithIncognito:(BOOL)isIncognito
                          tracker:(feature_engagement::Tracker*)tracker
              presentationContext:
                  (OmniboxPresentationContext)presentationContext {
  self = [super init];
  if (self) {
    _searchEngineSupportsSearchByImage = NO;
    _searchEngineSupportsLens = NO;
    _isIncognito = isIncognito;
    _tracker = tracker;
    _presentationContext = presentationContext;
  }
  return self;
}

- (void)setThumbnailImage:(UIImage*)image {
  [self.consumer setThumbnailImage:image];
  [self.omniboxTextController onThumbnailSet:image != nil];
}

#pragma mark - Setters

- (void)setConsumer:(id<OmniboxConsumer>)consumer {
  _consumer = consumer;

  // Initializes consumer values.
  [self placeholderTextUpdated];
  [self placeholderImageUpdated];
  [self searchEngineChanged];
  // Forces the layout of the leading image.
  [self setDefaultLeftImage];
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

- (void)setPlaceholderService:(PlaceholderService*)placeholderService {
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
  [self.consumer
      updateSearchByImageSupported:self.searchEngineSupportsSearchByImage];
  [self.consumer updateLensImageSupported:self.searchEngineSupportsLens];
}

#pragma mark - PlaceholderServiceObserving

- (void)placeholderTextUpdated {
  if (self.placeholderService) {
    [self.consumer setPlaceholderText:self.placeholderService
                                          ->GetCurrentPlaceholderText()];
    [self.consumer
        setSearchOnlyPlaceholderText:
            self.placeholderService->GetCurrentSearchOnlyPlaceholderText()];
  }
}

- (void)placeholderImageUpdated {
  // Show Default Search Engine favicon.
  // Remember what is the Default Search Engine provider that the icon is
  // for, in case the user changes Default Search Engine while this is being
  // loaded.
  __weak __typeof(self) weakSelf = self;
  [self loadDefaultSearchEngineFaviconWithCompletion:^(UIImage* image) {
    [weakSelf.consumer setEmptyTextLeadingImage:image];
  }];
}

- (void)placeholderServiceShuttingDown:(PlaceholderService*)service {
  // Removes observation.
  self.placeholderService = nil;
}

#pragma mark - OmniboxMutator

- (void)removeThumbnail {
  base::RecordAction(UserMetricsAction("Mobile.OmniboxThumbnail.Deleted"));
  // Update the UI.
  [self.consumer setThumbnailImage:nil];
  [self.omniboxTextController onUserRemoveThumbnail];
}

- (void)removeAdditionalText {
  [self.omniboxTextController onUserRemoveAdditionalText];
}

- (void)clearText {
  [self.omniboxTextController clearText];
}

- (void)acceptInput {
  [self.omniboxTextController acceptInput];
}

- (void)prepareForScribble {
  [self.omniboxTextController prepareForScribble];
}

- (void)cleanupAfterScribble {
  [self.omniboxTextController cleanupAfterScribble];
}

- (void)onTextInputModeChange {
  [self.omniboxTextController onTextInputModeChange];
}

#pragma mark Textfield delegate forwaring

- (void)onDidBeginEditing {
  [self.omniboxTextController onDidBeginEditing];
  [self.delegate omniboxMediatorDidBeginEditing:self];
}

- (BOOL)shouldChangeCharactersInRange:(NSRange)range
                    replacementString:(NSString*)newText {
  return [self.omniboxTextController shouldChangeCharactersInRange:range
                                                 replacementString:newText];
}

- (void)textDidChangeWithUserEvent:(BOOL)isProcessingUserEvent {
  [self.omniboxTextController textDidChangeWithUserEvent:isProcessingUserEvent];
}

- (void)onAcceptAutocomplete {
  [self.omniboxTextController onAcceptAutocomplete];
}

- (void)onCopy {
  [self.omniboxTextController onCopy];
}

- (void)willPaste {
  [self.omniboxTextController willPaste];
}

- (void)onDeleteBackward {
  [self.omniboxTextController onDeleteBackward];
}

#pragma mark ContextMenu event

- (void)pasteToSearch:(NSArray<NSItemProvider*>*)itemProviders {
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

- (void)visitCopiedLink {
  default_browser::NotifyOmniboxURLCopyPasteAndNavigate(
      self.isIncognito, self.tracker, self.sceneState);
  __weak __typeof(self) weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentURLFromClipboard(
      base::BindOnce(^(std::optional<GURL> optionalURL) {
        if (!optionalURL) {
          return;
        }
        NSString* url = [NSString cr_fromString:optionalURL.value().spec()];
        dispatch_async(dispatch_get_main_queue(), ^{
          [weakSelf.loadQueryCommandsHandler loadQuery:url immediately:YES];
          [weakSelf.omniboxCommandsHandler cancelOmniboxEdit];
        });
      }));
}

- (void)searchCopiedText {
  default_browser::NotifyOmniboxTextCopyPasteAndNavigate(self.tracker);
  __weak __typeof(self) weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentTextFromClipboard(
      base::BindOnce(^(std::optional<std::u16string> optionalText) {
        if (!optionalText) {
          return;
        }
        NSString* query = [NSString cr_fromString16:optionalText.value()];
        dispatch_async(dispatch_get_main_queue(), ^{
          [weakSelf.loadQueryCommandsHandler loadQuery:query immediately:YES];
          [weakSelf.omniboxCommandsHandler cancelOmniboxEdit];
        });
      }));
}

- (void)searchCopiedImage {
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

- (BOOL)shouldUseLensForCopiedImage {
  return ios::provider::IsLensSupported() &&
         base::FeatureList::IsEnabled(kEnableLensInOmniboxCopiedImage);
}

- (void)lensCopiedImage {
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

#pragma mark - OmniboxTextControllerDelegate

- (void)omniboxTextController:(OmniboxTextController*)omniboxTextController
         didPreviewSuggestion:(id<AutocompleteSuggestion>)suggestion
                isFirstUpdate:(BOOL)isFirstUpdate {
  // Updates the return key availability in case popup highlight changed.
  [self.consumer updateReturnKeyAvailability];

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
                      NSString* webPageUrl = [NSString
                          cr_fromString:suggestion.destinationUrl.gurl.spec()];
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
  auto handleFaviconResult =
      ^void(FaviconAttributes* faviconCacheResult, bool cached) {
        if (weakSelf.latestFaviconURL != pageURL ||
            !faviconCacheResult.faviconImage) {
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
  if (self.placeholderService) {
    self.placeholderService->FetchDefaultSearchEngineIcon(
        self.faviconSize, base::BindRepeating(completion));
  }
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
  if (_presentationContext == OmniboxPresentationContext::kLensOverlay ||
      _presentationContext == OmniboxPresentationContext::kComposebox) {
    return kDesiredSmallFaviconSizePt;
  } else {
    return kMinFaviconSizePt;
  }
}

@end
