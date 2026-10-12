// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_stream_delegate.h"

#include "base/strings/string_util.h"
#include "services/network/public/cpp/cors/cors.h"

namespace extensions {

MimeHandlerStreamDelegate::MimeHandlerStreamDelegate() = default;
MimeHandlerStreamDelegate::~MimeHandlerStreamDelegate() = default;

bool MimeHandlerStreamDelegate::ShouldSetUpPostMessage() const {
  return false;
}

void MimeHandlerStreamDelegate::OnPostMessageSetUp(
    content::RenderFrameHost* embedder_host) {}

void MimeHandlerStreamDelegate::OnExtensionFrameReadyToCommit(
    content::NavigationHandle* navigation_handle,
    StreamInfo* stream_info) {}

void MimeHandlerStreamDelegate::OnExtensionFrameFinished(
    content::NavigationHandle* navigation_handle,
    StreamInfo* stream_info) {}

void MimeHandlerStreamDelegate::ValidateContentFrameHost(
    content::RenderFrameHost* content_host,
    StreamInfo* stream_info) {}

void MimeHandlerStreamDelegate::OnStreamClaimed(
    content::RenderFrameHost* embedder_host,
    StreamInfo* stream_info) {}

bool MimeHandlerStreamDelegate::PluginCanSave() const {
  return false;
}

void MimeHandlerStreamDelegate::SetPluginCanSave(bool plugin_can_save) {}

bool MimeHandlerStreamDelegate::RequiresPerInstanceProcessIsolation() const {
  return false;
}

bool MimeHandlerStreamDelegate::ShouldFilterResponseHeadersForHandler() const {
  return true;
}

bool IsResponseHeaderAllowedForHandler(std::string_view name) {
  // The handler already reads the whole body. These two headers only tell it
  // how to name the file and whether the server can send parts of it.
  return network::cors::IsCorsSafelistedResponseHeaderName(name) ||
         base::EqualsCaseInsensitiveASCII(name, "content-disposition") ||
         base::EqualsCaseInsensitiveASCII(name, "accept-ranges");
}

}  // namespace extensions
