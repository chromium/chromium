// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"

#import "base/i18n/number_formatting.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/image_fetcher/core/image_fetcher.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "components/prefs/pref_service.h"
#import "components/themes/ntp_background_data.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/utils.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/home_customization/coordinator/background_customization_configuration_item.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_navigation_delegate.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/ui/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_discover_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_magic_stack_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_consumer.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/theme_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

@implementation HomeCustomizationMediator {
  // Pref service to handle preference changes.
  raw_ptr<PrefService> _prefService;
  // Browser agent to be notified of Discover eligibility.
  raw_ptr<DiscoverFeedVisibilityBrowserAgent>
      _discoverFeedVisibilityBrowserAgent;
  // The image fetcher used to download individual background images.
  raw_ptr<image_fetcher::ImageFetcher> _imageFetcher;
  // The Background customization service for getting current and recently used
  // backgrounds.
  raw_ptr<HomeBackgroundCustomizationService> _backgroundService;
  // The image manager used to load uesr uploaded images.
  raw_ptr<UserUploadedImageManager> _userUploadedImageManager;

  // Whether the theme has been changed.
  BOOL _themeHasChanged;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
    discoverFeedVisibilityBrowserAgent:
        (DiscoverFeedVisibilityBrowserAgent*)discoverFeedVisibilityBrowserAgent
                     backgroundService:
                         (HomeBackgroundCustomizationService*)backgroundService
                   imageFetcherService:
                       (image_fetcher::ImageFetcherService*)imageFetcherService
              userUploadedImageManager:
                  (UserUploadedImageManager*)userUploadedImageManager {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _discoverFeedVisibilityBrowserAgent = discoverFeedVisibilityBrowserAgent;
    _backgroundService = backgroundService;
    _imageFetcher = imageFetcherService->GetImageFetcher(
        image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
    _userUploadedImageManager = userUploadedImageManager;
  }
  return self;
}

#pragma mark - Public

- (void)configureMainPageData {
  std::map<CustomizationToggleType, BOOL> toggleMap = {
      {CustomizationToggleType::kMostVisited,
       [self isModuleEnabledForType:CustomizationToggleType::kMostVisited]},
      {CustomizationToggleType::kMagicStack,
       [self isModuleEnabledForType:CustomizationToggleType::kMagicStack]},
  };
  if (_discoverFeedVisibilityBrowserAgent->GetEligibility() ==
      DiscoverFeedEligibility::kEligible) {
    toggleMap.insert(
        {CustomizationToggleType::kDiscover,
         [self isModuleEnabledForType:CustomizationToggleType::kDiscover]});
  }
  [self.mainPageConsumer populateToggles:toggleMap];

  if (IsNTPBackgroundCustomizationEnabled() &&
      !_backgroundService->IsCustomizationDisabledOrColorManagedByPolicy()) {
    BackgroundCollectionConfiguration* collectionConfiguration =
        [[BackgroundCollectionConfiguration alloc] init];

    // Create and add a background configuration with no background applied.
    BackgroundCustomizationConfigurationItem* defaultConfig =
        [[BackgroundCustomizationConfigurationItem alloc] initWithNoBackground];
    collectionConfiguration.configurations[defaultConfig.configurationID] =
        defaultConfig;
    [collectionConfiguration.configurationOrder
        addObject:defaultConfig.configurationID];

    // Figure out the current background. This may not be element 1 in the
    // recently used backgrounds list if the current background is the default.
    // Or after a section toggle has been activated, as that refreshes all of
    // the data.
    std::optional<HomeCustomBackground> customBackground =
        _backgroundService->GetCurrentCustomBackground();
    std::optional<sync_pb::UserColorTheme> colorTheme =
        _backgroundService->GetCurrentColorTheme();
    std::optional<RecentlyUsedBackground> current = std::nullopt;
    if (customBackground) {
      current = customBackground.value();
    } else if (colorTheme) {
      current = colorTheme.value();
    }

    NSString* selectedBackgroundID;
    if (!current.has_value()) {
      selectedBackgroundID = defaultConfig.configurationID;
    }

    for (RecentlyUsedBackground background :
         _backgroundService->GetRecentlyUsedBackgrounds()) {
      BackgroundCustomizationConfigurationItem* config =
          [self generateConfigurationItemForRecentBackground:background];
      if (!config) {
        continue;
      }
      collectionConfiguration.configurations[config.configurationID] = config;
      [collectionConfiguration.configurationOrder
          addObject:config.configurationID];

      if (background == current) {
        selectedBackgroundID = config.configurationID;
      }
    }

    [self.mainPageConsumer
        populateBackgroundCollectionConfiguration:collectionConfiguration
                             selectedBackgroundId:selectedBackgroundID];
  }
}

- (void)configureDiscoverPageData {
  std::vector<CustomizationLinkType> linksVector = {
      CustomizationLinkType::kFollowing,
      CustomizationLinkType::kHidden,
      CustomizationLinkType::kActivity,
      CustomizationLinkType::kLearnMore,
  };
  [self.discoverPageConsumer populateDiscoverLinks:linksVector];
}

- (void)configureMagicStackPageData {
  std::map<CustomizationToggleType, BOOL> toggleMap = {
      {CustomizationToggleType::kSetUpList,
       [self
           isMagicStackCardEnabledForType:CustomizationToggleType::kSetUpList]},
      {CustomizationToggleType::kSafetyCheck,
       [self isMagicStackCardEnabledForType:CustomizationToggleType::
                                                kSafetyCheck]},
      {CustomizationToggleType::kTapResumption,
       [self isMagicStackCardEnabledForType:CustomizationToggleType::
                                                kTapResumption]},
  };
  if (IsTipsMagicStackEnabled()) {
    toggleMap.insert(
        {CustomizationToggleType::kTips,
         [self isMagicStackCardEnabledForType:CustomizationToggleType::kTips]});
  }
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1 ||
      commerce::kShopCardVariation.Get() == commerce::kShopCardArm2) {
    toggleMap.insert({CustomizationToggleType::kShopCard,
                      [self isMagicStackCardEnabledForType:
                                CustomizationToggleType::kShopCard]});
  }
  [self.magicStackPageConsumer populateToggles:toggleMap];
}

