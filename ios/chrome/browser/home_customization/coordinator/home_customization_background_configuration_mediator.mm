// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_configuration_mediator.h"

#import <Foundation/Foundation.h>

#import "base/debug/dump_without_crashing.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/image_fetcher/core/image_fetcher.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "components/image_fetcher/core/request_metadata.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "ios/chrome/browser/home_customization/coordinator/background_customization_configuration_item.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer_bridge.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/home_customization_seed_colors.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/ui/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_accessibility_identifiers.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_presentation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/theme_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

namespace {

// Key for Image Fetcher UMA metrics.
constexpr char kImageFetcherUmaClient[] = "HomeCustomization";

// NetworkTrafficAnnotationTag for fetching background gallery image from Google
// server.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "home_customization_background_gallery_image",
        R"(
        semantics {
        sender: "HomeCustomization"
        description:
            "Sends a request to a Google server to load a background gallery "
            "image."
        trigger:
            "A request will be sent when the user opens the customization menu "
            " and sees their recently chosen backgrounds."
        data: "Only image url, no user data"
        destination: GOOGLE_OWNED_SERVICE
        }
        policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        chrome_policy: {
          NTPCustomBackgroundEnabled {
            NTPCustomBackgroundEnabled: false
          }
        }
        }
        )");

}  // namespace

@interface HomeCustomizationBackgroundConfigurationMediator () <
    HomeBackgroundCustomizationServiceObserving> {
  // The image fetcher used to download individual background preset images.
  raw_ptr<image_fetcher::ImageFetcher, DanglingUntriaged> _imageFetcher;
  // The service that provides the background images.
  raw_ptr<HomeBackgroundImageService, DanglingUntriaged>
      _homeBackgroundImageService;
  // Used to get and observe the background state.
  raw_ptr<HomeBackgroundCustomizationService, DanglingUntriaged>
      _backgroundCustomizationService;
  raw_ptr<UserUploadedImageManager, DanglingUntriaged>
      _userUploadedImageManager;

  // Observer for the customization service.
  std::unique_ptr<HomeBackgroundCustomizationServiceObserverBridge>
      _backgroundCustomizationServiceObserverBridge;
}

// Redefine public property as readwrite
@property(nonatomic, readwrite, assign) BOOL themeHasChanged;

@end

@implementation HomeCustomizationBackgroundConfigurationMediator

- (instancetype)
    initWithBackgroundCustomizationService:
        (HomeBackgroundCustomizationService*)backgroundCustomizationService
                              imageFetcher:
                                  (image_fetcher::ImageFetcher*)imageFetcher
                homeBackgroundImageService:
                    (HomeBackgroundImageService*)homeBackgroundImageService
                  userUploadedImageManager:
                      (UserUploadedImageManager*)userUploadedImageManager {
  self = [super init];
  if (self) {
    _backgroundCustomizationService = backgroundCustomizationService;
    _backgroundCustomizationServiceObserverBridge =
        std::make_unique<HomeBackgroundCustomizationServiceObserverBridge>(
            _backgroundCustomizationService, self);
    _imageFetcher = imageFetcher;
    _homeBackgroundImageService = homeBackgroundImageService;
    _userUploadedImageManager = userUploadedImageManager;
  }
  return self;
}

- (void)loadGalleryBackgroundConfigurations {
  CHECK(_homeBackgroundImageService);
  __weak __typeof(self) weakSelf = self;
  _homeBackgroundImageService->FetchCollectionsImages(
      base::BindOnce(^(const HomeBackgroundImageService::CollectionImageMap&
                           collectionMapParam) {
        if (!collectionMapParam.empty()) {
          [weakSelf onCollectionDataReceived:collectionMapParam];
        }
      }));
}

