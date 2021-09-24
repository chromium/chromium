// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_ATTRIBUTES_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_ATTRIBUTES_H_

#include "content/public/common/referrer.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace content {

enum class PrerenderTriggerType {
  // https://jeremyroman.github.io/alternate-loading-modes/#speculation-rules
  kSpeculationRule,
};

// Records the basic attributes of a prerender request.
struct PrerenderAttributes {
  GURL url;
  PrerenderTriggerType trigger_type;
  Referrer referrer;

  // Serialises this struct into a trace.
  void WriteIntoTrace(perfetto::TracedValue trace_context) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_ATTRIBUTES_H_
