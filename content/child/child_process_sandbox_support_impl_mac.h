// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_MAC_H_
#define CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_MAC_H_

#include <CoreText/CoreText.h>

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
  ~WebSandboxSupportMac() override;

  // blink::WebSandboxSupport:
  bool LoadFont(CTFontRef font, CGFontRef* out, uint32_t* font_id) override;
  SkColor GetSystemColor(blink::MacSystemColorID color_id) override;

 private:
  void OnGotSystemColors(base::ReadOnlySharedMemoryRegion region);

  mojo::Remote<mojom::SandboxSupportMac> sandbox_support_;
  base::ReadOnlySharedMemoryMapping color_map_;

  DISALLOW_COPY_AND_ASSIGN(WebSandboxSupportMac);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_MAC_H_
