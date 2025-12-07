// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_MODEL_TAB_ORGANIZATION_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_MODEL_TAB_ORGANIZATION_SERVICE_IMPL_H_

#import "ios/chrome/browser/optimization_guide/mojom/tab_organization_service.mojom.h"
#import "mojo/public/cpp/bindings/receiver.h"

class OptimizationGuideService;
class WebStateList;
@class TabOrganizationRequestWrapper;

namespace optimization_guide {
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace ai {

class TabOrganizationServiceImpl : public mojom::TabOrganizationService {
 public:
  explicit TabOrganizationServiceImpl(
      mojo::PendingReceiver<mojom::TabOrganizationService> receiver,
      WebStateList* web_state_list,
      bool start_on_device);
  ~TabOrganizationServiceImpl() override;
  TabOrganizationServiceImpl(const TabOrganizationServiceImpl&) = delete;
  TabOrganizationServiceImpl& operator=(const TabOrganizationServiceImpl&) =
      delete;

  // ai::mojom::TabOrganizationServiceImpl:
  void ExecuteGroupTabs(::mojo_base::ProtoWrapper request,
                        ExecuteGroupTabsCallback callback) override;

 private:
  // Handles the response for a tab organization query.
  std::string OnGroupTabsResponse(
      optimization_guide::OptimizationGuideModelExecutionResult result);

  // Service used to execute LLM queries.
  raw_ptr<OptimizationGuideService> service_;

  // Receiver throughout the TabOrganizationServiceImpl lifecycle.
  mojo::Receiver<mojom::TabOrganizationService> receiver_;

  // List of web states used for tab organization.
  raw_ptr<WebStateList> web_state_list_;
};

}  // namespace ai
#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_MODEL_TAB_ORGANIZATION_SERVICE_IMPL_H_
