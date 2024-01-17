// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_METRICS_SITE_MANIFEST_METRICS_TASK_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_METRICS_SITE_MANIFEST_METRICS_TASK_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace content {
class WebContents;
}  // namespace content

namespace webapps {

// This class is responsible for gathing metrics for the given site on the given
// web contents to emit the "Site.Manifest" UKM events. To stop collection,
// simply destroy this object.
//
// Invariants:
// - `WebContents` is alive during its lifetime of this class.
// - `WebContents` is not navigated during the lifetime of this class and the
//   metrics gathered from it are valid for the `GetLastCommittedURL()`
//   retrieved on construction of this class.
//
// Browsertests are located in
// chrome/browser/web_applications/ml_promotion_browsertest.cc

class InstallableManager;
struct InstallableData;

class SiteManifestMetricsTask {
 public:
  using ResultCallback = base::OnceCallback<void(blink::mojom::ManifestPtr)>;
  ~SiteManifestMetricsTask();

  // Create the task, start collecting the manifest from the web_contents and
  // pass the task back to the MLInstallabilityPromoter. If the result of
  // fetching the manifest is that the URL changed, then we return an empty
  // manifest and the MLInstallabilityPromoter handles manifest URL changes.
  // If there is no manifest present, the callback runs with a nullptr.
  static std::unique_ptr<SiteManifestMetricsTask> CreateAndStart(
      content::WebContents& web_contents,
      ResultCallback collect_data_and_self_destruct_callback);

 private:
  SiteManifestMetricsTask(
      content::WebContents& web_contents,
      ResultCallback collect_data_and_self_destruct_callback);

  void Start();
  void OnDidFetchManifest(const InstallableData& data);

  blink::mojom::ManifestPtr manifest_;
  const raw_ref<content::WebContents> web_contents_;
  ResultCallback collect_data_and_self_destruct_callback_;
  raw_ptr<InstallableManager> manager_;

  base::WeakPtrFactory<SiteManifestMetricsTask> weak_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_METRICS_SITE_MANIFEST_METRICS_TASK_H_
