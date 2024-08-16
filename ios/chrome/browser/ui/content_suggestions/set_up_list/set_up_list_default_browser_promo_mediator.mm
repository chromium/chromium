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
    // Sets the Default Browser screen view title to the consumer with targeted
    // messaging based on the user's segment.
    [_consumer setPromoTitle:l10n_util::GetNSString(
                                 GetFirstRunDefaultBrowserScreenTitleStringID(
                                     _userSegment))];
    [_consumer
        setPromoSubtitle:l10n_util::GetNSString(
                             GetFirstRunDefaultBrowserScreenSubtitleStringID(
                                 _userSegment))];
  }
}

#pragma mark - Private

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
