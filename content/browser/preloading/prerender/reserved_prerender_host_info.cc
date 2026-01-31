// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/reserved_prerender_host_info.h"

#include "base/check.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/prerender_host_id.h"

namespace content {

ReservedPrerenderHostInfo::ReservedPrerenderHostInfo(
    PrerenderHostId prerender_host_id,
    PreloadingTriggerType trigger_type,
    std::string embedder_histogram_suffix,
    bool is_prerender_host_reused)
    : prerender_host_id(prerender_host_id),
      trigger_type(trigger_type),
      embedder_histogram_suffix(std::move(embedder_histogram_suffix)),
      is_prerender_host_reused(is_prerender_host_reused) {
  CHECK(prerender_host_id);
}

}  // namespace content
