// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/document_user_data.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content::internal {

base::SupportsUserData::Data* GetDocumentUserData(const RenderFrameHost* rfh,
                                                  const void* key) {
  return static_cast<const RenderFrameHostImpl*>(rfh)
      ->document_associated_data()
      .GetUserData(key);
}

void SetDocumentUserData(RenderFrameHost* rfh,
                         const void* key,
                         std::unique_ptr<base::SupportsUserData::Data> data) {
  static_cast<RenderFrameHostImpl*>(rfh)
      ->document_associated_data()
      .SetUserData(key, std::move(data));
}

void RemoveDocumentUserData(RenderFrameHost* rfh, const void* key) {
  static_cast<RenderFrameHostImpl*>(rfh)
      ->document_associated_data()
      .RemoveUserData(key);
}

}  // namespace content::internal
