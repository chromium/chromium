// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_document_host_user_data.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

base::SupportsUserData::Data* GetRenderDocumentHostUserData(
    const RenderFrameHost* rfh,
    const void* key) {
  return static_cast<const RenderFrameHostImpl*>(rfh)
      ->GetRenderDocumentHostUserData(key);
}

void SetRenderDocumentHostUserData(
    RenderFrameHost* rfh,
    const void* key,
    std::unique_ptr<base::SupportsUserData::Data> data) {
  static_cast<RenderFrameHostImpl*>(rfh)->SetRenderDocumentHostUserData(
      key, std::move(data));
}

void RemoveRenderDocumentHostUserData(RenderFrameHost* rfh, const void* key) {
  static_cast<RenderFrameHostImpl*>(rfh)->RemoveRenderDocumentHostUserData(key);
}

}  // namespace content
