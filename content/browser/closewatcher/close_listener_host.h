// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CLOSEWATCHER_CLOSE_LISTENER_HOST_H_
#define CONTENT_BROWSER_CLOSEWATCHER_CLOSE_LISTENER_HOST_H_

#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/close_watcher/close_listener.mojom.h"

namespace content {

class CloseListenerManager;
class RenderFrameHost;

// CloseListenerHost is a helper class that notifies a CloseListener
// in the renderer host of a close signal (e.g., an android back button press).
class CONTENT_EXPORT CloseListenerHost
    : public DocumentUserData<CloseListenerHost> {
 public:
  ~CloseListenerHost() override;
  CloseListenerHost(const CloseListenerHost&) = delete;
  CloseListenerHost& operator=(const CloseListenerHost&) = delete;

  void SetListener(mojo::PendingRemote<blink::mojom::CloseListener> listener);
  bool IsActive();
  bool SignalIfActive();

 private:
  explicit CloseListenerHost(RenderFrameHost* render_frame_host);
  friend class DocumentUserData<CloseListenerHost>;

  CloseListenerManager* GetOrCreateManager();

  void OnDisconnect();

  mojo::Remote<blink::mojom::CloseListener> close_listener_;
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_CLOSEWATCHER_CLOSE_LISTENER_HOST_H_
