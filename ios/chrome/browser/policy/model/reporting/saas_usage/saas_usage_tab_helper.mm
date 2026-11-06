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
  explicit SaasUsageNavigationDataDelegateIOS(const GURL& url) : url_(url) {}

  ~SaasUsageNavigationDataDelegateIOS() override = default;

  GURL GetUrl() const override { return url_; }

  void GetEncryptionProtocol(
      EncryptionProtocolCallback callback) const override {
    SaasUsageEncryptionProtocolProvider::GetInstance().GetEncryptionProtocol(
        url_, std::move(callback));
  }

 private:
  const GURL url_;
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

void SaasUsageTabHelper::SetDelayNavigationRecording(bool delay_recording) {
  delay_recording_ = delay_recording;
  if (delay_recording_) {
    return;
  }

  for (const GURL& url : std::exchange(recorded_urls_, {})) {
    RecordUrl(url);
  }
}

void SaasUsageTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  CHECK(navigation_context);
  if (!ShouldRecordNavigation(*navigation_context)) {
    return;
  }

  if (delay_recording_) {
    recorded_urls_.push_back(navigation_context->GetUrl());
    return;
  }

  RecordUrl(navigation_context->GetUrl());
}

void SaasUsageTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK(web_state);
  observation_.Reset();
}

void SaasUsageTabHelper::RecordUrl(const GURL& url) {
  controller_->RecordNavigation(SaasUsageNavigationDataDelegateIOS(url));
}

}  // namespace enterprise_reporting
