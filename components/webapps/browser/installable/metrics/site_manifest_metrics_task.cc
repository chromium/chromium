// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/metrics/site_manifest_metrics_task.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/web_contents.h"

namespace webapps {

SiteManifestMetricsTask::~SiteManifestMetricsTask() = default;

// static
std::unique_ptr<SiteManifestMetricsTask>
SiteManifestMetricsTask::CreateAndStart(
    content::WebContents& web_contents,
    ResultCallback collect_data_and_self_destruct_callback) {
  std::unique_ptr<SiteManifestMetricsTask> result =
      base::WrapUnique(new SiteManifestMetricsTask(
          web_contents, std::move(collect_data_and_self_destruct_callback)));
  result->Start();
  return result;
}

SiteManifestMetricsTask::SiteManifestMetricsTask(
    content::WebContents& web_contents,
    ResultCallback collect_data_and_self_destruct_callback)
    : web_contents_(web_contents),
      collect_data_and_self_destruct_callback_(
          std::move(collect_data_and_self_destruct_callback)),
      manager_(InstallableManager::FromWebContents(&web_contents)) {
  CHECK(manager_);
}

void SiteManifestMetricsTask::Start() {
  InstallableParams params;
  params.check_eligibility = true;
  manager_->GetData(params,
                    base::BindOnce(&SiteManifestMetricsTask::OnDidFetchManifest,
                                   weak_factory_.GetWeakPtr()));
}

void SiteManifestMetricsTask::OnDidFetchManifest(const InstallableData& data) {
  if (!data.manifest_url->is_empty() && data.manifest) {
    manifest_ = *data.manifest;
  } else {
    // When no developer-specified manifest exists on the page (or fetching/
    // parsing failed), `data.manifest` is the default manifest with an empty
    // `manifest_url`. Use `std::nullopt` to represent the absence of a
    // developer-specified manifest.
    manifest_ = std::nullopt;
  }

  CHECK(collect_data_and_self_destruct_callback_);
  std::move(collect_data_and_self_destruct_callback_).Run(std::move(manifest_));
}

}  // namespace webapps
