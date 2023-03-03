// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace content {
namespace content_index {

void RecordDisptachStatus(const std::string& phase,
                          blink::ServiceWorkerStatusCode status_code) {
  base::UmaHistogramEnumeration("ContentIndex.ContentDeleteEvent." + phase,
                                status_code);
}

}  // namespace content_index
}  // namespace content
