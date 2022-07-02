// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_data_impl.h"

#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_prediction.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

// static
PreloadingData* PreloadingData::GetOrCreateForWebContents(
    WebContents* web_contents) {
  return PreloadingDataImpl::GetOrCreateForWebContents(web_contents);
}

// static
PreloadingDataImpl* PreloadingDataImpl::GetOrCreateForWebContents(
    WebContents* web_contents) {
  auto* preloading_impl = PreloadingDataImpl::FromWebContents(web_contents);
  if (!preloading_impl)
    PreloadingDataImpl::CreateForWebContents(web_contents);

  return PreloadingDataImpl::FromWebContents(web_contents);
}

PreloadingAttempt* PreloadingDataImpl::AddPreloadingAttempt(
    PreloadingPredictor predictor,
    PreloadingType preloading_type,
    PreloadingURLMatchCallback url_match_predicate) {
  // We want to log the metrics for user visible primary pages to measure the
  // impact of PreloadingAttempt on the page user is viewing.
  // TODO(crbug.com/1330783): Extend this for non-primary page and inner
  // WebContents preloading attempts.
  ukm::SourceId triggered_primary_page_source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  auto attempt = std::make_unique<PreloadingAttemptImpl>(
      predictor, preloading_type, triggered_primary_page_source_id,
      std::move(url_match_predicate));
  preloading_attempts_.push_back(std::move(attempt));

  return preloading_attempts_.back().get();
}

void PreloadingDataImpl::AddPreloadingPrediction(
    PreloadingPredictor predictor,
    int64_t confidence,
    PreloadingURLMatchCallback url_match_predicate) {
  // Cross-check that we set confidence percentage in the limits.
  DCHECK(confidence >= 0 && confidence <= 100);

  // We want to log the metrics for user visible primary pages to measure the
  // impact of PreloadingPredictions on the page user is viewing.
  // TODO(crbug.com/1330783): Extend this for non-primary page and inner
  // WebContents preloading predictions.
  ukm::SourceId triggered_primary_page_source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  auto prediction = std::make_unique<PreloadingPrediction>(
      predictor, confidence, triggered_primary_page_source_id,
      std::move(url_match_predicate));
  preloading_predictions_.push_back(std::move(prediction));
}

PreloadingDataImpl::PreloadingDataImpl(WebContents* web_contents)
    : WebContentsUserData<PreloadingDataImpl>(*web_contents),
      WebContentsObserver(web_contents) {}

PreloadingDataImpl::~PreloadingDataImpl() = default;

void PreloadingDataImpl::PrimaryPageChanged(Page& page) {
  ukm::SourceId navigated_page_source_id =
      page.GetMainDocument().GetPageUkmSourceId();
  GURL navigated_url = page.GetMainDocument().GetLastCommittedURL();

  // Log the UKMs also on navigation when the user ends up navigating. Please
  // note that we currently log the metrics on the primary page to analyze
  // preloading impact on user-visible primary pages.
  RecordUKMForPreloadingAttempts(navigated_page_source_id, navigated_url);
  RecordUKMForPreloadingPredictions(navigated_page_source_id, navigated_url);

  // Delete the user data after logging.
  web_contents()->RemoveUserData(UserDataKey());
}

void PreloadingDataImpl::WebContentsDestroyed() {
  // Log the UKMs also on WebContentsDestroyed event to avoid losing the data
  // in case the user doesn't end up navigating. When the WebContents is
  // destroyed before navigation, we pass ukm::kInvalidSourceId and empty URL to
  // avoid the UKM associated to wrong page.
  RecordUKMForPreloadingAttempts(ukm::kInvalidSourceId, GURL());
  RecordUKMForPreloadingPredictions(ukm::kInvalidSourceId, GURL());

  // Delete the user data after logging.
  web_contents()->RemoveUserData(UserDataKey());
}

void PreloadingDataImpl::RecordUKMForPreloadingAttempts(
    ukm::SourceId navigated_page_source_id,
    const GURL& navigated_url) {
  for (auto& attempt : preloading_attempts_) {
    attempt->RecordPreloadingAttemptUKMs(navigated_page_source_id,
                                         navigated_url);
  }

  // Clear all records once we record the UKMs.
  preloading_attempts_.clear();
}

void PreloadingDataImpl::RecordUKMForPreloadingPredictions(
    ukm::SourceId navigated_page_source_id,
    const GURL& navigated_url) {
  for (auto& prediction : preloading_predictions_) {
    prediction->RecordPreloadingPredictionUKMs(navigated_page_source_id,
                                               navigated_url);
  }

  // Clear all records once we record the UKMs.
  preloading_predictions_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreloadingDataImpl);

}  // namespace content
