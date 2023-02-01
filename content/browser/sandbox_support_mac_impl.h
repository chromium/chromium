// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOX_SUPPORT_MAC_IMPL_H_
#define CONTENT_BROWSER_SANDBOX_SUPPORT_MAC_IMPL_H_

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

  SandboxSupportMacImpl(const SandboxSupportMacImpl&) = delete;
  SandboxSupportMacImpl& operator=(const SandboxSupportMacImpl&) = delete;

  ~SandboxSupportMacImpl() override;

  void BindReceiver(mojo::PendingReceiver<mojom::SandboxSupportMac> receiver);

  // content::mojom::SandboxSupportMac:
  void GetSystemColors(GetSystemColorsCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::SandboxSupportMac> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_SUPPORT_MAC_IMPL_H_
