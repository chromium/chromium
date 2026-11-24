// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_EXTENSION_H_
#define CONTENT_RENDERER_WEB_UI_EXTENSION_H_

#include <stdint.h>

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/common/web_ui.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "v8/include/v8-forward.h"

namespace gin {
class Arguments;
}

namespace content {

class RenderFrame;

// Installs and implements the chrome.* bindings used by WebUI pages. These are
// for WebUI only: they are installed in, and respond to calls from, frames the
// browser has granted WebUI bindings via RenderFrameHostImpl::AllowBindings.
class WebUIExtension : public RenderFrameObserver,
                       public RenderFrameObserverTracker<WebUIExtension>,
                       public mojom::WebUI {
 public:
  static void CreateIfNeeded(RenderFrame* render_frame);

  WebUIExtension(const WebUIExtension&) = delete;
  WebUIExtension& operator=(const WebUIExtension&) = delete;

  ~WebUIExtension() override;

  void BindWebUI(mojo::PendingAssociatedReceiver<mojom::WebUI> receiver,
                 mojo::PendingAssociatedRemote<mojom::WebUIHost> remote);

 private:
  explicit WebUIExtension(RenderFrame* render_frame);

  // RenderFrameObserver:
  void DidClearWindowObject() override;
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int32_t world_id) override;
  void OnDestruct() override;

  // mojom::WebUI:
  void SetProperty(const std::string& name, const std::string& value) override;

  void InstallDefaultWebUIExtension(v8::Isolate* isolate,
                                    v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> chrome);
  void Send(gin::Arguments* args);
  std::string GetVariableValue(const std::string& name) const;
  void RequestPresentationTime(gin::Arguments* args);

  absl::flat_hash_map<std::string, std::string> variable_map_;
  mojo::AssociatedReceiver<mojom::WebUI> receiver_{this};
  mojo::AssociatedRemote<mojom::WebUIHost> remote_;
  base::WeakPtrFactory<WebUIExtension> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_EXTENSION_H_
