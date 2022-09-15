// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_EXTENSION_DATA_H_
#define CONTENT_RENDERER_WEB_UI_EXTENSION_DATA_H_

#include <map>
#include <string>

#include "base/values.h"
#include "content/common/web_ui.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace content {

class WebUIExtensionData
    : public RenderFrameObserver,
      public RenderFrameObserverTracker<WebUIExtensionData>,
      public mojom::WebUI {
 public:
  static void Create(RenderFrame* render_frame,
                     mojo::PendingAssociatedReceiver<mojom::WebUI> receiver,
                     mojo::PendingAssociatedRemote<mojom::WebUIHost> remote);

  WebUIExtensionData() = delete;

  WebUIExtensionData(const WebUIExtensionData&) = delete;
  WebUIExtensionData& operator=(const WebUIExtensionData&) = delete;

  ~WebUIExtensionData() override;

  // Returns value for a given |key|. Will return an empty string if no such key
  // exists in the |variable_map_|.
  std::string GetValue(const std::string& key) const;

  void SendMessage(const std::string& message, base::Value::List args);

 private:
  // Use Create() instead.
  WebUIExtensionData(RenderFrame* render_frame,
                     mojo::PendingAssociatedRemote<mojom::WebUIHost> remote);

  // mojom::WebUI:
  void SetProperty(const std::string& name, const std::string& value) override;

  // RenderFrameObserver:
  void OnDestruct() override;

  std::map<std::string, std::string> variable_map_;

  mojo::AssociatedRemote<mojom::WebUIHost> remote_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_EXTENSION_DATA_H_