- (void)loadRecentlyUsedBackgroundConfigurations {
  BackgroundCollectionConfiguration* collectionConfiguration =
      [[BackgroundCollectionConfiguration alloc] init];

  // Create and add a background configuration with no background applied.
  BackgroundCustomizationConfigurationItem* defaultConfig =
      [[BackgroundCustomizationConfigurationItem alloc] initWithNoBackground];
  defaultConfig.accessibilityName = l10n_util::GetNSString(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_DEFAULT_ACCESSIBILITY_LABEL);
  collectionConfiguration.configurations[defaultConfig.configurationID] =
      defaultConfig;
  [collectionConfiguration.configurationOrder
      addObject:defaultConfig.configurationID];

  // Figure out the current background. This may not be element 1 in the
  // recently used backgrounds list if the current background is the default.
  // Or after a section toggle has been activated, as that refreshes all of
  // the data.
  std::optional<HomeCustomBackground> customBackground =
      _backgroundCustomizationService->GetCurrentCustomBackground();
  std::optional<sync_pb::UserColorTheme> colorTheme =
      _backgroundCustomizationService->GetCurrentColorTheme();
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
       _backgroundCustomizationService->GetRecentlyUsedBackgrounds()) {
    BackgroundCustomizationConfigurationItem* config =
        [self generateConfigurationItemForRecentBackground:background];
    if (!config) {
      continue;
    }

    if ([collectionConfiguration.configurationOrder
            containsObject:config.configurationID]) {
      static crash_reporter::CrashKeyString<64> id_key(
          "duplicate-recent-configuration-id");
      id_key.Set(base::SysNSStringToUTF8(config.configurationID));
      base::debug::DumpWithoutCrashing();
      continue;
    }

    collectionConfiguration.configurations[config.configurationID] = config;
    [collectionConfiguration.configurationOrder
        addObject:config.configurationID];

    if (background == current) {
      selectedBackgroundID = config.configurationID;
    }
  }

  [self.consumer
      setBackgroundCollectionConfigurations:@[ collectionConfiguration ]
                       selectedBackgroundId:selectedBackgroundID];
}

- (void)loadColorBackgroundConfigurations {
  BackgroundCollectionConfiguration* collectionConfiguration =
      [[BackgroundCollectionConfiguration alloc] init];
  std::optional<sync_pb::UserColorTheme> colorTheme =
      _backgroundCustomizationService->GetCurrentColorTheme();
  NSString* selectedColorID = nil;

  BackgroundCustomizationConfigurationItem* noBackgroundConfiguration =
      [[BackgroundCustomizationConfigurationItem alloc] initWithNoBackground];
  collectionConfiguration
      .configurations[noBackgroundConfiguration.configurationID] =
      noBackgroundConfiguration;
  [collectionConfiguration.configurationOrder
      addObject:noBackgroundConfiguration.configurationID];

  for (SeedColor seedColor : kSeedColors) {
    BackgroundCustomizationConfigurationItem* item =
        [[BackgroundCustomizationConfigurationItem alloc]
            initWithBackgroundColor:UIColorFromRGB(seedColor.color)
                       colorVariant:seedColor.variant
                  accessibilityName:l10n_util::GetNSString(
                                        seedColor.accessibilityNameId)];
    collectionConfiguration.configurations[item.configurationID] = item;
    [collectionConfiguration.configurationOrder addObject:item.configurationID];

    if (colorTheme && colorTheme->color() &&
        seedColor.color == colorTheme->color()) {
      selectedColorID = item.configurationID;
    }
  }

  BOOL isCustomColor = IsNTPBackgroundColorSliderEnabled() &&
                       !selectedColorID && colorTheme && colorTheme->color();
  BOOL isDefaultBackground =
      !selectedColorID &&
      !_backgroundCustomizationService->GetCurrentCustomBackground();

  if (IsNTPBackgroundColorSliderEnabled()) {
    // The hue slider displays either the custom color or the default red, since
    // hue = 0% represents red on the color wheel.
    UIColor* hueSliderColor =
        isCustomColor ? skia::UIColorFromSkColor(colorTheme->color())
                      : UIColor.redColor;

    BackgroundCustomizationConfigurationItem* customHueConfiguration =
        [[BackgroundCustomizationConfigurationItem alloc]
            initWithBackgroundColor:hueSliderColor
                       colorVariant:ui::ColorProviderKey::SchemeVariant::
                                        kTonalSpot
                  accessibilityName:
                      l10n_util::GetNSString(
                          IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_CUSTOM_ACCESSIBILITY_LABEL)];
    customHueConfiguration.isCustomColor = isCustomColor;
    collectionConfiguration
        .configurations[customHueConfiguration.configurationID] =
        customHueConfiguration;
    [collectionConfiguration.configurationOrder
        addObject:customHueConfiguration.configurationID];

    if (isCustomColor) {
      selectedColorID = customHueConfiguration.configurationID;
    }
  }

  if (!isCustomColor && isDefaultBackground) {
    selectedColorID = noBackgroundConfiguration.configurationID;
  }

  [self.consumer
      setBackgroundCollectionConfigurations:@[ collectionConfiguration ]
                       selectedBackgroundId:selectedColorID];
}

