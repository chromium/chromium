// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_container.h"

#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"

namespace content {

PrefetchContainer::PrefetchContainer(
    const GlobalRenderFrameHostId& referring_render_frame_host_id,
    const GURL& url,
    const PrefetchType& prefetch_type)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      url_(url),
      prefetch_type_(prefetch_type) {}

PrefetchContainer::~PrefetchContainer() = default;

}  // namespace content
