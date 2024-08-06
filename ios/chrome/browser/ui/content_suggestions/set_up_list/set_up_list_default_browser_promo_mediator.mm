// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_mediator.h"

#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_consumer.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

@implementation SetUpListDefaultBrowserPromoMediator {
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      _segmentationService;
  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      _deviceSwitcherResultDispatcher;
  segmentation_platform::DefaultBrowserUserSegment _userSegment;
}

#pragma mark - Public

- (instancetype)initWithSegmentationService:
                    (segmentation_platform::SegmentationPlatformService*)
                        segmentationService
             deviceSwitcherResultDispatcher:
                 (segmentation_platform::DeviceSwitcherResultDispatcher*)
                     dispatcher {
  self = [super init];
  if (self) {
    CHECK(segmentationService);
    CHECK(dispatcher);
    _segmentationService = segmentationService;
    _deviceSwitcherResultDispatcher = dispatcher;
  }
  return self;
}

- (void)disconnect {
  _segmentationService = nullptr;
  _deviceSwitcherResultDispatcher = nullptr;
}

- (void)setConsumer:(id<DefaultBrowserScreenConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self updatePromoTitleForConsumer];
    [self updatePromoSubtitleForConsumer];
  }
}

#pragma mark - Private

// Retrieves user segmentation data from the Segmentation Platform.
- (void)retrieveUserSegmentWithCompletion:(ProceduralBlock)completion {
  segmentation_platform::PredictionOptions options =
      segmentation_platform::PredictionOptions::ForCached();
  segmentation_platform::ClassificationResult deviceSwitcherResult =
      _deviceSwitcherResultDispatcher->GetCachedClassificationResult();

  __weak __typeof(self) weakSelf = self;
  auto classificationResultCallback = base::BindOnce(
      [](__typeof(self) strongSelf,
         segmentation_platform::ClassificationResult deviceSwitcherResult,
         ProceduralBlock completion,
         const segmentation_platform::ClassificationResult& shopperResult) {
        [strongSelf didReceiveShopperSegmentationResult:shopperResult
                                   deviceSwitcherResult:deviceSwitcherResult];
        if (completion) {
          completion();
        }
      },
      weakSelf, deviceSwitcherResult, completion);
  _segmentationService->GetClassificationResult(
      segmentation_platform::kShoppingUserSegmentationKey, options, nullptr,
      std::move(classificationResultCallback));
}

// Sets user's highest priority targeted segment retrieved from the Segmentation
// Platform.
- (void)didReceiveShopperSegmentationResult:
            (const segmentation_platform::ClassificationResult&)shopperResult
                       deviceSwitcherResult:
                           (const segmentation_platform::ClassificationResult&)
                               deviceSwitcherResult {
  _userSegment =
      GetDefaultBrowserUserSegment(&deviceSwitcherResult, &shopperResult);
}

// Sets the Default Browser screen view title with targeted messaging based on
// the user's segment.
- (void)updatePromoTitleForConsumer {
  int promoTitleStringID = 0;
  switch (_userSegment) {
    case segmentation_platform::DefaultBrowserUserSegment::kDesktopUser:
    case segmentation_platform::DefaultBrowserUserSegment::kAndroidSwitcher:
      promoTitleStringID =
          UseIPadTailoredStringForDefaultBrowserPromo()
              ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPAD
              : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPHONE;
      break;
    case segmentation_platform::DefaultBrowserUserSegment::kShopper:
      promoTitleStringID =
          IDS_IOS_SEGMENTED_DEFAULT_BROWSER_SCREEN_SHOPPER_TITLE;
      break;
    case segmentation_platform::DefaultBrowserUserSegment::kDefault:
      promoTitleStringID =
          UseIPadTailoredStringForDefaultBrowserPromo()
              ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
              : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE;
      break;
  }
  CHECK_NE(promoTitleStringID, 0);
  [_consumer setPromoTitle:l10n_util::GetNSString(promoTitleStringID)];
}

// Sets the Default Browser screen view subtitle with targeted messaging based
// on the user's segment.
- (void)updatePromoSubtitleForConsumer {
  int promoSubtitleStringID = 0;
  switch (_userSegment) {
    case segmentation_platform::DefaultBrowserUserSegment::kDesktopUser:
      promoSubtitleStringID =
          UseIPadTailoredStringForDefaultBrowserPromo()
              ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPAD
              : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPHONE;
      break;
    case segmentation_platform::DefaultBrowserUserSegment::kAndroidSwitcher:
      promoSubtitleStringID =
          UseIPadTailoredStringForDefaultBrowserPromo()
              ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPAD
              : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPHONE;
      break;
    case segmentation_platform::DefaultBrowserUserSegment::kShopper:
      promoSubtitleStringID =
          IDS_IOS_SEGMENTED_DEFAULT_BROWSER_SCREEN_SHOPPER_SUBTITLE;
      break;
    case segmentation_platform::DefaultBrowserUserSegment::kDefault:
      promoSubtitleStringID =
          UseIPadTailoredStringForDefaultBrowserPromo()
              ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
              : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE;
      break;
  }
  CHECK_NE(promoSubtitleStringID, 0);
  [_consumer setPromoSubtitle:l10n_util::GetNSString(promoSubtitleStringID)];
}

@end