- (void)saveCurrentTheme {
  if (_themeHasChanged) {
    _backgroundService->StoreCurrentTheme();
    _themeHasChanged = NO;
  }
}

#pragma mark - Private

// Returns whether the module with `type` is enabled in the preferences.
- (BOOL)isModuleEnabledForType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMostVisitedEnabled);
    case CustomizationToggleType::kMagicStack:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackEnabled);
    case CustomizationToggleType::kDiscover:
      return _discoverFeedVisibilityBrowserAgent->IsEnabled();
    default:
      NOTREACHED();
  }
}

// Returns whether the Magic Stack card with `type` is enabled in the
// preferences.
- (BOOL)isMagicStackCardEnabledForType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kSetUpList:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackSetUpListEnabled);
    case CustomizationToggleType::kSafetyCheck:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackSafetyCheckEnabled);
    case CustomizationToggleType::kTapResumption:
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackTabResumptionEnabled);
    case CustomizationToggleType::kTips: {
      CHECK(IsTipsMagicStackEnabled());
      return _prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackTipsEnabled);
    }
    case CustomizationToggleType::kShopCard:
      if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1) {
        return _prefService->GetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled);
      } else if (commerce::kShopCardVariation.Get() ==
                 commerce::kShopCardArm2) {
        return _prefService->GetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled);
      } else {
        return false;
      }
    default:
      NOTREACHED();
  }
}

