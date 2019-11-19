// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace content {
namespace content_index {

void RecordDatabaseOperationStatus(const std::string& name,
                                   blink::ServiceWorkerStatusCode status) {
  base::UmaHistogramEnumeration("ContentIndex.Database." + name, status);
}

void RecordDisptachStatus(const std::string& phase,
                          blink::ServiceWorkerStatusCode status_code) {
  base::UmaHistogramEnumeration("ContentIndex.ContentDeleteEvent." + phase,
                                status_code);
}

void RecordRegistrationBlocked(blink::mojom::ContentCategory category) {
  base::UmaHistogramEnumeration("ContentIndex.RegistrationBlocked", category);
}

}  // namespace content_index
}  // namespace content
