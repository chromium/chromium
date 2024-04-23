// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_TAB_HELPER_H_

#import "base/containers/flat_map.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/optimization_guide/core/insertion_ordered_set.h"
#import "components/optimization_guide/core/optimization_guide_navigation_data.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class OptimizationGuideService;

// Contains the required pieces of navigation specific to IOS.
class IOSOptimizationGuideNavigationData
    : public OptimizationGuideNavigationData {
 public:
  explicit IOSOptimizationGuideNavigationData(int64_t navigation_id);
  ~IOSOptimizationGuideNavigationData();

  IOSOptimizationGuideNavigationData(
      const IOSOptimizationGuideNavigationData&) = delete;
  IOSOptimizationGuideNavigationData& operator=(
      const IOSOptimizationGuideNavigationData&) = delete;

  // Notifies the navigation started with `url` to update the redirect chain and
  // navigation data.
  void NotifyNavigationStart(const GURL& url);

  // Notifies the navigation redireced to `url` to update the redirect chain and
  // navigation data.
  void NotifyNavigationRedirect(const GURL& url);

  const std::vector<GURL>& redirect_chain() const { return redirect_chain_; }

 private:
  // The redirect chain of this navigation, including the starting URL, and all
  // its redirects.  The `navigation_context` does not provide a way to get
  // this directly, so its maintained here.
  std::vector<GURL> redirect_chain_;
};

// A tab helper that observes WebState and passes the navigations to the
// optimization guide service. This is a rough copy of the
// OptimizationGuideWebContentsObserver in //chrome/browser. It cannot be
// directly used due to the platform differences of the common data
// structures.
class OptimizationGuideTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<OptimizationGuideTabHelper> {
 public:
  ~OptimizationGuideTabHelper() override;

  OptimizationGuideTabHelper(const OptimizationGuideTabHelper&) = delete;
  OptimizationGuideTabHelper& operator=(const OptimizationGuideTabHelper&) =
      delete;

 private:
  friend class web::WebStateUserData<OptimizationGuideTabHelper>;

  explicit OptimizationGuideTabHelper(web::WebState* web_state);

  // WebStateObserver implementation:
  // These DidStart, DidRedirect, DidFinish navigation are called only for
  // main-frames, and never for subframes.
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidRedirectNavigation(
      web::WebState* web_state,
      web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Gets the IOSOptimizationGuideNavigationData associated with the
  // `navigation_context`. If one does not exist already, one will be created
  // for it.
  IOSOptimizationGuideNavigationData*
  GetOrCreateOptimizationGuideNavigationData(
      web::NavigationContext* navigation_context);

  // Notifies `optimization_guide_service_` that the navigation has finished.
  void NotifyNavigationFinish(
      int64_t navigation_id,
      const std::vector<GURL>& navigation_redirect_chain);

  // The data related to a given navigation ID.
  base::flat_map<int64_t, std::unique_ptr<IOSOptimizationGuideNavigationData>>
      inflight_optimization_guide_navigation_datas_;

  // The navigation data for the last completed navigation.
  // TODO(crbug.com/40194554): Clear the last navigation data when the tab gets
  // hidden and when Chrome app is backgrounded.
  std::unique_ptr<IOSOptimizationGuideNavigationData> last_navigation_data_;

  // Initialized in constructor. It may be null if the OptimizationGuideService
  // feature is not enabled.
  raw_ptr<OptimizationGuideService> optimization_guide_service_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OptimizationGuideTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_TAB_HELPER_H_
