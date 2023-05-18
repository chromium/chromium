// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALLABILITY_PROMOTER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALLABILITY_PROMOTER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace webapps {

class SiteMetricsCollectionTask;

// This class is used to measure metrics after page load and trigger a ML model
// to promote installability of a site.
//
// Browsertests are located in
// chrome/browser/web_applications/ml_promotion_browsertest.cc
class MLInstallabilityPromoter
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MLInstallabilityPromoter> {
 public:
  ~MLInstallabilityPromoter() override;

  MLInstallabilityPromoter(const MLInstallabilityPromoter&) = delete;
  MLInstallabilityPromoter& operator=(const MLInstallabilityPromoter&) = delete;

  // This is technically where the UKMs will be measured.
  void StartGatheringMetricsForFrameUrl(const GURL& url);

 private:
  explicit MLInstallabilityPromoter(content::WebContents* web_contents);
  friend class content::WebContentsUserData<MLInstallabilityPromoter>;

  void OnMetricsTaskFinished();

  std::unique_ptr<SiteMetricsCollectionTask> current_collection_task_;

  base::WeakPtrFactory<MLInstallabilityPromoter> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALLABILITY_PROMOTER_H_
