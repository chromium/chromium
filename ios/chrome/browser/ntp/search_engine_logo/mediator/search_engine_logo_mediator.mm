// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "build/branding_buildflags.h"
#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#import "components/search_provider_logos/logo_observer.h"
#import "ios/chrome/browser/google/model/google_logo_service.h"
#import "ios/chrome/browser/google/model/google_logo_service_factory.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_container_view.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
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
}  // namespace

#pragma mark - SearchEngineLogoMediator Private Interface

@interface SearchEngineLogoMediator () <SearchEngineLogoContainerViewDelegate>

// The container view used to display the Google logo or doodle.
@property(strong, nonatomic, readonly)
    SearchEngineLogoContainerView* containerView;

// Whether or not the doodle is being displayed.
@property(nonatomic, readwrite, assign, getter=isShowingDoodle)
    BOOL showingDoodle;

// Shows the doodle UIImageView with a fade animation.
- (void)updateLogo:(const search_provider_logos::Logo*)logo
           animate:(BOOL)animate;

@end

#pragma mark - LogoObserverIOS

class LogoObserverIOS : public search_provider_logos::LogoObserver {
 public:
  explicit LogoObserverIOS(SearchEngineLogoMediator* owner) : owner_(owner) {}
  ~LogoObserverIOS() override {}

  // search_provider_logos::LogoObserver implementation.
  void OnLogoAvailable(const search_provider_logos::Logo* logo,
                       bool from_cache) override {
    [owner_ updateLogo:logo animate:YES];
  }
  void OnObserverRemoved() override { delete this; }

 private:
  __weak SearchEngineLogoMediator* owner_;  // weak.
};

#pragma mark - SearchEngineLogoMediator Implementation

@implementation SearchEngineLogoMediator {
  raw_ptr<ProfileIOS> _profile;
  raw_ptr<web::WebState> _webState;
  raw_ptr<Browser> _browser;

  // Current logo fingerprint.
  std::string _fingerprint;

  // `YES` if the 'call to action' button been tapped and replaced with the
  // animated image.
  BOOL _ctaTapped;

  GURL _onClickUrl;
  GURL _animatedUrl;

  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> _imageFetcher;
}

@synthesize showingLogo = _showingLogo;
@synthesize containerView = _containerView;
@synthesize showingDoodle = _showingDoodle;
@synthesize doodleObserver = _doodleObserver;

- (instancetype)initWithBrowser:(Browser*)browser
                       webState:(web::WebState*)webState {
  DCHECK(browser);
  DCHECK(webState);
  if ((self = [super init])) {
    _browser = browser;
    _profile = _browser->GetProfile();
    _webState = webState;
    _showingLogo = YES;
  }
  return self;
}

#pragma mark - Accessors

- (SearchEngineLogoContainerView*)containerView {
  if (!_containerView) {
    // Create the container view and set its delegate.
    _containerView =
        [[SearchEngineLogoContainerView alloc] initWithFrame:CGRectZero];
    [_containerView setDelegate:self];

    // Set the accessibility label of the container to the alt text for the
    // logo.
    _containerView.isAccessibilityElement = YES;
    _containerView.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_NEW_TAB_LOGO_ACCESSIBILITY_LABEL);

    _containerView.shrunkLogoView =
        [[UIImageView alloc] initWithImage:[self logoImage]];
  }
  return _containerView;
}

#pragma mark - LogoVendor

- (UIView*)view {
  return self.containerView;
}

- (void)setShowingLogo:(BOOL)showingLogo {
  if (_showingLogo == showingLogo) {
    return;
  }
  _showingLogo = showingLogo;
  self.view.hidden = !_showingLogo;
}

- (void)fetchDoodle {
  GoogleLogoService* logoService =
      GoogleLogoServiceFactory::GetForProfile(_profile);
  const search_provider_logos::Logo logo = logoService->GetCachedLogo();
  if (!logo.image.empty()) {
    [self updateLogo:&logo animate:NO];
  }
  logoService->GetLogo(new LogoObserverIOS(self));
}

- (void)setShowingDoodle:(BOOL)showingDoodle {
  _showingDoodle = showingDoodle;
}

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
}

- (void)setUsesMonochromeLogo:(BOOL)usesMonochromeLogo {
  if (usesMonochromeLogo != _usesMonochromeLogo) {
    _usesMonochromeLogo = usesMonochromeLogo;
    if (self.containerView) {
      self.containerView.shrunkLogoView.image = [self logoImage];
    }
  }
}

