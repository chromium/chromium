// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_magic_stack_mediator.h"

#import <vector>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/power_bookmarks/core/power_bookmark_utils.h"
#import "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_magic_stack_mediator_delegate.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/ntp_theme_actionable_tip_config.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_audience.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_config.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_customization_seed_colors.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/theme_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "url/gurl.h"
#import "url/url_util.h"

namespace {
// Index of the Violet seed color in `kSeedColors`.
constexpr size_t kVioletSeedColorIndex = 8;
static_assert(kVioletSeedColorIndex < kSeedColors.size());

// Number of preset background themes displayed in the actionable NTP Theme tip.
constexpr size_t kPresetThemeCount = 2;

// Holds a preset custom background proto and its downloaded thumbnail image.
struct NtpCustomBackgroundWithImage {
  sync_pb::NtpCustomBackground proto;
  UIImage* image = [[UIImage alloc] init];
};

}  // namespace

using segmentation_platform::TipIdentifier;

@interface TipsMagicStackMediator () <PrefObserverDelegate, TipsModuleAudience>
@end

@implementation TipsMagicStackMediator {
  // The profile Pref service.
  raw_ptr<PrefService> _profilePrefService;

  // Registrar for user Pref changes notifications.
  PrefChangeRegistrar _profilePrefChangeRegistrar;

  // Bridge to listen to Pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;

  // The shopping service used to fetch product images for shopping tips.
  raw_ptr<commerce::ShoppingService> _shoppingService;

  // The image fetcher used to download product images.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;

  // The bookmark model used to check if a shoping page is bookmarked.
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;

  // The home customization service used to update the background and store the
  // selected theme.
  raw_ptr<HomeBackgroundCustomizationService> _backgroundCustomizationService;

  // The preset background themes and their thumbnail images displayed in the
  // actionable NTP theme tip.
  std::vector<NtpCustomBackgroundWithImage> _presetBackgrounds;

  // The current actionable module configuration when an actionable tip is
  // active.
  NTPThemeActionableTipConfig* _actionableConfig;
}

- (instancetype)initWithIdentifier:(TipIdentifier)identifier
                profilePrefService:(PrefService*)profilePrefService
                   shoppingService:(commerce::ShoppingService*)shoppingService
                     bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                      imageFetcher:
                          (std::unique_ptr<image_fetcher::ImageDataFetcher>)
                              imageFetcher
    backgroundCustomizationService:
        (HomeBackgroundCustomizationService*)backgroundCustomizationService {
  self = [super init];

  if (self) {
    CHECK(profilePrefService);
    CHECK(shoppingService);
    CHECK(bookmarkModel);
    CHECK(imageFetcher);

    _profilePrefService = profilePrefService;
    _config = [[TipsModuleConfig alloc] initWithTipIdentifier:identifier];
    _config.audience = self;
    _shoppingService = shoppingService;
    _bookmarkModel = bookmarkModel;
    _imageFetcher = std::move(imageFetcher);
    _backgroundCustomizationService = backgroundCustomizationService;

    if (!_prefObserverBridge) {
      _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);

      _profilePrefChangeRegistrar.Init(profilePrefService);

      _prefObserverBridge->ObserveChangesForPreference(
          ntp_tiles::prefs::kTipsHomeModuleEnabled,
          &_profilePrefChangeRegistrar);
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kIosRecentlyUsedBackgrounds, &_profilePrefChangeRegistrar);
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kIosUserUploadedBackground, &_profilePrefChangeRegistrar);
    }
  }

  return self;
}

- (void)disconnect {
  _shoppingService = nil;
  _imageFetcher = nullptr;
  _bookmarkModel = nullptr;
  _backgroundCustomizationService = nullptr;

  _profilePrefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _profilePrefService = nil;
}

- (void)reconfigureWithTipIdentifier:(TipIdentifier)identifier {
  // Always update `_config` so `_config.identifier` reflects the current tip
  // for impression and interaction logging in `ContentSuggestionsCoordinator`,
  // even when an actionable config is displayed.
  _config = [[TipsModuleConfig alloc] initWithTipIdentifier:identifier];
  _config.audience = self;

  // Reset `_actionableConfig` so `activeConfig` falls back to `_config` when
  // reconfigured with an informational tip.
  _actionableConfig = nil;

  if (identifier == TipIdentifier::kNTPTheme &&
      segmentation_platform::features::
          IsMagicStackNTPThemeTipIosActionableEnabled()) {
    _actionableConfig = [[NTPThemeActionableTipConfig alloc] init];
    _actionableConfig.audience = self;
    _actionableConfig.colorPalettes = [self buildNTPThemeColorPalettes];
    _actionableConfig.selectedIndex = 0;
    [self fetchPresetBackgroundImages];
  }

  if (_config.identifier == TipIdentifier::kLensShop) {
    [self fetchImage];
  }
}

