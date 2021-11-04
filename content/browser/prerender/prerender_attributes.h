// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_ATTRIBUTES_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_ATTRIBUTES_H_

#include "content/public/common/referrer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace content {

enum class PrerenderTriggerType {
  // https://wicg.github.io/nav-speculation/prerendering.html#speculation-rules
  kSpeculationRule,
};

// Records the basic attributes of a prerender request.
struct CONTENT_EXPORT PrerenderAttributes {
  PrerenderAttributes(const GURL& prerendering_url,
                      PrerenderTriggerType trigger_type,
                      Referrer referrer,
                      const url::Origin& initiator_origin,
                      const GURL& initiator_url,
                      int initiator_process_id,
                      const blink::LocalFrameToken& initiator_frame_token,
                      ukm::SourceId initiator_ukm_id);
  ~PrerenderAttributes();
  PrerenderAttributes(const PrerenderAttributes&);
  PrerenderAttributes& operator=(const PrerenderAttributes&) = delete;
  PrerenderAttributes(PrerenderAttributes&&);
  PrerenderAttributes& operator=(PrerenderAttributes&&) = delete;

  GURL prerendering_url;
  PrerenderTriggerType trigger_type;
  Referrer referrer;
  url::Origin initiator_origin;
  GURL initiator_url;
  int initiator_process_id;
  blink::LocalFrameToken initiator_frame_token;
  ukm::SourceId initiator_ukm_id;

  // Serialises this struct into a trace.
  void WriteIntoTrace(perfetto::TracedValue trace_context) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_ATTRIBUTES_H_
