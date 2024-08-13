// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_mediator.h"

#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_consumer.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation DefaultBrowserGenericPromoMediator {
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
    _segmentationService = segmentationService;
    _deviceSwitcherResultDispatcher = dispatcher;
  }
  return self;
}

- (void)disconnect {
  _segmentationService = nullptr;
  _deviceSwitcherResultDispatcher = nullptr;
}

- (void)setConsumer:(id<DefaultBrowserGenericPromoConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    // Sets the Default Browser screen view title with targeted messaging based
    // on the user's segment.
    [_consumer setPromoTitle:l10n_util::GetNSString(
                                 GetDefaultBrowserGenericPromoTitleStringID(
                                     _userSegment))];
  }
}

- (void)didTapPrimaryActionButton {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
}

// Retrieves user segmentation data from the Segmentation Platform.
- (void)retrieveUserSegmentWithCompletion:(ProceduralBlock)completion {
  CHECK(_segmentationService);
  CHECK(_deviceSwitcherResultDispatcher);
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

#pragma mark - Private

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

@end
