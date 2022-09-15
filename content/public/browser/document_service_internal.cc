// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/document_service_internal.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content::internal {

DocumentServiceBase::DocumentServiceBase(RenderFrameHost& render_frame_host)
    : render_frame_host_(render_frame_host) {
  static_cast<RenderFrameHostImpl&>(*render_frame_host_)
      .AddDocumentService(this, {});
}

DocumentServiceBase::~DocumentServiceBase() {
  static_cast<RenderFrameHostImpl&>(*render_frame_host_)
      .RemoveDocumentService(this, {});
}

}  // namespace content::internal
