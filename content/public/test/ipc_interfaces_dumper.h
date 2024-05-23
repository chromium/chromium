// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_IPC_INTERFACES_DUMPER_H_
#define CONTENT_PUBLIC_TEST_IPC_INTERFACES_DUMPER_H_

#include <string>
#include <vector>

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace content {
void GetBoundInterfacesForTesting(RenderFrameHost* rfh,
                                  std::vector<std::string>& out);
void GetBoundAssociatedInterfacesForTesting(RenderFrameHost* rfh,
                                            std::vector<std::string>& out);
void GetBoundInterfacesForTesting(RenderProcessHost* rfh,
                                  std::vector<std::string>& out);
}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_IPC_INTERFACES_DUMPER_H_
