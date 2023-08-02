// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/canonical_url_retriever.h"

#import "base/functional/bind.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/ui_metrics/canonical_url_share_metrics_types.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {

// Logs `result` in the Mobile.CanonicalURLResult histogram.
void LogCanonicalUrlResultHistogram(ui_metrics::CanonicalURLResult result) {
  UMA_HISTOGRAM_ENUMERATION(ui_metrics::kCanonicalURLResultHistogram, result,
                            ui_metrics::CANONICAL_URL_RESULT_COUNT);
}

// Converts a `value` to a GURL. Returns an empty GURL if `value` is not a valid
// HTTPS URL, indicating that retrieval failed. This function also handles
// logging retrieval failures and success.
GURL UrlFromValue(const GURL& visible_url, const base::Value* value) {
  GURL canonical_url;
  bool canonical_url_found = false;

  if (value && value->is_string() && !value->GetString().empty()) {
    canonical_url = GURL(value->GetString());

    // This variable is required for metrics collection in order to distinguish
    // between the no canonical URL found and the invalid canonical URL found
    // cases. The `canonical_url` GURL cannot be relied upon to distinguish
    // between these cases because GURLs created with invalid URLs can be
    // constructed as empty GURLs.
    canonical_url_found = true;
  }

  if (!canonical_url_found) {
    // Log result if no canonical URL is found.
    LogCanonicalUrlResultHistogram(ui_metrics::FAILED_NO_CANONICAL_URL_DEFINED);
  } else if (!canonical_url.is_valid()) {
    // Log result if an invalid canonical URL is found.
    LogCanonicalUrlResultHistogram(ui_metrics::FAILED_CANONICAL_URL_INVALID);
  } else {
    // If the canonical URL is valid, then the retrieval was successful,
    // and the success can be logged.
    LogCanonicalUrlResultHistogram(
        !canonical_url.SchemeIsCryptographic()
            ? ui_metrics::SUCCESS_CANONICAL_URL_NOT_HTTPS
        : visible_url == canonical_url
            ? ui_metrics::SUCCESS_CANONICAL_URL_SAME_AS_VISIBLE
            : ui_metrics::SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE);
  }

  return canonical_url.is_valid() ? canonical_url : GURL();
}

}  // namespace

namespace activity_services {

const char16_t kCanonicalURLScript[] =
    u"(function() {"
    u"  var linkNode = document.querySelector(\"link[rel='canonical']\");"
    u"  return linkNode ? linkNode.getAttribute(\"href\") : \"\";"
    u"})()";

void RetrieveCanonicalUrl(web::WebState* web_state,
                          CanonicalUrlRetrievedCallback completion) {
  // Do not use the canonical URL if the page is not secured with HTTPS.
  const GURL visible_url = web_state->GetVisibleURL();
  if (!visible_url.SchemeIsCryptographic()) {
    LogCanonicalUrlResultHistogram(ui_metrics::FAILED_VISIBLE_URL_NOT_HTTPS);
    std::move(completion).Run(GURL());
    return;
  }

  web::WebFrame* main_frame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!main_frame) {
    std::move(completion).Run(GURL());
    return;
  }

  main_frame->ExecuteJavaScript(
      kCanonicalURLScript,
      base::BindOnce(&UrlFromValue, visible_url).Then(std::move(completion)));
}
}  // namespace activity_services
