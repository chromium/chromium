// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/341324165): Fix and remove.
#pragma allow_unsafe_buffers
#endif

#include "content/web_test/browser/web_test_origin_trial_throttle.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const char kThrottleName[] = "WebTestOriginTrialThrottle";
const char kWebTestOriginTrialHeaderName[] = "X-Web-Test-Enabled-Origin-Trials";

}  // namespace

WebTestOriginTrialThrottle::WebTestOriginTrialThrottle(
    NavigationHandle* navigation_handle,
    OriginTrialsControllerDelegate* delegate)
    : NavigationThrottle(navigation_handle),
      origin_trials_controller_delegate_(delegate) {}

NavigationThrottle::ThrottleCheckResult
WebTestOriginTrialThrottle::WillStartRequest() {
  SetHeaderForRequest();
  return NavigationThrottle::ThrottleAction::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
WebTestOriginTrialThrottle::WillRedirectRequest() {
  SetHeaderForRequest();
  return NavigationThrottle::ThrottleAction::PROCEED;
}

void WebTestOriginTrialThrottle::SetHeaderForRequest() {
  GURL request_url = navigation_handle()->GetURL();
  url::Origin origin = url::Origin::CreateFromNormalizedTuple(
      request_url.scheme(), request_url.host(), request_url.EffectiveIntPort());

  base::flat_set<std::string> trials;
  if (!origin.opaque()) {
    url::Origin partition_origin = origin;
    if (navigation_handle()->GetParentFrameOrOuterDocument()) {
      partition_origin = navigation_handle()
                             ->GetParentFrameOrOuterDocument()
                             ->GetOutermostMainFrame()
                             ->GetLastCommittedOrigin();
    }
    trials = origin_trials_controller_delegate_->GetPersistedTrialsForOrigin(
        origin, partition_origin, base::Time::Now());
  }
  std::string header_value = base::JoinString(
      base::span<std::string>(trials.begin(), trials.end()), ", ");
  if (!header_value.empty()) {
    navigation_handle()->SetRequestHeader(kWebTestOriginTrialHeaderName,
                                          header_value);
  }
}

const char* WebTestOriginTrialThrottle::GetNameForLogging() {
  return kThrottleName;
}

}  // namespace content
