// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "build/branding_buildflags.h"
#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search/search.h"
#import "ios/chrome/browser/google/model/google_logo_service.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_consumer.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_container_view.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/base/l10n/l10n_util.h"

#pragma mark - SearchEngineLogoMediator Private Interface

@interface SearchEngineLogoMediator () <SearchEngineLogoContainerViewDelegate,
                                        SearchEngineObserving>

// The container view used to display the Google logo or doodle.
@property(strong, nonatomic, readonly)
    SearchEngineLogoContainerView* containerView;

// The state of the current logo.
@property(assign, nonatomic) SearchEngineLogoState logoState;

// Called when the logo is downloaded or failed to be downloaded.
- (void)logoDownloaded:(const search_provider_logos::Logo*)logo
    searchEngineKeyword:(std::u16string)searchEngineKeyword
         callbackReason:
             (search_provider_logos::LogoCallbackReason)callbackReason;

@end

namespace {

// Key and enum for NewTabPage.LogoShown UMA histogram.
const char kUMANewTabPageLogoShown[] = "NewTabPage.LogoShown";
enum ShownLogoType {
  SHOWN_LOGO_TYPE_STATIC,
  SHOWN_LOGO_TYPE_CTA,
  SHOWN_LOGO_TYPE_COUNT
};

// Key and enum for NewTabPage.LogoClick UMA histogram.
const char kUMANewTabPageLogoClick[] = "NewTabPage.LogoClick";
enum ClickedLogoType {
  CLICKED_LOGO_TYPE_STATIC,
  CLICKED_LOGO_TYPE_CTA,
  CLICKED_LOGO_TYPE_ANIMATING,
  CLICKED_LOGO_TYPE_COUNT
};

const char kLogoShownGoogleDSE[] = "NewTabPage.LogoShowniOS.GoogleDSE";
// LINT.IfChange(NewTabPageLogoShowniOSGoogleDSEEnum)
enum class NewTabPageLogoShowniOSGoogleDSEEnum : int {
  // Embedded logo from Chrome app. This is always record before
  // kDownloadedLogo, kStaticImageDoodle or kCTADoodle.
  kEmbeddedLogo,
  // Logo downloaded. This not possible for Google search engine.
  kDownloadedLogo,
  // Doogle with a static image.
  kStaticImageDoodle,
  // Call to action doodle (animated doodle).
  kCTADoodle,
  kMaxValue = kCTADoodle,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NewTabPageLogoShowniOSGoogleDSEEnum)

const char kLogoShownThirdPartyDSE[] = "NewTabPage.LogoShowniOS.ThirdPartyDSE";
// LINT.IfChange(NewTabPageLogoShowniOSThirdPartyDSEEnum)
enum class NewTabPageLogoShowniOSThirdPartyDSEEnum : int {
  // No logo is displayed. This is always record before kDownloadedLogo,
  // kStaticImageDoodle or kCTADoodle.
  kNoLogo,
  // Logo downloaded. This not possible for Google search engine.
  kDownloadedLogo,
  // Doogle with a static image.
  kStaticImageDoodle,
  // Call to action doodle (animated doodle).
  kCTADoodle,
  kMaxValue = kCTADoodle,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NewTabPageLogoShowniOSThirdPartyDSEEnum)

// Records either Google DSE or 3rd party metric for a downloaded logo.
void RecordDownloadedLogoMetric(bool is_google_dse) {
  if (is_google_dse) {
    base::UmaHistogramEnumeration(
        kLogoShownGoogleDSE,
        NewTabPageLogoShowniOSGoogleDSEEnum::kDownloadedLogo);
  } else {
    base::UmaHistogramEnumeration(
        kLogoShownThirdPartyDSE,
        NewTabPageLogoShowniOSThirdPartyDSEEnum::kDownloadedLogo);
  }
}

// Records either Google DSE or 3rd party metric for a doodle (image or cta).
void RecordDoodleMetric(bool is_google_dse, bool is_cta_doodle) {
  if (is_google_dse) {
    NewTabPageLogoShowniOSGoogleDSEEnum value =
        is_cta_doodle ? NewTabPageLogoShowniOSGoogleDSEEnum::kCTADoodle
                      : NewTabPageLogoShowniOSGoogleDSEEnum::kStaticImageDoodle;
    base::UmaHistogramEnumeration(kLogoShownGoogleDSE, value);
  } else {
    NewTabPageLogoShowniOSThirdPartyDSEEnum value =
        is_cta_doodle
            ? NewTabPageLogoShowniOSThirdPartyDSEEnum::kCTADoodle
            : NewTabPageLogoShowniOSThirdPartyDSEEnum::kStaticImageDoodle;
    base::UmaHistogramEnumeration(kLogoShownThirdPartyDSE, value);
  }
}

// Called when logo has been fetched.
void OnLogoAvailable(SearchEngineLogoMediator* mediator,
                     std::u16string search_engine_keyword,
                     search_provider_logos::LogoCallbackReason callback_reason,
                     const std::optional<search_provider_logos::Logo>& logo) {
  [mediator logoDownloaded:(logo ? &logo.value() : nullptr)
       searchEngineKeyword:search_engine_keyword
            callbackReason:callback_reason];
}

}  // namespace