// Applies the user-uploaded photo background to the NTP.
- (void)applyUserUploadedBackground:
    (BackgroundCustomizationConfigurationItem*)configurationItem {
  FramingCoordinates coordinates =
      FramingCoordinatesFromHomeCustomizationFramingCoordinates(
          configurationItem.userUploadedFramingCoordinates);
  _backgroundService->SetCurrentUserUploadedBackground(
      base::SysNSStringToUTF8(configurationItem.userUploadedImagePath),
      coordinates);
}

// Applies the preset gallery background for the given collection image.
- (void)applyPresetGalleryBackgroundForCollectionImage:
    (const CollectionImage&)collectionImage {
  std::string attribution_line_1;
  std::string attribution_line_2;
  // Set attribution lines if available.
  if (!collectionImage.attribution.empty()) {
    attribution_line_1 = collectionImage.attribution[0];
    if (collectionImage.attribution.size() > 1) {
      attribution_line_2 = collectionImage.attribution[1];
    }
  }

  _backgroundService->SetCurrentBackground(
      collectionImage.image_url, collectionImage.thumbnail_image_url,
      attribution_line_1, attribution_line_2,
      collectionImage.attribution_action_url, collectionImage.collection_id);
}

- (void)applyPresetGalleryBackgroundForCustomBackground:
            (const sync_pb::NtpCustomBackground)customBackground
                                           thumbnailURL:
                                               (const GURL&)thumbnailURL {
  _backgroundService->SetCurrentBackground(
      GURL(customBackground.url()), thumbnailURL,
      customBackground.attribution_line_1(),
      customBackground.attribution_line_2(),
      GURL(customBackground.attribution_action_url()),
      customBackground.collection_id());
}

// Applies a background color to the NTP.
- (void)applyBackgroundColor:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  if (![backgroundConfiguration
          isKindOfClass:[BackgroundCustomizationConfigurationItem class]]) {
    // Only `BackgroundCustomizationConfigurationItem` exposes required
    // fields. Other implementations may not support expected properties.
    return;
  }

  BackgroundCustomizationConfigurationItem* configurationItem =
      static_cast<BackgroundCustomizationConfigurationItem*>(
          backgroundConfiguration);

  if (!configurationItem.backgroundColor) {
    [self applyDefaultBackground];
    return;
  }

  _backgroundService->SetBackgroundColor(
      skia::UIColorToSkColor(configurationItem.backgroundColor),
      SchemeVariantToProtoEnum(configurationItem.colorVariant));
}

// Sets the NTP to the default background (no color, no image, etc.).
- (void)applyDefaultBackground {
  _backgroundService->ClearCurrentBackground();
}

// Generates a `BackgroundCustomizationConfigurationItem` for the provided
// recently used background to display in the UI.
- (BackgroundCustomizationConfigurationItem*)
    generateConfigurationItemForRecentBackground:
        (RecentlyUsedBackground)recentBackground {
  if (std::holds_alternative<HomeCustomBackground>(recentBackground)) {
    HomeCustomBackground customBackground =
        std::get<HomeCustomBackground>(recentBackground);
    if (std::holds_alternative<sync_pb::NtpCustomBackground>(
            customBackground)) {
      sync_pb::NtpCustomBackground ntpCustomBackground =
          std::get<sync_pb::NtpCustomBackground>(customBackground);
      return [[BackgroundCustomizationConfigurationItem alloc]
          initWithNtpCustomBackground:ntpCustomBackground];
    } else {
      HomeUserUploadedBackground currentUserUploadedBackground =
          std::get<HomeUserUploadedBackground>(customBackground);
      NSString* imagePath =
          base::SysUTF8ToNSString(currentUserUploadedBackground.image_path);

      return [[BackgroundCustomizationConfigurationItem alloc]
          initWithUserUploadedImagePath:imagePath
                     framingCoordinates:currentUserUploadedBackground
                                            .framing_coordinates];
    }
  } else {
    sync_pb::UserColorTheme colorTheme =
        std::get<sync_pb::UserColorTheme>(recentBackground);

    UIColor* backgroundColor = UIColorFromRGB(colorTheme.color());
    ui::ColorProviderKey::SchemeVariant colorVariant =
        ProtoEnumToSchemeVariant(colorTheme.browser_color_variant());
    return [[BackgroundCustomizationConfigurationItem alloc]
        initWithBackgroundColor:backgroundColor
                   colorVariant:colorVariant];
  }
}

