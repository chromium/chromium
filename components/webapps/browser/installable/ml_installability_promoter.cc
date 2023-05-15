// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/ml_installability_promoter.h"

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
}

MLInstallabilityPromoter::MLInstallabilityPromoter(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<MLInstallabilityPromoter>(*web_contents),
      frame_url_(web_contents->GetLastCommittedURL()) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MLInstallabilityPromoter);

}  // namespace webapps
