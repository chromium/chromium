// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/model/web_payments_metrics_recorder.h"

#import <string_view>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/hash/hash.h"
#import "base/strings/string_view_util.h"
#import "components/payments/core/features.h"
#import "components/payments/core/web_payments_telemetry.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ui/base/page_transition_types.h"

namespace {

bool IsFormSubmissionPost(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  return [request.HTTPMethod isEqualToString:@"POST"] &&
         ui::PageTransitionCoreTypeIs(request_info.transition_type,
                                      ui::PAGE_TRANSITION_FORM_SUBMIT);
}

}  // namespace

WebPaymentsMetricsRecorder::WebPaymentsMetricsRecorder(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

WebPaymentsMetricsRecorder::~WebPaymentsMetricsRecorder() = default;

void WebPaymentsMetricsRecorder::ShouldAllowRequest(
    NSURLRequest* request,
    RequestInfo request_info,
    PolicyDecisionCallback callback) {
  NSData* body = request.HTTPBody;
  if (base::FeatureList::IsEnabled(
          payments::features::kThreeDSecureTelemetryForIos) &&
      body.length && IsFormSubmissionPost(request, request_info)) {
    std::string_view form_data =
        base::as_string_view(base::apple::NSDataToSpan(body));

    // 307/308 redirects will refire this method with identical `form_data`. To
    // avoid recording duplicate metrics on redirect, we don't emit when the
    // current and previous `form_data` hashes are equal.
    size_t form_data_hash = base::FastHash(form_data);
    if (form_data_hash != last_form_data_hash_) {
      last_form_data_hash_ = form_data_hash;
      payments::RecordThreeDSecureTelemetryFromFormData(
          form_data, ukm::GetSourceIdForWebStateDocument(web_state()));
    }
  }
  std::move(callback).Run(PolicyDecision::Allow());
}