- (void)disconnect {
  _profile = nullptr;
  _webState = nullptr;
  _browser = nullptr;
  _imageFetcher.reset();
}

#pragma mark - SearchEngineLogoContainerViewDelegate

- (void)searchEngineLogoContainerViewDoodleWasTapped:
    (SearchEngineLogoContainerView*)containerView {
  [self handleDoodleTapped];
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return nil;
}

#pragma mark - VisibleForTesting

- (void)simulateDoodleTapped {
  [self searchEngineLogoContainerViewDoodleWasTapped:self.containerView];
}

- (void)setClickURLText:(const GURL&)url {
  _onClickUrl = url;
}

#pragma mark - Private

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
    UrlLoadingBrowserAgent::FromBrowser(_browser)->Load(params);
    bool is_ntp = _webState && _webState->GetVisibleURL() == kChromeUINewTabURL;
    new_tab_page_uma::RecordNTPAction(_profile->IsOffTheRecord(), is_ntp,
                                      new_tab_page_uma::ACTION_OPENED_DOODLE);
    logoType = self.containerView.animatingDoodle ? CLICKED_LOGO_TYPE_ANIMATING
                                                  : CLICKED_LOGO_TYPE_STATIC;
  }
  DCHECK_NE(logoType, CLICKED_LOGO_TYPE_COUNT);
  UMA_HISTOGRAM_ENUMERATION(kUMANewTabPageLogoClick, logoType,
                            CLICKED_LOGO_TYPE_COUNT);
}

- (void)updateLogo:(const search_provider_logos::Logo*)logo
           animate:(BOOL)animate {
  GoogleLogoService* logoService =
      GoogleLogoServiceFactory::GetForProfile(_profile);
  if (!logo) {
    _fingerprint = "";
    [self.containerView setStyle:SEARCH_ENGINE_LOGO_CONTAINER_VIEW_STYLE_LOGO
                        animated:animate];
    self.containerView.isAccessibilityElement = YES;
    return;
  }

  // The -updateLogo call can be noisy. Don't reload the image if the
  // fingerprint hasn't changed.
  if (_fingerprint == logo->metadata.fingerprint) {
    return;
  }
  _fingerprint = logo->metadata.fingerprint;

  // Cache a valid, non null, logo for other window/tab uses.
  logoService->SetCachedLogo(logo);

  // If there is a doodle, remove the accessibility of the container view so the
  // doodle alt text can be read with voice over.
  self.containerView.isAccessibilityElement = NO;

  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  UIImage* doodle = skia::SkBitmapToUIImageWithColorSpace(
      logo->image, 1 /* scale */, color_space.get());

  // Animate this view seperately in case the doodle has updated multiple times.
  // This can happen when a particular doodle cycles thru multiple images.
  __weak __typeof(self) weakSelf = self;
  [self.containerView
      setDoodleImage:doodle
            animated:animate
          animations:^{
            weakSelf.showingDoodle = YES;
            [weakSelf.doodleObserver doodleDisplayStateChanged:YES];
          }];

  _onClickUrl = logo->metadata.on_click_url;

  if (!logo->metadata.animated_url.is_empty()) {
    _animatedUrl = logo->metadata.animated_url;
  }

  self.containerView.doodleAltText =
      base::SysUTF8ToNSString(logo->metadata.alt_text);

  // Report the UMA metric.
  UMA_HISTOGRAM_ENUMERATION(
      kUMANewTabPageLogoShown,
      _animatedUrl.is_valid() ? SHOWN_LOGO_TYPE_CTA : SHOWN_LOGO_TYPE_STATIC,
      SHOWN_LOGO_TYPE_COUNT);

  [self.containerView setStyle:SEARCH_ENGINE_LOGO_CONTAINER_VIEW_STYLE_DOODLE
                      animated:animate];
}

// Attempts to fetch an animated GIF for the doodle.
- (void)fetchAnimatedDoodle {
  if (_imageFetcher) {
    // Only attempt to fetch the doodle once per ntp.
    return;
  }
  _imageFetcher = std::make_unique<image_fetcher::IOSImageDataFetcherWrapper>(
      _profile->GetSharedURLLoaderFactory());
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

- (UIImage*)logoImage {
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
