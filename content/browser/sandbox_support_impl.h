// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOX_SUPPORT_IMPL_H_
#define CONTENT_BROWSER_SANDBOX_SUPPORT_IMPL_H_

#include "content/common/sandbox_support.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

// Performs privileged operations on behalf of sandboxed child processes.
// This is used to implement the blink::WebSandboxSupport interface in the
// renderer. However all child process types have access to this interface.
// This class lives on the IO thread and is owned by the Mojo interface
// registry.
class SandboxSupportImpl : public mojom::SandboxSupport {
 public:
  SandboxSupportImpl();

  SandboxSupportImpl(const SandboxSupportImpl&) = delete;
  SandboxSupportImpl& operator=(const SandboxSupportImpl&) = delete;

  ~SandboxSupportImpl() override;

  void BindReceiver(mojo::PendingReceiver<mojom::SandboxSupport> receiver);

  // content::mojom::SandboxSupport:
  void GetSystemColors(GetSystemColorsCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::SandboxSupport> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_SUPPORT_IMPL_H_
