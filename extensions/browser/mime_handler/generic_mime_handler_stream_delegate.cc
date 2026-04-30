// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/generic_mime_handler_stream_delegate.h"

#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "extensions/browser/mime_handler/stream_info.h"

namespace extensions::mime_handler {

GenericMimeHandlerStreamDelegate::GenericMimeHandlerStreamDelegate() = default;

void GenericMimeHandlerStreamDelegate::OnExtensionFrameReadyToCommit(
    content::NavigationHandle* navigation_handle,
    extensions::StreamInfo* stream_info) {
  CHECK(stream_info);
  // Register the transferable URL loader as a subresource override so the
  // handler page can fetch the original response data via the stream URL.
  navigation_handle->RegisterSubresourceOverride(
      stream_info->stream()->TakeTransferrableURLLoader());
}

void GenericMimeHandlerStreamDelegate::ValidateContentFrameHost(
    content::RenderFrameHost* /*content_host*/,
    extensions::StreamInfo* /*stream_info*/) {}

GenericMimeHandlerStreamDelegate::~GenericMimeHandlerStreamDelegate() = default;

}  // namespace extensions::mime_handler
