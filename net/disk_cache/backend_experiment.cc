// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/backend_experiment.h"

#include "base/feature_list.h"
#include "net/base/features.h"

namespace disk_cache {

bool InBackendExperiment() {
  return base::FeatureList::IsEnabled(
      net::features::kDiskCacheBackendExperiment);
}

bool InSimpleBackendExperimentGroup() {
  return InBackendExperiment() && net::features::kDiskCacheBackendParam.Get() ==
                                      net::features::DiskCacheBackend::kSimple;
}

bool InBlockfileBackendExperimentGroup() {
  return InBackendExperiment() &&
         net::features::kDiskCacheBackendParam.Get() ==
             net::features::DiskCacheBackend::kBlockfile;
}

}  // namespace disk_cache
