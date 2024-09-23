// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "components/optimization_guide/core/optimization_guide_decision.h"
#import "components/page_info/core/about_this_site_service.h"
#import "components/page_info/core/proto/about_this_site_metadata.pb.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol AboutThisSiteTabHelperDelegate;

// This WebStateObserver fetches AboutThisSite hints from OptimizationGuide.
class AboutThisSiteTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<AboutThisSiteTabHelper>,
      public page_info::AboutThisSiteService::TabHelper {
 public:
  AboutThisSiteTabHelper(const AboutThisSiteTabHelper&) = delete;
  AboutThisSiteTabHelper& operator=(const AboutThisSiteTabHelper&) = delete;

  ~AboutThisSiteTabHelper() override;

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Get the AboutThisSiteMetadata for this page if available.
  page_info::AboutThisSiteService::DecisionAndMetadata
  GetAboutThisSiteMetadata() const override;

 private:
  explicit AboutThisSiteTabHelper(
      web::WebState* web_state,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  friend class web::WebStateUserData<AboutThisSiteTabHelper>;

  // Callback from OptimizationGuide with the AboutThisSite data proto.
  void OnOptimizationGuideDecision(
      const GURL& main_frame_url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Whether the OptimizationGuide was able to fetch the AboutThisSite info for
  // the `web_state_`.
  optimization_guide::OptimizationGuideDecision decision_;

  // The AboutThisSite data returned by the OptimizationGuide. Empty if
  // `decision_` is not `kTrue`.
  std::optional<page_info::proto::AboutThisSiteMetadata>
      about_this_site_metadata_;

  // The WebState with which this object is associated.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // The OptimizationGuideDecider with which the `web_state_` of this object is
  // associated.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  // The URL from the previous successful main frame navigation. This will be
  // empty if this is the first navigation for this tab or post-restart.
  GURL previous_main_frame_url_;

  base::WeakPtrFactory<AboutThisSiteTabHelper> weak_ptr_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_TAB_HELPER_H_
