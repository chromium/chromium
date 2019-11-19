// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_extension_data.h"

#include "content/common/frame_messages.h"
#include "content/public/renderer/render_frame.h"

namespace content {

WebUIExtensionData::WebUIExtensionData(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<WebUIExtensionData>(render_frame) {}

WebUIExtensionData::~WebUIExtensionData() {
}

std::string WebUIExtensionData::GetValue(const std::string& key) const {
  auto it = variable_map_.find(key);
  if (it == variable_map_.end())
    return std::string();
  return it->second;
}

bool WebUIExtensionData::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebUIExtensionData, message)
    IPC_MESSAGE_HANDLER(FrameMsg_SetWebUIProperty, OnSetWebUIProperty)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebUIExtensionData::OnSetWebUIProperty(const std::string& name,
                                            const std::string& value) {
  variable_map_[name] = value;
}

void WebUIExtensionData::OnDestruct() {
  delete this;
}

}  // namespace content
