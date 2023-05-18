// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/ml_installability_promoter.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/site_metrics_collection_task.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace webapps {

MLInstallabilityPromoter::~MLInstallabilityPromoter() = default;

void MLInstallabilityPromoter::StartGatheringMetricsForFrameUrl(
    const GURL& url) {
  // TODO(b/279521783): Start gathering UKMs from here. Assign the input url to
  // frame_url if a ML model is not already running.

  CHECK(web_contents());
  if (base::FeatureList::IsEnabled(features::kWebAppsMlUkmCollection)) {
    // Note: the destruct of any previous task is intended here.
    current_collection_task_ = SiteMetricsCollectionTask::CreateAndStart(
        *web_contents(), /*maximum_wait_time=*/base::Seconds(3),
        base::BindOnce(&MLInstallabilityPromoter::OnMetricsTaskFinished,
                       weak_factory_.GetWeakPtr()));
  }
}

void MLInstallabilityPromoter::OnMetricsTaskFinished() {
  current_collection_task_.reset();
}

MLInstallabilityPromoter::MLInstallabilityPromoter(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<MLInstallabilityPromoter>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MLInstallabilityPromoter);

}  // namespace webapps
