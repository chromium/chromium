// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "third_party/blink/renderer/platform/graphics/viz_util.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

namespace {

std::atomic<uint32_t> g_next_bundle_id{1};

}  // namespace

viz::FrameSinkBundleId GenerateFrameSinkBundleId(uint32_t client_id) {
  return viz::FrameSinkBundleId(
      client_id, g_next_bundle_id.fetch_add(1, std::memory_order_relaxed));
}

}  // namespace blink