#pragma mark - SearchEngineLogoMediator Implementation

@implementation SearchEngineLogoMediator {
  raw_ptr<web::WebState> _webState;
  raw_ptr<TemplateURLService, DanglingUntriaged> _templateURLService;
  // Listen for default search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  // Default search provider. This can be null with some enterprise policy
  // settings.
  raw_ptr<const TemplateURL, DanglingUntriaged> _defaultSearchProvider;
  raw_ptr<GoogleLogoService> _logoService;
  raw_ptr<UrlLoadingBrowserAgent> _URLLoadingBrowserAgent;

  // Current logo fingerprint.
  std::string _fingerprint;

  // `YES` if the 'call to action' button been tapped and replaced with the
  // animated image.
  BOOL _ctaTapped;

  GURL _onClickUrl;
  GURL _animatedUrl;

  scoped_refptr<network::SharedURLLoaderFactory> _sharedURLLoaderFactory;
  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> _imageFetcher;
  BOOL _offTheRecord;
}

@synthesize containerView = _containerView;

- (instancetype)initWithWebState:(web::WebState*)webState
              templateURLService:(TemplateURLService*)templateURLService
                     logoService:(GoogleLogoService*)logoService
          URLLoadingBrowserAgent:(UrlLoadingBrowserAgent*)URLLoadingBrowserAgent
          sharedURLLoaderFactory:
              (scoped_refptr<network::SharedURLLoaderFactory>)
                  sharedURLLoaderFactory
                    offTheRecord:(BOOL)offTheRecord {
  DCHECK(webState);
  if ((self = [super init])) {
    _webState = webState;
    _templateURLService = templateURLService;
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _logoService = logoService;
    _URLLoadingBrowserAgent = URLLoadingBrowserAgent;
    _sharedURLLoaderFactory = sharedURLLoaderFactory;
    _offTheRecord = offTheRecord;
    [self searchEngineChanged];
  }
  return self;
}

- (void)disconnect {
  _webState = nullptr;
  _templateURLService = nullptr;
  _searchEngineObserver.reset();
  _defaultSearchProvider = nullptr;
  _logoService = nullptr;
  _URLLoadingBrowserAgent = nullptr;
  _sharedURLLoaderFactory = nullptr;
  _imageFetcher.reset();
}

- (UIView*)view {
  return self.containerView;
}

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
}

- (void)setUsesMonochromeLogo:(BOOL)usesMonochromeLogo {
  if (usesMonochromeLogo == _usesMonochromeLogo) {
    return;
  }
  _usesMonochromeLogo = usesMonochromeLogo;
  [self setContainerLogoIfAllowed];
}

#pragma mark - Accessors

- (SearchEngineLogoContainerView*)containerView {
  if (!_containerView) {
    // Create the container view and set its delegate.
    _containerView =
        [[SearchEngineLogoContainerView alloc] initWithFrame:CGRectZero];
    [_containerView setDelegate:self];
    if (!base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV3)) {
      // Those values are now automatically set when changing default search
      // engine.
      // Set the accessibility label of the container to the alt text for the
      // logo.
      _containerView.isAccessibilityElement = YES;
      _containerView.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_NEW_TAB_LOGO_ACCESSIBILITY_LABEL);
      _containerView.shrunkLogoView.image = [self offlineGoogleLogoImage];
    }
  }
  return _containerView;
}

- (void)setConsumer:(id<SearchEngineLogoConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  // The consumer should not be set after disconnect.
  CHECK(_templateURLService);
  [self searchEngineChanged];
}

- (void)setLogoState:(SearchEngineLogoState)logoState {
  if (logoState == _logoState) {
    return;
  }
  _logoState = logoState;
  [self setContainerLogoIfAllowed];
}

