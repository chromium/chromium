// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/ipc_interfaces_dumper.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"

namespace content {

void GetBoundInterfacesForTesting(RenderFrameHost* rfh,
                                  std::vector<std::string>& out) {
  static_cast<RenderFrameHostImpl*>(rfh)->GetBoundInterfacesForTesting(out);
}

void GetBoundAssociatedInterfacesForTesting(RenderFrameHost* rfh,
                                            std::vector<std::string>& out) {
  static_cast<RenderFrameHostImpl*>(rfh)
      ->GetBoundAssociatedInterfacesForTesting(out);
}

void GetBoundInterfacesForTesting(RenderProcessHost* rph,
                                  std::vector<std::string>& out) {
  static_cast<content::RenderProcessHostImpl*>(rph)
      ->GetBoundInterfacesForTesting(out);
}

}  // namespace content
