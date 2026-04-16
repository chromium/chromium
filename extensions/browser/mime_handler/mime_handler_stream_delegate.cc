// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_stream_delegate.h"

namespace extensions {

MimeHandlerStreamDelegate::MimeHandlerStreamDelegate() = default;
MimeHandlerStreamDelegate::~MimeHandlerStreamDelegate() = default;

void MimeHandlerStreamDelegate::OnStreamClaimed(
    content::RenderFrameHost* embedder_host,
    StreamInfo* stream_info) {}

bool MimeHandlerStreamDelegate::PluginCanSave() const {
  return false;
}

void MimeHandlerStreamDelegate::SetPluginCanSave(bool plugin_can_save) {}

}  // namespace extensions