- (void)removeModuleWithCompletion:(ProceduralBlock)completion {
  [self.delegate removeTipsModuleWithCompletion:completion];
}

- (MagicStackModule*)activeConfig {
  return _actionableConfig ? _actionableConfig : _config;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  CHECK(_profilePrefService);
  if (preferenceName == ntp_tiles::prefs::kTipsHomeModuleEnabled) {
    if (!_profilePrefService->GetBoolean(
            ntp_tiles::prefs::kTipsHomeModuleEnabled)) {
      [self.delegate removeTipsModuleWithCompletion:nil];
    }
  } else if (preferenceName == prefs::kIosRecentlyUsedBackgrounds) {
    if (_actionableConfig && _presetBackgrounds.empty()) {
      [self fetchPresetBackgroundImages];
    }
  } else if (preferenceName == prefs::kIosUserUploadedBackground) {
    if (_actionableConfig &&
        !_profilePrefService->GetDict(prefs::kIosUserUploadedBackground)
             .empty()) {
      __weak TipsMagicStackMediator* weakSelf = self;
      [self.delegate removeTipsModuleWithCompletion:^{
        [weakSelf.presentationAudience didSaveNTPThemeBackgroundPhoto];
      }];
    }
  }
}

#pragma mark - TipsModuleAudience

- (void)didSelectTip:(TipIdentifier)tip {
  // Log engagement before forwarding the selection, as handling the tip
  // selection removes the module from the Magic Stack.
  [self.delegate logMagicStackEngagementForType:self.activeConfig.type];
  [self.presentationAudience didSelectTip:tip];
}

- (void)didSelectNTPThemePhotoPicker {
  // Log engagement before forwarding the selection to the presentation
  // audience.
  [self.delegate logMagicStackEngagementForType:self.activeConfig.type];
  [self.presentationAudience didSelectNTPThemePhotoPicker];
}

- (void)didSelectNTPThemeAtIndex:(NSInteger)index {
  if (!_backgroundCustomizationService) {
    return;
  }
  _actionableConfig.selectedIndex = index;

  if (index == 0) {
    _backgroundCustomizationService->ClearCurrentBackground();
  } else if (index == 1) {
    const SeedColor& seedColor = kSeedColors[kVioletSeedColorIndex];
    _backgroundCustomizationService->SetBackgroundColor(
        seedColor.color, SchemeVariantToProtoEnum(seedColor.variant));
  } else if (index >= 2 &&
             static_cast<size_t>(index - 2) < _presetBackgrounds.size()) {
    const sync_pb::NtpCustomBackground& background =
        _presetBackgrounds[index - 2].proto;
    GURL thumbnailURL =
        AddOptionsToImageURL(RemoveOptionsFromImageURL(background.url()).spec(),
                             GetThumbnailImageOptions());
    _backgroundCustomizationService->SetCurrentBackground(
        GURL(background.url()), thumbnailURL, background.attribution_line_1(),
        background.attribution_line_2(),
        GURL(background.attribution_action_url()), background.collection_id());
  }
  _backgroundCustomizationService->StoreCurrentTheme();
  [self.delegate logMagicStackEngagementForType:self.activeConfig.type];
  [self.presentationAudience didSelectNTPThemeOption];
}

- (void)didToggleTipsNotificationsSwitch:(BOOL)enabled {
  [self.delegate logMagicStackEngagementForType:self.activeConfig.type];
  [self.presentationAudience didToggleTipsNotificationsSwitch:enabled];
}

#pragma mark - Private