// Generates a `RecentlyUsedBackground` for the provided
// `BackgroundCustomizationConfigurationItem`.
- (RecentlyUsedBackground)generateRecentBackgroundForConfiguration:
    (BackgroundCustomizationConfigurationItem*)configuration {
  switch (configuration.backgroundStyle) {
    case HomeCustomizationBackgroundStyle::kDefault: {
      return RecentlyUsedBackground();
    }
    case HomeCustomizationBackgroundStyle::kColor: {
      sync_pb::UserColorTheme colorTheme;
      colorTheme.set_color(
          skia::UIColorToSkColor(configuration.backgroundColor));
      colorTheme.set_browser_color_variant(
          SchemeVariantToProtoEnum(configuration.colorVariant));
      return colorTheme;
    }
    case HomeCustomizationBackgroundStyle::kPreset: {
      return configuration.customBackground;
    }
    case HomeCustomizationBackgroundStyle::kUserUploaded: {
      HomeUserUploadedBackground userUploadedBackground;
      userUploadedBackground.image_path =
          base::SysNSStringToUTF8(configuration.userUploadedImagePath);
      userUploadedBackground.framing_coordinates =
          FramingCoordinatesFromHomeCustomizationFramingCoordinates(
              configuration.userUploadedFramingCoordinates);

      return userUploadedBackground;
    }
  }
}

#pragma mark - HomeCustomizationMutator

- (void)toggleModuleVisibilityForType:(CustomizationToggleType)type
                              enabled:(BOOL)enabled {
  [HomeCustomizationMetricsRecorder recordCellToggled:type];
  switch (type) {
    // Main page toggles.
    case CustomizationToggleType::kMostVisited:
      _prefService->SetBoolean(prefs::kHomeCustomizationMostVisitedEnabled,
                               enabled);
      break;
    case CustomizationToggleType::kMagicStack:
      _prefService->SetBoolean(prefs::kHomeCustomizationMagicStackEnabled,
                               enabled);
      break;
    case CustomizationToggleType::kDiscover:
      _discoverFeedVisibilityBrowserAgent->SetEnabled(enabled);
      break;

    // Magic Stack page toggles.
    case CustomizationToggleType::kSetUpList:
      _prefService->SetBoolean(
          prefs::kHomeCustomizationMagicStackSetUpListEnabled, enabled);
      break;
    case CustomizationToggleType::kSafetyCheck:
      _prefService->SetBoolean(
          prefs::kHomeCustomizationMagicStackSafetyCheckEnabled, enabled);
      break;
    case CustomizationToggleType::kTapResumption:
      _prefService->SetBoolean(
          prefs::kHomeCustomizationMagicStackTabResumptionEnabled, enabled);
      break;
    case CustomizationToggleType::kTips: {
      CHECK(IsTipsMagicStackEnabled());
      _prefService->SetBoolean(prefs::kHomeCustomizationMagicStackTipsEnabled,
                               enabled);
      break;
    }
    case CustomizationToggleType::kShopCard:
      if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm1) {
        _prefService->SetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardPriceTrackingEnabled,
            enabled);
      } else if (commerce::kShopCardVariation.Get() ==
                 commerce::kShopCardArm2) {
        _prefService->SetBoolean(
            prefs::kHomeCustomizationMagicStackShopCardReviewsEnabled, enabled);
      }
      break;
  }
}

- (void)navigateToSubmenuForType:(CustomizationToggleType)type {
  [self.navigationDelegate
      presentCustomizationMenuPage:[HomeCustomizationHelper
                                       menuPageForToggleType:type]];
}

