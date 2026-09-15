// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_tab_helper.h"

#import <utility>

#import "base/feature_list.h"
#import "base/memory/raw_ref.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"
#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_encryption_protocol_provider.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace enterprise_reporting {

namespace {

bool ShouldRecordNavigation(const web::NavigationContext& navigation_context) {
  return navigation_context.HasCommitted() && !navigation_context.GetError() &&
         !navigation_context.IsSameDocument();
}

class SaasUsageNavigationDataDelegateIOS
    : public SaasUsageReportingController::NavigationDataDelegate {
 public:
  explicit SaasUsageNavigationDataDelegateIOS(
      const web::NavigationContext& navigation_context)
      : navigation_context_(navigation_context) {}

  ~SaasUsageNavigationDataDelegateIOS() override = default;

  GURL GetUrl() const override { return navigation_context_->GetUrl(); }

  void GetEncryptionProtocol(
      EncryptionProtocolCallback callback) const override {
    SaasUsageEncryptionProtocolProvider::GetInstance().GetEncryptionProtocol(
        GetUrl(), std::move(callback));
  }

 private:
  const raw_ref<const web::NavigationContext> navigation_context_;
};

}  // namespace

SaasUsageTabHelper::SaasUsageTabHelper(web::WebState* web_state,
                                       SaasUsageReportingController* controller)
    : controller_(raw_ref<SaasUsageReportingController>::from_ptr(controller)) {
  CHECK(web_state);
  CHECK(base::FeatureList::IsEnabled(kSaasUsageReporting));
  observation_.Observe(web_state);
}

SaasUsageTabHelper::~SaasUsageTabHelper() = default;

void SaasUsageTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  CHECK(navigation_context);
  if (ShouldRecordNavigation(*navigation_context)) {
    controller_->RecordNavigation(
        SaasUsageNavigationDataDelegateIOS(*navigation_context));
  }
}

void SaasUsageTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK(web_state);
  observation_.Reset();
}

}  // namespace enterprise_reporting