- (void)saveCurrentTheme {
  if (!self.themeHasChanged) {
    return;
  }

  _backgroundCustomizationService->StoreCurrentTheme();
  self.themeHasChanged = NO;
  self.backgroundSelectionOutcome = BackgroundSelectionOutcome::kApplied;
}

- (void)cancelThemeSelection {
  if (!self.themeHasChanged) {
    self.backgroundSelectionOutcome = BackgroundSelectionOutcome::kCanceled;
    return;
  }

  _backgroundCustomizationService->RestoreCurrentTheme();
  self.themeHasChanged = NO;
  self.backgroundSelectionOutcome =
      BackgroundSelectionOutcome::kCanceledAfterSelected;
}

#pragma mark - HomeCustomizationBackgroundConfigurationMutator

- (void)fetchBackgroundCustomizationThumbnailURLImage:(GURL)thumbnailURL
                                           completion:(void (^)(UIImage* image,
                                                                NSError* error))
                                                          completion {
  CHECK(!thumbnailURL.is_empty());
  CHECK(thumbnailURL.is_valid());

  _imageFetcher->FetchImage(
      thumbnailURL,
      base::BindOnce(^(const gfx::Image& image,
                       const image_fetcher::RequestMetadata& metadata) {
        if (image.IsEmpty()) {
          // Image fetch failed or returned empty.
          NSDictionary<NSErrorUserInfoKey, id>* userInfo = @{
            NSURLErrorFailingURLStringErrorKey :
                base::SysUTF8ToNSString(thumbnailURL.spec())
          };
          NSError* fetchError = [NSError errorWithDomain:NSURLErrorDomain
                                                    code:NSURLErrorUnknown
                                                userInfo:userInfo];
          base::UmaHistogramBoolean("IOS.HomeCustomization.Background.Gallery."
                                    "ImageDownloadSuccessful",
                                    false);
          base::UmaHistogramSparse(
              "IOS.HomeCustomization.Background.Gallery.ImageDownloadErrorCode",
              metadata.http_response_code);
          completion(nil, fetchError);
          return;
        }
        UIImage* uiImage = image.ToUIImage();
        base::UmaHistogramBoolean(
            "IOS.HomeCustomization.Background.Gallery.ImageDownloadSuccessful",
            true);
        if (completion) {
          completion(uiImage, nil);
        }
      }),
      image_fetcher::ImageFetcherParams(kTrafficAnnotation,
                                        kImageFetcherUmaClient));
}

- (void)fetchBackgroundCustomizationUserUploadedImage:(NSString*)imagePath
                                           completion:
                                               (UserUploadImageCompletion)
                                                   completion {
  DCHECK(imagePath.length > 0);
  CHECK(_userUploadedImageManager);

  base::FilePath path = base::FilePath(base::SysNSStringToUTF8(imagePath));

  _userUploadedImageManager->LoadUserUploadedImage(path,
                                                   base::BindOnce(completion));
}

