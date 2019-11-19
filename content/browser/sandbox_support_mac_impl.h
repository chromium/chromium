// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOX_SUPPORT_MAC_IMPL_H_
#define CONTENT_BROWSER_SANDBOX_SUPPORT_MAC_IMPL_H_

#include "base/macros.h"
#include "content/common/sandbox_support_mac.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

// Performs privileged operations on behalf of sandboxed child processes.
// This is used to implement the blink::WebSandboxSupport interface in the
// renderer. However all child process types have access to this interface.
// This class lives on the IO thread and is owned by the Mojo interface
// registry.
class SandboxSupportMacImpl : public mojom::SandboxSupportMac {
 public:
  SandboxSupportMacImpl();
  ~SandboxSupportMacImpl() override;

  void BindReceiver(mojo::PendingReceiver<mojom::SandboxSupportMac> receiver);

  // content::mojom::SandboxSupportMac:
  void GetSystemColors(GetSystemColorsCallback callback) override;
  void LoadFont(const base::string16& font_name,
                float font_point_size,
                LoadFontCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::SandboxSupportMac> receivers_;

  DISALLOW_COPY_AND_ASSIGN(SandboxSupportMacImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_SUPPORT_MAC_IMPL_H_
