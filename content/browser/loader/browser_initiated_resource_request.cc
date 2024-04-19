// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/browser_initiated_resource_request.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace content {

void UpdateAdditionalHeadersForBrowserInitiatedRequest(
    net::HttpRequestHeaders* headers,
    BrowserContext* browser_context,
    bool should_update_existing_headers,
    const blink::RendererPreferences& renderer_preferences,
    bool is_for_worker_script) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Set the DoNotTrack header if appropriate.
  // https://w3c.github.io/dnt/drafts/tracking-dnt.html#expression-format
  if (renderer_preferences.enable_do_not_track) {
    if (should_update_existing_headers) {
      headers->RemoveHeader(blink::kDoNotTrackHeader);
    }
    headers->SetHeaderIfMissing(blink::kDoNotTrackHeader, "1");
  }

  // TODO(crbug.com/40833603): WARNING: This bypasses the permissions policy.
  // Unfortunately, workers lack a permissions policy and to derive proper hints
  // https://github.com/w3c/webappsec-permissions-policy/issues/207.
  // Save-Data was previously included in hints for workers, thus we cannot
  // remove it for the time being. If you're reading this, consider building
  // permissions policies for workers and/or deprecating this inclusion.
  if (is_for_worker_script &&
      GetContentClient()->browser()->IsDataSaverEnabled(browser_context)) {
    if (should_update_existing_headers) {
      headers->RemoveHeader("Save-Data");
    }
    headers->SetHeaderIfMissing("Save-Data", "on");
  }
}

}  // namespace content
