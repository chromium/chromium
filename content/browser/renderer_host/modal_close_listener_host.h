// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MODAL_CLOSE_LISTENER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MODAL_CLOSE_LISTENER_HOST_H_

#include "content/common/content_export.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/modal_close_watcher/modal_close_listener.mojom.h"

namespace content {

// ModalCloseListenerHost is a helper class that notifies a ModalCloseListener
// in the renderer host of a close signal (e.g., an android back button press).
class CONTENT_EXPORT ModalCloseListenerHost
    : public RenderDocumentHostUserData<ModalCloseListenerHost> {
 public:
  ~ModalCloseListenerHost() override;

  void SetListener(
      mojo::PendingRemote<blink::mojom::ModalCloseListener> listener);
  bool SignalIfActive();

 private:
  explicit ModalCloseListenerHost(RenderFrameHost* render_frame_host);
  friend class RenderDocumentHostUserData<ModalCloseListenerHost>;

  void Disconnect();

  mojo::Remote<blink::mojom::ModalCloseListener> modal_close_listener_;
  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();
  DISALLOW_COPY_AND_ASSIGN(ModalCloseListenerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MODAL_CLOSE_LISTENER_HOST_H_
