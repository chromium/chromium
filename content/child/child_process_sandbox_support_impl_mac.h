// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_MAC_H_
#define CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_MAC_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "content/common/sandbox_support_mac.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/mac/web_sandbox_support.h"

namespace content {

// Implementation of the interface used by Blink to upcall to the privileged
// process (browser) for handling requests for data that are not allowed within
// the sandbox.
class WebSandboxSupportMac : public blink::WebSandboxSupport {
 public:
  WebSandboxSupportMac();

  WebSandboxSupportMac(const WebSandboxSupportMac&) = delete;
  WebSandboxSupportMac& operator=(const WebSandboxSupportMac&) = delete;

  ~WebSandboxSupportMac() override;

  SkColor GetSystemColor(blink::MacSystemColorID color_id,
                         blink::mojom::ColorScheme color_scheme) override;

 private:
  void OnGotSystemColors(base::ReadOnlySharedMemoryRegion region);

  mojo::Remote<mojom::SandboxSupportMac> sandbox_support_;
  base::ReadOnlySharedMemoryMapping color_map_;
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_MAC_H_