- (void)applyBackgroundForConfiguration:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  if (![backgroundConfiguration
          isKindOfClass:[BackgroundCustomizationConfigurationItem class]]) {
    return;
  }

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
      } else if (!configurationItem.customBackground.url().empty()) {
        [self
            applyPresetGalleryBackgroundForCustomBackground:
                configurationItem.customBackground
                                               thumbnailURL:configurationItem
                                                                .thumbnailURL];
      } else {
        NOTREACHED();
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

  _backgroundCustomizationService->DeleteRecentlyUsedBackground(
      [self generateRecentBackgroundForConfiguration:configurationItem]);
}

- (void)saveBackground {
  [self saveCurrentTheme];
}

#pragma mark - HomeBackgroundCustomizationServiceObserving

- (void)onBackgroundChanged {
  self.themeHasChanged = YES;

  id<BackgroundCustomizationConfiguration> currentConfiguration;

  std::optional<HomeCustomBackground> customBackground =
      _backgroundCustomizationService->GetCurrentCustomBackground();
  std::optional<sync_pb::UserColorTheme> colorTheme =
      _backgroundCustomizationService->GetCurrentColorTheme();
  if (customBackground) {
    currentConfiguration = [self
        generateConfigurationItemForRecentBackground:customBackground.value()];
  } else if (colorTheme) {
    currentConfiguration =
        [self generateConfigurationItemForRecentBackground:colorTheme.value()];
  } else {
    currentConfiguration =
        [[BackgroundCustomizationConfigurationItem alloc] initWithNoBackground];
  }

  [self.consumer currentBackgroundConfigurationChanged:currentConfiguration];
}

#pragma mark - Private

// Callback function that is called when the collection images are fetched. This
// will then create BackgroundCollectionConfiguration objects and send them to
// the consumer.
- (void)onCollectionDataReceived:
    (HomeBackgroundImageService::CollectionImageMap)collectionMap {
  NSMutableArray<BackgroundCollectionConfiguration*>* collectionConfigurations =
      [NSMutableArray array];

  std::optional<HomeCustomBackground> background =
      _backgroundCustomizationService->GetCurrentCustomBackground();

  std::optional<sync_pb::NtpCustomBackground> ntpCustomBackground;
  if (background && std::holds_alternative<sync_pb::NtpCustomBackground>(
                        background.value())) {
    ntpCustomBackground =
        std::get<sync_pb::NtpCustomBackground>(background.value());
  }

  NSString* selectedBackgroundId = nil;

  for (const auto& [collectionName, collectionImages] : collectionMap) {
    // Create a new section for the collection.
    BackgroundCollectionConfiguration* section =
        [[BackgroundCollectionConfiguration alloc] init];
    section.collectionName = base::SysUTF8ToNSString(collectionName);
    for (size_t i = 0; i < collectionImages.size(); i++) {
      const auto& image = collectionImages[i];

      NSString* accessibilityName =
          base::SysUTF8ToNSString(base::JoinString(image.attribution, " "));
      NSString* accessibilityValue = base::SysUTF16ToNSString(
          base::i18n::MessageFormatter::FormatWithNamedArgs(
              l10n_util::GetStringUTF16(
                  IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_ACCESSIBILITY_VALUE),
              "position", static_cast<int>(i) + 1, "total",
              static_cast<int>(collectionImages.size())));

      BackgroundCustomizationConfigurationItem* config =
          [[BackgroundCustomizationConfigurationItem alloc]
              initWithCollectionImage:image
                    accessibilityName:accessibilityName
                   accessibilityValue:accessibilityValue];

      [section.configurations setObject:config forKey:config.configurationID];
      [section.configurationOrder addObject:config.configurationID];

      if (ntpCustomBackground &&
          image.image_url == ntpCustomBackground->url()) {
        selectedBackgroundId = config.configurationID;
      }
    }
    [collectionConfigurations addObject:section];
  }

  [self.consumer setBackgroundCollectionConfigurations:collectionConfigurations
                                  selectedBackgroundId:selectedBackgroundId];
}

