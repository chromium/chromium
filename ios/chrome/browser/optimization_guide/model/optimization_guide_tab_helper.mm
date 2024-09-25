// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_tab_helper.h"

#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "url/gurl.h"

IOSOptimizationGuideNavigationData::IOSOptimizationGuideNavigationData(
    int64_t navigation_id)
    : OptimizationGuideNavigationData(
          navigation_id,
          /*navigation_start=*/base::TimeTicks::Now()) {}

IOSOptimizationGuideNavigationData::~IOSOptimizationGuideNavigationData() =
    default;

void IOSOptimizationGuideNavigationData::NotifyNavigationStart(
    const GURL& url) {
  redirect_chain_.clear();
  redirect_chain_.push_back(url);
  set_navigation_url(url);
}

void IOSOptimizationGuideNavigationData::NotifyNavigationRedirect(
    const GURL& url) {
  redirect_chain_.push_back(url);
  set_navigation_url(url);
}

OptimizationGuideTabHelper::OptimizationGuideTabHelper(web::WebState* web_state)
    : optimization_guide_service_(
          OptimizationGuideServiceFactory::GetForProfile(
              ProfileIOS::FromBrowserState(web_state->GetBrowserState()))) {
  DCHECK(web_state);
  if (optimization_guide_service_)
    web_state->AddObserver(this);
}

OptimizationGuideTabHelper::~OptimizationGuideTabHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OptimizationGuideTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!navigation_context->GetUrl().SchemeIsHTTPOrHTTPS())
    return;

  IOSOptimizationGuideNavigationData* navigation_data =
      GetOrCreateOptimizationGuideNavigationData(navigation_context);
  navigation_data->NotifyNavigationStart(navigation_context->GetUrl());
  optimization_guide_service_->OnNavigationStartOrRedirect(navigation_data);
}

void OptimizationGuideTabHelper::DidRedirectNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!navigation_context->GetUrl().SchemeIsHTTPOrHTTPS())
    return;

  IOSOptimizationGuideNavigationData* navigation_data =
      GetOrCreateOptimizationGuideNavigationData(navigation_context);
  navigation_data->NotifyNavigationRedirect(navigation_context->GetUrl());
  optimization_guide_service_->OnNavigationStartOrRedirect(navigation_data);
}

void OptimizationGuideTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!navigation_context->GetUrl().SchemeIsHTTPOrHTTPS())
    return;

  IOSOptimizationGuideNavigationData* navigation_data =
      GetOrCreateOptimizationGuideNavigationData(navigation_context);

  // Delete Optimization Guide information later, so that other
  // DidFinishNavigation methods can reliably use
  // GetOrCreateOptimizationGuideNavigationData regardless of order of
  // TabHelpers. Note that a lot of Navigations (sub-frames, same document,
  // non-committed, etc.) might not have navigation data associated with them,
  // but we reduce likelihood of future leaks by always trying to remove the
  // data.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OptimizationGuideTabHelper::NotifyNavigationFinish,
                     weak_factory_.GetWeakPtr(),
                     navigation_context->GetNavigationId(),
                     navigation_data->redirect_chain()));
}

void OptimizationGuideTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

IOSOptimizationGuideNavigationData*
OptimizationGuideTabHelper::GetOrCreateOptimizationGuideNavigationData(
    web::NavigationContext* navigation_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t navigation_id = navigation_context->GetNavigationId();
  if (inflight_optimization_guide_navigation_datas_.find(navigation_id) ==
      inflight_optimization_guide_navigation_datas_.end()) {
    // We do not have one already - create one.
    inflight_optimization_guide_navigation_datas_[navigation_id] =
        std::make_unique<IOSOptimizationGuideNavigationData>(navigation_id);
  }

  DCHECK(inflight_optimization_guide_navigation_datas_.find(navigation_id) !=
         inflight_optimization_guide_navigation_datas_.end());
  return inflight_optimization_guide_navigation_datas_.find(navigation_id)
      ->second.get();
}

void OptimizationGuideTabHelper::NotifyNavigationFinish(
    int64_t navigation_id,
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto nav_data_iter =
      inflight_optimization_guide_navigation_datas_.find(navigation_id);
  if (nav_data_iter == inflight_optimization_guide_navigation_datas_.end())
    return;

  optimization_guide_service_->OnNavigationFinish(navigation_redirect_chain);

  // We keep the last navigation data around to keep track of events happening
  // for the navigation that can happen after commit, such as a fetch for the
  // navigation successfully completing (which is not guaranteed to come back
  // before commit, if at all).
  last_navigation_data_ = std::move(nav_data_iter->second);
  inflight_optimization_guide_navigation_datas_.erase(navigation_id);
}

WEB_STATE_USER_DATA_KEY_IMPL(OptimizationGuideTabHelper)