// Fetches the product image for the most recently subscribed product from
// the shopping service and stores it in `_config.productImageData`.
- (void)fetchImage {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      _shoppingService->GetAllShoppingBookmarks();

  GURL mostRecentSubscriptionProductURL =
      [self getMostRecentSubscriptionProductURL:nodes];

  __weak TipsMagicStackMediator* weakSelf = self;

  _imageFetcher->FetchImageData(
      mostRecentSubscriptionProductURL,
      base::BindOnce(^(const std::string& imageData,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf onImageFetchedResult:imageData];
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

// Returns the URL of the lead image for the most recently subscribed product
// in the given list of `subscriptions`. If no subscriptions have a lead
// image, returns an empty GURL.
- (GURL)getMostRecentSubscriptionProductURL:
    (std::vector<const bookmarks::BookmarkNode*>)subscriptions {
  GURL mostRecentSubscriptionProductURL;
  int64_t mostRecentSubscriptionTime = 0;

  for (const bookmarks::BookmarkNode* bookmark : subscriptions) {
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(_bookmarkModel, bookmark);

    if (!meta || !meta->has_shopping_specifics()) {
      continue;
    }

    const power_bookmarks::ShoppingSpecifics specifics =
        meta->shopping_specifics();

    if (mostRecentSubscriptionProductURL.is_empty() ||
        mostRecentSubscriptionTime <
            specifics.last_subscription_change_time()) {
      mostRecentSubscriptionProductURL = GURL(meta->lead_image().url());
      mostRecentSubscriptionTime = specifics.last_subscription_change_time();
    }
  }

  return mostRecentSubscriptionProductURL;
}

// Called when the image data has been fetched. Updates the
// `_config.productImageData` with the fetched `imageData` and notifies the
// delegate of the state change.
- (void)onImageFetchedResult:(const std::string&)imageData {
  NSData* data = [NSData dataWithBytes:imageData.data()
                                length:imageData.size()];

  if (data.length == 0) {
    return;
  }

  _config.productImageData = data;
  [self.delegate tipsMagicStackMediatorDidReconfigureItem];
}

// Loads up to `kPresetThemeCount` preset backgrounds and fetches their
// thumbnail images.
- (void)fetchPresetBackgroundImages {
  if (!_backgroundCustomizationService || !_imageFetcher) {
    return;
  }
  _presetBackgrounds.clear();
  for (const RecentlyUsedBackground& recent :
       _backgroundCustomizationService->GetRecentlyUsedBackgrounds()) {
    if (const HomeCustomBackground* customBg =
            std::get_if<HomeCustomBackground>(&recent)) {
      if (const sync_pb::NtpCustomBackground* ntpBg =
              std::get_if<sync_pb::NtpCustomBackground>(customBg)) {
        _presetBackgrounds.push_back(
            NtpCustomBackgroundWithImage{.proto = *ntpBg});
        if (_presetBackgrounds.size() == kPresetThemeCount) {
          break;
        }
      }
    }
  }

  __weak TipsMagicStackMediator* weakSelf = self;
  for (size_t i = 0; i < _presetBackgrounds.size(); ++i) {
    GURL thumbnailURL = AddOptionsToImageURL(
        RemoveOptionsFromImageURL(_presetBackgrounds[i].proto.url()).spec(),
        GetThumbnailImageOptions());
    _imageFetcher->FetchImageData(
        thumbnailURL,
        base::BindOnce(^(const std::string& imageData,
                         const image_fetcher::RequestMetadata& metadata) {
          [weakSelf onPresetBackgroundImageFetched:imageData atIndex:i];
        }),
        NO_TRAFFIC_ANNOTATION_YET);
  }
}

- (NSArray<NewTabPageColorPalette*>*)buildNTPThemeColorPalettes {
  const SeedColor& violet = kSeedColors[kVioletSeedColorIndex];
  return @[
    [[NewTabPageColorPalette alloc] init],
    CreateColorPaletteFromSeedColor(UIColorFromRGB(violet.color),
                                    violet.variant),
  ];
}

// Updates `_actionableConfig.backgroundImages` when a preset thumbnail image
// finishes downloading.
- (void)onPresetBackgroundImageFetched:(const std::string&)imageData
                               atIndex:(size_t)index {
  UIImage* image =
      [UIImage imageWithData:[NSData dataWithBytes:imageData.data()
                                            length:imageData.size()]];
  if (!image || !_actionableConfig || index >= _presetBackgrounds.size()) {
    return;
  }
  _presetBackgrounds[index].image = image;
  NSMutableArray<UIImage*>* backgroundImages =
      [NSMutableArray arrayWithCapacity:_presetBackgrounds.size()];
  for (const NtpCustomBackgroundWithImage& background : _presetBackgrounds) {
    [backgroundImages addObject:background.image];
  }
  _actionableConfig.backgroundImages = backgroundImages;
  [self.delegate tipsMagicStackMediatorDidReconfigureItem];
}

@end