#pragma mark - SearchEngineLogoContainerViewDelegate

- (void)searchEngineLogoContainerViewDoodleWasTapped:
    (SearchEngineLogoContainerView*)containerView {
  [self handleDoodleTapped];
}

#pragma mark - VisibleForTesting

- (void)simulateDoodleTapped {
  [self searchEngineLogoContainerViewDoodleWasTapped:self.containerView];
}

- (void)setClickURLText:(const GURL&)url {
  _onClickUrl = url;
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  const TemplateURL* newDefaultSearchProvider =
      _templateURLService->GetDefaultSearchProvider();
  if (newDefaultSearchProvider == _defaultSearchProvider.get()) {
    // Nothing to do since it is the same default search provider.
    return;
  }
  _defaultSearchProvider = newDefaultSearchProvider;
  _logoService->SetCachedLogo(nullptr);
  self.containerView.doodleAltText = nil;
  if (search::DefaultSearchProviderIsGoogle(_templateURLService)) {
    self.logoState = SearchEngineLogoState::kLogo;
    // For legacy reason, the Google logo should be displayed with aspect fill.
    self.containerView.shrunkLogoView.contentMode =
        UIViewContentModeScaleAspectFill;
    base::UmaHistogramEnumeration(
        kLogoShownGoogleDSE,
        NewTabPageLogoShowniOSGoogleDSEEnum::kEmbeddedLogo);
  } else {
    self.logoState = SearchEngineLogoState::kNone;
    base::UmaHistogramEnumeration(
        kLogoShownThirdPartyDSE,
        NewTabPageLogoShowniOSThirdPartyDSEEnum::kNoLogo);
  }
  self.containerView.shrunkLogoView.image = [self offlineGoogleLogoImage];
  if (_defaultSearchProvider) {
    self.containerView.accessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_NEW_TAB_SEARCH_ENGINE_LOGO_ACCESSIBILITY_LABEL,
        _defaultSearchProvider->short_name());
  } else {
    self.containerView.accessibilityLabel = nil;
  }
  _fingerprint = "";
  [self.containerView setLogoState:self.logoState animated:YES];
  self.containerView.isAccessibilityElement = YES;
  if ([self canShowLogoOrDoodle]) {
    [self fetchLogoOrDoodle];
  }
}

#pragma mark - Private

// Sets the container view's logo to monochrome if state allows for it.
- (void)setContainerLogoIfAllowed {
  // Doodle supercedes monochrome logo.
  if (self.logoState == SearchEngineLogoState::kDoodle) {
    return;
  }

  // TODO(crbug.com/438460743): Need implementation.
  if (!search::DefaultSearchProviderIsGoogle(_templateURLService)) {
    return;
  }
  self.containerView.shrunkLogoView.image = [self offlineGoogleLogoImage];
}

// Returns whether a logo or doodle can be shown with the current search engine.
- (BOOL)canShowLogoOrDoodle {
  return search::DefaultSearchProviderIsGoogle(_templateURLService) ||
         (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV3) &&
          _defaultSearchProvider &&
          (_defaultSearchProvider->doodle_url().is_valid() ||
           _defaultSearchProvider->logo_url().is_valid()));
}

- (void)fetchLogoOrDoodle {
  if (!_defaultSearchProvider || !_logoService) {
    return;
  }
  const search_provider_logos::Logo logo = _logoService->GetCachedLogo();
  if (!logo.image.empty()) {
    [self updateLogo:&logo animate:NO];
  }
  search_provider_logos::LogoCallbacks callbacks;
  __weak __typeof(self) weakSelf = self;
  std::u16string searchEngineKeyword = _defaultSearchProvider->keyword();
  callbacks.on_cached_decoded_logo_available =
      base::BindOnce(&OnLogoAvailable, weakSelf, searchEngineKeyword);
  callbacks.on_fresh_decoded_logo_available =
      base::BindOnce(&OnLogoAvailable, weakSelf, searchEngineKeyword);
  _logoService->GetLogo(std::move(callbacks), false);
}

