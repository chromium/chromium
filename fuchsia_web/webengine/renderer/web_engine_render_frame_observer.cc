// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_render_frame_observer.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"

WebEngineRenderFrameObserver::WebEngineRenderFrameObserver(
    content::RenderFrame* render_frame,
    base::OnceCallback<void(const blink::LocalFrameToken&)>
        on_render_frame_deleted_callback)
    : content::RenderFrameObserver(render_frame),
      url_request_rules_receiver_(render_frame),
      on_render_frame_deleted_callback_(
          std::move(on_render_frame_deleted_callback)) {
  DCHECK(render_frame);
  DCHECK(on_render_frame_deleted_callback_);
}

WebEngineRenderFrameObserver::~WebEngineRenderFrameObserver() = default;

void WebEngineRenderFrameObserver::OnDestruct() {
  // We should never hit this since we will have destroyed this observer
  // in WillDetach.
  NOTREACHED();
}

void WebEngineRenderFrameObserver::WillDetach(
    blink::DetachReason detach_reason) {
  std::move(on_render_frame_deleted_callback_)
      .Run(render_frame()->GetWebFrame()->GetLocalFrameToken());
}
