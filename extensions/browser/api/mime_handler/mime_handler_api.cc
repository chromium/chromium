// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/mime_handler/mime_handler_api.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "extensions/common/api/mime_handler.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "net/http/http_response_headers.h"

namespace extensions {

namespace {

struct ResolvedStream {
  raw_ptr<content::RenderFrameHost> embedder_rfh;
  raw_ptr<mime_handler::MimeHandlerStreamManager> stream_manager;
  base::WeakPtr<StreamContainer> stream;
};

// Validates that `extension_rfh` is a child frame whose embedder owns a
// MIME handler stream belonging to `expected_extension_id`. On success
// returns the resolved embedder frame, stream manager, and stream;
// otherwise returns the user-facing error string.
base::expected<ResolvedStream, std::string> ResolveStreamForExtensionFrame(
    content::RenderFrameHost* extension_rfh,
    const ExtensionId& expected_extension_id) {
  if (!extension_rfh) {
    return base::unexpected("Must be called from a web frame.");
  }
  content::RenderFrameHost* embedder_rfh = extension_rfh->GetParent();
  if (!embedder_rfh) {
    return base::unexpected("Must be called from a child frame.");
  }
  auto* stream_manager =
      mime_handler::MimeHandlerStreamManager::FromRenderFrameHost(embedder_rfh);
  if (!stream_manager) {
    return base::unexpected("No MIME handler stream for this frame.");
  }
  base::WeakPtr<StreamContainer> stream =
      stream_manager->GetStreamContainer(embedder_rfh);
  if (!stream) {
    return base::unexpected("No stream found for this frame.");
  }
  if (stream->extension_id() != expected_extension_id) {
    return base::unexpected("Stream does not belong to this extension.");
  }
  return ResolvedStream{embedder_rfh, stream_manager, std::move(stream)};
}

}  // namespace

MimeHandlerGetStreamInfoFunction::MimeHandlerGetStreamInfoFunction() = default;
MimeHandlerGetStreamInfoFunction::~MimeHandlerGetStreamInfoFunction() = default;

ExtensionFunction::ResponseAction MimeHandlerGetStreamInfoFunction::Run() {
  auto resolved =
      ResolveStreamForExtensionFrame(render_frame_host(), extension()->id());
  if (!resolved.has_value()) {
    return RespondNow(Error(std::move(resolved.error())));
  }
  StreamContainer& stream = *resolved->stream;

  api::mime_handler::StreamInfo stream_info;
  stream_info.mime_type = stream.mime_type();
  stream_info.original_url = stream.original_url().spec();
  stream_info.stream_url = stream.stream_url().spec();
  stream_info.tab_id = stream.tab_id();
  stream_info.embedded = stream.embedded();

  if (stream.response_headers()) {
    size_t iter = 0;
    std::string name;
    std::string value;
    while (
        stream.response_headers()->EnumerateHeaderLines(&iter, &name, &value)) {
      if (!base::IsStringASCII(name) || !base::IsStringASCII(value)) {
        continue;
      }
      const std::string* existing =
          stream_info.response_headers.additional_properties.FindString(name);
      if (existing) {
        stream_info.response_headers.additional_properties.Set(
            name, base::StrCat({*existing, ", ", value}));
      } else {
        stream_info.response_headers.additional_properties.Set(name, value);
      }
    }
  }

  return RespondNow(WithArguments(stream_info.ToValue()));
}

MimeHandlerAbortAndFallbackToNativeHandlerFunction::
    MimeHandlerAbortAndFallbackToNativeHandlerFunction() = default;
MimeHandlerAbortAndFallbackToNativeHandlerFunction::
    ~MimeHandlerAbortAndFallbackToNativeHandlerFunction() = default;

ExtensionFunction::ResponseAction
MimeHandlerAbortAndFallbackToNativeHandlerFunction::Run() {
  if (std::ranges::contains(MimeTypesHandler::GetMIMETypeAllowlist(),
                            extension_id())) {
    return RespondNow(
        Error("abortAndFallbackToNativeHandler is not available "
              "for built-in MIME handler extensions."));
  }

  auto resolved =
      ResolveStreamForExtensionFrame(render_frame_host(), extension()->id());
  if (!resolved.has_value()) {
    return RespondNow(Error(std::move(resolved.error())));
  }
  resolved->stream_manager->AbortAndFallbackToNativeHandler(
      resolved->embedder_rfh);
  return RespondNow(NoArguments());
}

}  // namespace extensions
