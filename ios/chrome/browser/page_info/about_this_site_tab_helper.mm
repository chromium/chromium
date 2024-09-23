// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/about_this_site_tab_helper.h"

#import "components/optimization_guide/core/optimization_guide_decider.h"
#import "components/optimization_guide/core/optimization_metadata.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

AboutThisSiteTabHelper::AboutThisSiteTabHelper(
    web::WebState* web_state,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : web_state_(web_state),
      optimization_guide_decider_(optimization_guide_decider) {
  CHECK(web_state);
  CHECK(optimization_guide_decider);
  web_state->AddObserver(this);
}

AboutThisSiteTabHelper::~AboutThisSiteTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
  optimization_guide_decider_ = nullptr;
  about_this_site_metadata_.reset();
}

#pragma mark - WebStateObserver

void AboutThisSiteTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  const GURL& url = navigation_context->GetUrl();
  if (previous_main_frame_url_ == url) {
    return;
  }

  decision_ = optimization_guide::OptimizationGuideDecision::kUnknown;
  about_this_site_metadata_.reset();
  previous_main_frame_url_ = url;

  if (url.SchemeIsHTTPOrHTTPS()) {
    optimization_guide_decider_->CanApplyOptimization(
        url, optimization_guide::proto::ABOUT_THIS_SITE,
        base::BindOnce(&AboutThisSiteTabHelper::OnOptimizationGuideDecision,
                       weak_ptr_factory_.GetWeakPtr(), url));
  }
}

void AboutThisSiteTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
  optimization_guide_decider_ = nullptr;
  about_this_site_metadata_.reset();
}

#pragma mark - Public

page_info::AboutThisSiteService::DecisionAndMetadata
AboutThisSiteTabHelper::GetAboutThisSiteMetadata() const {
  return {decision_, about_this_site_metadata_};
}

#pragma mark - Private

void AboutThisSiteTabHelper::OnOptimizationGuideDecision(
    const GURL& main_frame_url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // Navigated away.
  if (previous_main_frame_url_ != main_frame_url) {
    return;
  }

  decision_ = decision;
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }
  about_this_site_metadata_ =
      metadata.ParsedMetadata<page_info::proto::AboutThisSiteMetadata>();
}

WEB_STATE_USER_DATA_KEY_IMPL(AboutThisSiteTabHelper)
