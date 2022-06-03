// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/browser_initiated_resource_request.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace content {

void UpdateAdditionalHeadersForBrowserInitiatedRequest(
    net::HttpRequestHeaders* headers,
    BrowserContext* browser_context,
    bool should_update_existing_headers,
    const blink::RendererPreferences& renderer_preferences) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Set the DoNotTrack header if appropriate.
  // https://w3c.github.io/dnt/drafts/tracking-dnt.html#expression-format
  if (renderer_preferences.enable_do_not_track) {
    if (should_update_existing_headers) {
      headers->RemoveHeader(blink::kDoNotTrackHeader);
    }
    headers->SetHeaderIfMissing(blink::kDoNotTrackHeader, "1");
  }

  // Set the Save-Data header if appropriate.
  // https://tools.ietf.org/html/draft-grigorik-http-client-hints-03#section-7
  if (GetContentClient()->browser()->IsDataSaverEnabled(browser_context) &&
      !base::GetFieldTrialParamByFeatureAsBool(features::kDataSaverHoldback,
                                               "holdback_web", false)) {
    if (should_update_existing_headers) {
      headers->RemoveHeader("Save-Data");
    }
    headers->SetHeaderIfMissing("Save-Data", "on");
  }
}

}  // namespace content