- (void)navigateToLinkForType:(CustomizationLinkType)type {
  GURL URL;
  switch (type) {
    case CustomizationLinkType::kFollowing:
      URL = GURL(kDiscoverFollowingURL);
      break;
    case CustomizationLinkType::kHidden:
      URL = GURL(kDiscoverHiddenURL);
      break;
    case CustomizationLinkType::kActivity:
      URL = GURL(kDiscoverActivityURL);
      break;
    case CustomizationLinkType::kLearnMore:
      URL = GURL(kDiscoverLearnMoreURL);
      break;
    case CustomizationLinkType::kEnterpriseLearnMore:
      URL = GURL(kManagementLearnMoreURL);
      break;
  }
  [self.navigationDelegate navigateToURL:URL];
}

- (void)dismissMenuPage {
  [self.navigationDelegate dismissMenuPage];
}

- (void)applyBackgroundForConfiguration:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  if (![backgroundConfiguration
          isKindOfClass:[BackgroundCustomizationConfigurationItem class]]) {
    return;
  }

  _themeHasChanged = YES;

  BackgroundCustomizationConfigurationItem* configurationItem =
      static_cast<BackgroundCustomizationConfigurationItem*>(
          backgroundConfiguration);
  switch (configurationItem.backgroundStyle) {
    case HomeCustomizationBackgroundStyle::kUserUploaded:
      [self applyUserUploadedBackground:configurationItem];
      break;
    case HomeCustomizationBackgroundStyle::kPreset:
      // Use whichever data item has a URL.
      if (!configurationItem.collectionImage.image_url.is_empty()) {
        [self applyPresetGalleryBackgroundForCollectionImage:
                  configurationItem.collectionImage];
      } else {
        [self
            applyPresetGalleryBackgroundForCustomBackground:
                configurationItem.customBackground
                                               thumbnailURL:configurationItem
                                                                .thumbnailURL];
      }
      break;
    case HomeCustomizationBackgroundStyle::kColor:
      [self applyBackgroundColor:backgroundConfiguration];
      break;
    case HomeCustomizationBackgroundStyle::kDefault:
      [self applyDefaultBackground];
      break;
    default:
      NOTREACHED();
  }
}

- (void)deleteBackgroundFromRecentlyUsed:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  if (![backgroundConfiguration
          isKindOfClass:[BackgroundCustomizationConfigurationItem class]]) {
    // Only `BackgroundCustomizationConfigurationItem` exposes required
    // fields. Other implementations may not support expected properties.
    return;
  }

  BackgroundCustomizationConfigurationItem* configurationItem =
      static_cast<BackgroundCustomizationConfigurationItem*>(
          backgroundConfiguration);

  _backgroundService->DeleteRecentlyUsedBackground(
      [self generateRecentBackgroundForConfiguration:configurationItem]);
}

- (void)fetchBackgroundCustomizationThumbnailURLImage:(GURL)thumbnailURL
                                           completion:
                                               (void (^)(UIImage*))completion {
  CHECK(!thumbnailURL.is_empty());
  CHECK(thumbnailURL.is_valid());

  _imageFetcher->FetchImage(
      thumbnailURL,
      base::BindOnce(^(const gfx::Image& image,
                       const image_fetcher::RequestMetadata& metadata) {
        if (!image.IsEmpty()) {
          UIImage* uiImage = image.ToUIImage();
          if (completion) {
            completion(uiImage);
          }
        }
      }),
      // TODO (crbug.com/417234848): Add annotation.
      image_fetcher::ImageFetcherParams(NO_TRAFFIC_ANNOTATION_YET, "Test"));
}

- (void)fetchBackgroundCustomizationUserUploadedImage:(NSString*)imagePath
                                           completion:
                                               (void (^)(UIImage*))completion {
  DCHECK(imagePath.length > 0);

  base::FilePath path = base::FilePath(base::SysNSStringToUTF8(imagePath));

  _userUploadedImageManager->LoadUserUploadedImage(path,
                                                   base::BindOnce(completion));
}

@end
