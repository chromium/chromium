// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_RESERVED_PRERENDER_HOST_INFO_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_RESERVED_PRERENDER_HOST_INFO_H_

#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/prerender_host_id.h"

namespace content {

// Information about a reserved prerender host. This is used to pass information
// about the reserved host including information required for metrics
// collection.
struct ReservedPrerenderHostInfo {
  ReservedPrerenderHostInfo(PrerenderHostId prerender_host_id,
                            PreloadingTriggerType trigger_type,
                            std::string embedder_histogram_suffix,
                            bool is_prerender_host_reused);

  PrerenderHostId prerender_host_id;
  PreloadingTriggerType trigger_type;
  std::string embedder_histogram_suffix;
  bool is_prerender_host_reused;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_RESERVED_PRERENDER_HOST_INFO_H_