// Handler for taps on the doodle. Navigates the to the doodle's URL.
- (void)handleDoodleTapped {
  BOOL tapWillAnimate = _animatedUrl.is_valid() && _ctaTapped == NO;
  BOOL tapWillNavigate = _onClickUrl.is_valid();
  if (!tapWillAnimate && !tapWillNavigate) {
    return;
  }

  ClickedLogoType logoType = CLICKED_LOGO_TYPE_COUNT;
  if (tapWillAnimate) {
    [self fetchAnimatedDoodle];
    logoType = CLICKED_LOGO_TYPE_CTA;
  } else if (tapWillNavigate) {
    // It is necessary to include PAGE_TRANSITION_FROM_ADDRESS_BAR in the
    // transition type is so that query-in-the-omnibox is triggered for the
    // URL.
    UrlLoadParams params = UrlLoadParams::InCurrentTab(_onClickUrl);
    params.web_params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    _URLLoadingBrowserAgent->Load(params);
    bool is_ntp = _webState && _webState->GetVisibleURL() == kChromeUINewTabURL;
    new_tab_page_uma::RecordNTPAction(_offTheRecord, is_ntp,
                                      new_tab_page_uma::ACTION_OPENED_DOODLE);
    logoType = self.containerView.animatingDoodle ? CLICKED_LOGO_TYPE_ANIMATING
                                                  : CLICKED_LOGO_TYPE_STATIC;
  }
  DCHECK_NE(logoType, CLICKED_LOGO_TYPE_COUNT);
  UMA_HISTOGRAM_ENUMERATION(kUMANewTabPageLogoClick, logoType,
                            CLICKED_LOGO_TYPE_COUNT);
}

// Shows the doodle UIImageView with a fade animation.
- (void)updateLogo:(const search_provider_logos::Logo*)logo
           animate:(BOOL)animate {
  if (!logo) {
    _fingerprint = "";
    self.logoState = SearchEngineLogoState::kNone;
    [self.containerView setLogoState:self.logoState animated:animate];
    self.containerView.isAccessibilityElement = YES;
    return;
  }

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV3) &&
      logo->metadata.fingerprint != "") {
    // The -updateLogo call can be noisy. Don't reload the image if the
    // fingerprint hasn't changed.
    // TODO(crbug.com/436747442): fingerprint is empty for 3rd party search
    // engine logo.
    if (_fingerprint == logo->metadata.fingerprint) {
      return;
    }
    _fingerprint = logo->metadata.fingerprint;
  }

  // Cache a valid, non null, logo for other window/tab uses.
  _logoService->SetCachedLogo(logo);

  if (![self canShowLogoOrDoodle]) {
    // In case the logo state has been updated between the fetch and the
    // response.
    return;
  }

  // If there is a doodle, remove the accessibility of the container view so the
  // doodle alt text can be read with voice over.
  self.containerView.isAccessibilityElement = NO;

  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  UIImage* doodle = skia::SkBitmapToUIImageWithColorSpace(
      logo->image, 1 /* scale */, color_space.get());

  self.logoState = SearchEngineLogoState::kNone;
  switch (logo->metadata.type) {
    case search_provider_logos::LogoType::LOGO:
      self.logoState = SearchEngineLogoState::kLogo;
      break;
    case search_provider_logos::LogoType::SIMPLE:
    case search_provider_logos::LogoType::ANIMATED:
    case search_provider_logos::LogoType::INTERACTIVE:
      self.logoState = SearchEngineLogoState::kDoodle;
      break;
  }
  if (self.logoState == SearchEngineLogoState::kLogo &&
      base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV3)) {
    RecordDownloadedLogoMetric(
        search::DefaultSearchProviderIsGoogle(_templateURLService));
    // For 3rd party search engine, the logo needs to fit the image view.
    self.containerView.shrunkLogoView.contentMode =
        UIViewContentModeScaleAspectFit;
    self.containerView.isAccessibilityElement = YES;
    self.containerView.shrunkLogoView.image = doodle;
    [self.containerView setLogoState:self.logoState animated:animate];
    [self doodleAppearanceAnimationDidFinish:self.logoState];
    return;
  }

  // Animate this view seperately in case the doodle has updated multiple times.
  // This can happen when a particular doodle cycles thru multiple images.
  __weak __typeof(self) weakSelf = self;
  SearchEngineLogoState logoState = self.logoState;
  [self.containerView
      setDoodleImage:doodle
            animated:animate
          animations:^{
            [weakSelf doodleAppearanceAnimationDidFinish:logoState];
          }];

  _onClickUrl = logo->metadata.on_click_url;

  if (!logo->metadata.animated_url.is_empty()) {
    _animatedUrl = logo->metadata.animated_url;
  }

  self.containerView.doodleAltText =
      base::SysUTF8ToNSString(logo->metadata.alt_text);

  // Report the UMA metric.
  bool hasAnimatedURL = _animatedUrl.is_valid();
  UMA_HISTOGRAM_ENUMERATION(
      kUMANewTabPageLogoShown,
      hasAnimatedURL ? SHOWN_LOGO_TYPE_CTA : SHOWN_LOGO_TYPE_STATIC,
      SHOWN_LOGO_TYPE_COUNT);
  RecordDoodleMetric(search::DefaultSearchProviderIsGoogle(_templateURLService),
                     /*is_cta_doodle=*/hasAnimatedURL);

  [self.containerView setLogoState:self.logoState animated:animate];
}