// Applies the user-uploaded photo background to the NTP.
- (void)applyUserUploadedBackground:
    (BackgroundCustomizationConfigurationItem*)configurationItem {
  FramingCoordinates coordinates =
      FramingCoordinatesFromHomeCustomizationFramingCoordinates(
          configurationItem.userUploadedFramingCoordinates);
  _backgroundCustomizationService->SetCurrentUserUploadedBackground(
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

  _backgroundCustomizationService->SetCurrentBackground(
      collectionImage.image_url, collectionImage.thumbnail_image_url,
      attribution_line_1, attribution_line_2,
      collectionImage.attribution_action_url, collectionImage.collection_id);
}

// Applies the preset gallery background for the given NtpCustomBackground.
- (void)applyPresetGalleryBackgroundForCustomBackground:
            (const sync_pb::NtpCustomBackground)customBackground
                                           thumbnailURL:
                                               (const GURL&)thumbnailURL {
  _backgroundCustomizationService->SetCurrentBackground(
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

  _backgroundCustomizationService->SetBackgroundColor(
      skia::UIColorToSkColor(configurationItem.backgroundColor),
      SchemeVariantToProtoEnum(configurationItem.colorVariant));
}

- (void)applyDefaultBackground {
  _backgroundCustomizationService->ClearCurrentBackground();
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
      std::vector<std::string> attributions;

      if (ntpCustomBackground.has_attribution_line_1() &&
          !ntpCustomBackground.attribution_line_1().empty()) {
        attributions.push_back(ntpCustomBackground.attribution_line_1());
      }
      if (ntpCustomBackground.has_attribution_line_2() &&
          !ntpCustomBackground.attribution_line_2().empty()) {
        attributions.push_back(ntpCustomBackground.attribution_line_2());
      }

      NSString* accessibilityName =
          base::SysUTF8ToNSString(base::JoinString(attributions, " "));

      return [[BackgroundCustomizationConfigurationItem alloc]
          initWithNtpCustomBackground:ntpCustomBackground
                    accessibilityName:accessibilityName];
    } else {
      HomeUserUploadedBackground currentUserUploadedBackground =
          std::get<HomeUserUploadedBackground>(customBackground);
      NSString* imagePath =
          base::SysUTF8ToNSString(currentUserUploadedBackground.image_path);

      return [[BackgroundCustomizationConfigurationItem alloc]
          initWithUserUploadedImagePath:imagePath
                     framingCoordinates:currentUserUploadedBackground
                                            .framing_coordinates
                      accessibilityName:
                          l10n_util::GetNSString(
                              IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_LIBRARY_ACCESSIBILITY_LABEL)];
    }
  } else {
    sync_pb::UserColorTheme colorTheme =
        std::get<sync_pb::UserColorTheme>(recentBackground);

    auto it = std::find_if(kSeedColors.begin(), kSeedColors.end(),
                           [&colorTheme](const SeedColor& seedColor) {
                             return seedColor.color == colorTheme.color();
                           });

    NSString* accessibilityName =
        it == kSeedColors.end()
            ? nil
            : l10n_util::GetNSString(it->accessibilityNameId);

    UIColor* backgroundColor = UIColorFromRGB(colorTheme.color());
    ui::ColorProviderKey::SchemeVariant colorVariant =
        ProtoEnumToSchemeVariant(colorTheme.browser_color_variant());
    return [[BackgroundCustomizationConfigurationItem alloc]
        initWithBackgroundColor:backgroundColor
                   colorVariant:colorVariant
              accessibilityName:accessibilityName];
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

- (void)discardBackground {
  [self cancelThemeSelection];
}

@end
