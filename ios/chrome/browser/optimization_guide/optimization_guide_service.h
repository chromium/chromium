// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

namespace optimization_guide {
class TabUrlProvider;
class TopHostProvider;
class OptimizationGuideStore;
class HintsManager;
}  // namespace optimization_guide

class OptimizationGuideNavigationData;

namespace web {
class BrowserState;
class NavigationContext;
}  // namespace web

// A BrowserState keyed service that is used to own the underlying Optimization
// Guide components. This is a rough copy of the OptimizationGuideKeyedService
// in //chrome/browser that is used for non-iOS. It cannot be directly used due
// to the platform differences of the common data structures -
// NavigationContext vs NavigationHandle, BrowserState vs Profile, etc.
// TODO(crbug.com/1240907): Add support for clearing the hints when browsing
// data is cleared.
class OptimizationGuideService : public KeyedService {
 public:
  explicit OptimizationGuideService(web::BrowserState* browser_state);
  ~OptimizationGuideService() override;

  OptimizationGuideService(const OptimizationGuideService&) = delete;
  OptimizationGuideService& operator=(const OptimizationGuideService&) = delete;

  // Registers the optimization types that intend to be queried during the
  // session. It is expected for this to be called right after the browser has
  // been initialized.
  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types);

  // Returns whether |optimization_type| can be applied for |url|. This should
  // only be called for main frame navigations or future main frame navigations.
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata);

  // Invokes |callback| with the decision for the URL contained in
  // |navigation_context| and |optimization_type|, when sufficient information
  // has been collected to make the decision. This should only be called for
  // main frame navigations.
  void CanApplyOptimizationAsync(
      web::NavigationContext* navigation_context,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback);

 private:
  friend class OptimizationGuideServiceTest;

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_data| has started or redirected.
  void OnNavigationStartOrRedirect(
      OptimizationGuideNavigationData* navigation_data);

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_redirect_chain| has finished.
  void OnNavigationFinish(const std::vector<GURL>& navigation_redirect_chain);

  // KeyedService implementation:
  void Shutdown() override;

  optimization_guide::HintsManager* GetHintsManager();

  // The store of hints.
  std::unique_ptr<optimization_guide::OptimizationGuideStore> hint_store_;

  // Manages the storing, loading, and fetching of hints.
  std::unique_ptr<optimization_guide::HintsManager> hints_manager_;

  // The top host provider to use for fetching information for the user's top
  // hosts. Will be null if the user has not consented to this type of browser
  // behavior.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  // The tab URL provider to use for fetching information for the user's active
  // tabs. Will be null if the user is off the record.
  std::unique_ptr<optimization_guide::TabUrlProvider> tab_url_provider_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
