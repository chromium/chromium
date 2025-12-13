// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/model/text_classifier_util.h"

#import "base/command_line.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/core/hints/optimization_guide_decision.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/text_selection/model/text_selection_util.h"

bool IsEntitySelectionAllowedForURL(web::WebState* web_state) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  CHECK(profile);
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  GURL url = web_state->GetLastCommittedURL();
  if (!url.is_valid()) {
    return false;
  }

  if (url.GetHost() ==
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kForceAllowDomainForEntitySelection)) {
    return true;
  }

  if (url.HostIsIPAddress()) {
    // IP address sites are often private sites that cannot be checked.
    return false;
  }
  return optimization_guide_service->CanApplyOptimization(
             url, optimization_guide::proto::TEXT_CLASSIFIER_ENTITY_DETECTION,
             /*optimization_metadata=*/nullptr) !=
         optimization_guide::OptimizationGuideDecision::kFalse;
}
