// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_validation_tab_helper.h"

#import "base/metrics/histogram_functions.h"
#import "components/optimization_guide/core/hints_processing_util.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "url/gurl.h"

OptimizationGuideValidationTabHelper::OptimizationGuideValidationTabHelper(
    web::WebState* web_state) {
  if (base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideMetadataValidation)) {
    web_state->AddObserver(this);
  }

  if (OptimizationGuideService* optimization_guide_service =
          OptimizationGuideServiceFactory::GetForProfile(
              ProfileIOS::FromBrowserState(web_state->GetBrowserState()))) {
    optimization_guide_service->RegisterOptimizationTypes(
        {optimization_guide::proto::METADATA_FETCH_VALIDATION,
         optimization_guide::proto::BLOOM_FILTER_VALIDATION});
  }
}

OptimizationGuideValidationTabHelper::~OptimizationGuideValidationTabHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OptimizationGuideValidationTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(
      optimization_guide::features::kOptimizationGuideMetadataValidation));

  // Ignore non-committed navigations such as Downloads, etc.
  if (!navigation_context->HasCommitted())
    return;

  if (!navigation_context->GetUrl().SchemeIsHTTPOrHTTPS())
    return;

  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
  if (!optimization_guide_service)
    return;

  // Async.
  optimization_guide_service->CanApplyOptimization(
      navigation_context->GetUrl(),
      optimization_guide::proto::METADATA_FETCH_VALIDATION,
      base::BindOnce(&OptimizationGuideValidationTabHelper::
                         OnMetadataFetchValidationDecisionReceived,
                     weak_factory_.GetWeakPtr(), navigation_context->GetUrl()));
  // Sync.
  optimization_guide_service->CanApplyOptimization(
      navigation_context->GetUrl(),
      optimization_guide::proto::BLOOM_FILTER_VALIDATION,
      /*optimization_metadata=*/nullptr);
}

void OptimizationGuideValidationTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK(base::FeatureList::IsEnabled(
      optimization_guide::features::kOptimizationGuideMetadataValidation));
  web_state->RemoveObserver(this);
}

void OptimizationGuideValidationTabHelper::
    OnMetadataFetchValidationDecisionReceived(
        const GURL& url,
        optimization_guide::OptimizationGuideDecision decision,
        const optimization_guide::OptimizationMetadata& metadata) {
  DCHECK(base::FeatureList::IsEnabled(
      optimization_guide::features::kOptimizationGuideMetadataValidation));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue)
    return;

  auto expected_metadata =
      optimization_guide::features::ShouldMetadataValidationFetchHostKeyed()
          ? url.host()
          : url.spec();

  auto string_metadata =
      metadata.ParsedMetadata<optimization_guide::proto::StringValue>();
  base::UmaHistogramBoolean(
      "OptimizationGuide.MetadataFetchValidation.Result",
      string_metadata && string_metadata->value() == expected_metadata);
}

WEB_STATE_USER_DATA_KEY_IMPL(OptimizationGuideValidationTabHelper)
