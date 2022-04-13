// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FENCED_FRAME_REPORTING_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FENCED_FRAME_REPORTING_H_

#include "base/containers/flat_map.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

namespace blink {

// See `blink::mojom::FencedFrameReporting`.

struct WebFencedFrameReporting {
  base::flat_map<mojom::ReportingDestination, base::flat_map<WebString, WebURL>>
      metadata;
};

}  // namespace blink

#endif
