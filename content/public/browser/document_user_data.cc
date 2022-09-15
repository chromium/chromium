// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/document_user_data.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

base::SupportsUserData::Data* GetDocumentUserData(const RenderFrameHost* rfh,
                                                  const void* key) {
  return static_cast<const RenderFrameHostImpl*>(rfh)->GetDocumentUserData(key);
}

void SetDocumentUserData(RenderFrameHost* rfh,
                         const void* key,
                         std::unique_ptr<base::SupportsUserData::Data> data) {
  static_cast<RenderFrameHostImpl*>(rfh)->SetDocumentUserData(key,
                                                              std::move(data));
}

void RemoveDocumentUserData(RenderFrameHost* rfh, const void* key) {
  static_cast<RenderFrameHostImpl*>(rfh)->RemoveDocumentUserData(key);
}

}  // namespace content