- (void)logoDownloaded:(const search_provider_logos::Logo*)logo
    searchEngineKeyword:(std::u16string)searchEngineKeyword
         callbackReason:
             (search_provider_logos::LogoCallbackReason)callbackReason {
  if (!_logoService) {
    // The mediator was disconnected.
    return;
  }
  if (_defaultSearchProvider->keyword() != searchEngineKeyword) {
    // Ignore the logo/doodle fetch result, if it was triggered while the
    // defaut search engine was updated.
    return;
  }
  switch (callbackReason) {
    case search_provider_logos::LogoCallbackReason::DETERMINED:
      [self updateLogo:logo animate:YES];
      break;
    case search_provider_logos::LogoCallbackReason::CANCELED: {
      // The logo fetch was canceled. This can be for several reasons, for
      // example the search engine was changed, or the cookies were updated.
      // The fetch needs to be restarted, to make sure there is no mistake,
      // `[self searchEngineChanged]` is called.
      // TODO(crbug.com/439815392): This should be a temporary fix. The real
      // fix should be done in LogoServiceImpl.
      __weak __typeof(self) weakSelf = self;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(
                         [](__typeof(self) strongSelf) {
                           [strongSelf logoDownloadCanceled];
                         },
                         weakSelf));
      break;
    }
    case search_provider_logos::LogoCallbackReason::DISABLED:
    case search_provider_logos::LogoCallbackReason::REVALIDATED:
    case search_provider_logos::LogoCallbackReason::FAILED:
      break;
  }
}

// Called when the logo fetch was canceled.
- (void)logoDownloadCanceled {
  if (!_templateURLService) {
    // If the mediator was disconnected, this call should be ignored.
    return;
  }
  // Makes sure the logo is fetched again.
  [self fetchLogoOrDoodle];
}

// Called when the doodle's appearance animation completes.
- (void)doodleAppearanceAnimationDidFinish:(SearchEngineLogoState)logoState {
  self.logoState = logoState;
  self.view.hidden = (self.logoState == SearchEngineLogoState::kNone);
  [self.consumer searchEngineLogoStateDidChange:logoState];
}

// Attempts to fetch an animated GIF for the doodle.
- (void)fetchAnimatedDoodle {
  if (_imageFetcher) {
    // Only attempt to fetch the doodle once per ntp.
    return;
  }
  _imageFetcher = std::make_unique<image_fetcher::IOSImageDataFetcherWrapper>(
      _sharedURLLoaderFactory);
  __weak __typeof(self) weakSelf = self;
  image_fetcher::ImageDataFetcherBlock callback =
      base::CallbackToBlock(base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(
              ^(NSData* data, const image_fetcher::RequestMetadata&) {
                [weakSelf onFetchAnimatedDoodleCompleteWithData:data];
              })));
  _imageFetcher->FetchImageDataWebpDecoded(_animatedUrl, callback);
}

// Callback to receive the animated doodle information on the main thread.
- (void)onFetchAnimatedDoodleCompleteWithData:(NSData*)data {
  // Once a response has come back, even a failure response, mark the cta
  // as tapped. Any following taps will redirect to _onClickUrl.
  _ctaTapped = YES;

  if (data.length > 0) {
    UIImage* animatedImage = ios::provider::CreateAnimatedImageFromData(data);
    if (animatedImage) {
      [self.containerView setAnimatedDoodleImage:animatedImage animated:YES];
    }
  }
  _imageFetcher.reset();
}

// Returns the Google logo.
- (UIImage*)offlineGoogleLogoImage {
  if (!search::DefaultSearchProviderIsGoogle(_templateURLService)) {
    return nil;
  }
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImageSymbolConfiguration* config =
      self.usesMonochromeLogo
          ? [UIImageSymbolConfiguration configurationPreferringMonochrome]
          : [UIImageSymbolConfiguration configurationPreferringMulticolor];
  return [UIImage imageNamed:kGoogleSearchEngineLogoImage
                    inBundle:nil
           withConfiguration:config];
#else
  return nil;
#endif
}

@end
