// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_process_sandbox_support_impl_mac.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "content/public/child/child_thread.h"

namespace content {

WebSandboxSupportMac::WebSandboxSupportMac() {
  if (auto* thread = ChildThread::Get()) {
    thread->BindHostReceiver(sandbox_support_.BindNewPipeAndPassReceiver());
    sandbox_support_->GetSystemColors(base::BindOnce(
        &WebSandboxSupportMac::OnGotSystemColors, base::Unretained(this)));
  }
}

WebSandboxSupportMac::~WebSandboxSupportMac() = default;

SkColor WebSandboxSupportMac::GetSystemColor(
    blink::MacSystemColorID color_id,
    blink::mojom::ColorScheme color_scheme) {
  if (!color_map_.IsValid()) {
    DLOG(ERROR) << "GetSystemColor does not have a valid color_map_";
    return SK_ColorMAGENTA;
  }
  static_assert(blink::kMacSystemColorSchemeCount == 2,
                "Light and dark color scheme system colors loaded.");
  base::span<const SkColor> color_map = color_map_.GetMemoryAsSpan<SkColor>(
      blink::kMacSystemColorIDCount * blink::kMacSystemColorSchemeCount);
  base::span<const SkColor> color_map_for_scheme =
      color_map.subspan(color_scheme == blink::mojom::ColorScheme::kDark
                            ? blink::kMacSystemColorIDCount
                            : 0,
                        blink::kMacSystemColorIDCount);
  return color_map_for_scheme[static_cast<size_t>(color_id)];
}

void WebSandboxSupportMac::OnGotSystemColors(
    base::ReadOnlySharedMemoryRegion region) {
  color_map_ = region.Map();
}

}  // namespace content
