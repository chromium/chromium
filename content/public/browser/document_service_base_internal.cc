// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/document_service_base_internal.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

DocumentServiceBaseInternal::DocumentServiceBaseInternal(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  static_cast<RenderFrameHostImpl*>(render_frame_host_)
      ->AddDocumentService(this, {});
}

DocumentServiceBaseInternal::~DocumentServiceBaseInternal() {
  static_cast<RenderFrameHostImpl*>(render_frame_host_)
      ->RemoveDocumentService(this, {});
}

}  // namespace content
