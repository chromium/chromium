// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_render_frame_observer.h"

#include "base/bind.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

WebEngineRenderFrameObserver::WebEngineRenderFrameObserver(
    content::RenderFrame* render_frame,
    base::OnceCallback<void(int)> on_render_frame_deleted_callback)
    : content::RenderFrameObserver(render_frame),
      url_request_rules_receiver_(render_frame),
      on_render_frame_deleted_callback_(
          std::move(on_render_frame_deleted_callback)) {
  DCHECK(render_frame);
  DCHECK(on_render_frame_deleted_callback_);
}

WebEngineRenderFrameObserver::~WebEngineRenderFrameObserver() = default;

void WebEngineRenderFrameObserver::OnDestruct() {
  std::move(on_render_frame_deleted_callback_).Run(routing_id());
}
