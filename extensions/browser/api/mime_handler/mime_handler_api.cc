// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/mime_handler/mime_handler_api.h"

#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "extensions/common/api/mime_handler.h"
#include "net/http/http_response_headers.h"

namespace extensions {

MimeHandlerGetStreamInfoFunction::MimeHandlerGetStreamInfoFunction() = default;
MimeHandlerGetStreamInfoFunction::~MimeHandlerGetStreamInfoFunction() = default;

ExtensionFunction::ResponseAction MimeHandlerGetStreamInfoFunction::Run() {
  content::RenderFrameHost* extension_rfh = render_frame_host();
  if (!extension_rfh) {
    return RespondNow(Error("Must be called from a web frame."));
  }

  content::RenderFrameHost* embedder_rfh = extension_rfh->GetParent();
  if (!embedder_rfh) {
    return RespondNow(Error("Must be called from a child frame."));
  }

  auto* stream_manager =
      mime_handler::MimeHandlerStreamManager::FromRenderFrameHost(embedder_rfh);
  if (!stream_manager) {
    return RespondNow(Error("No MIME handler stream for this frame."));
  }

  base::WeakPtr<StreamContainer> stream =
      stream_manager->GetStreamContainer(embedder_rfh);
  if (!stream) {
    return RespondNow(Error("No stream found for this frame."));
  }

  if (stream->extension_id() != extension()->id()) {
    return RespondNow(Error("Stream does not belong to this extension."));
  }

  api::mime_handler::StreamInfo stream_info;
  stream_info.mime_type = stream->mime_type();
  stream_info.original_url = stream->original_url().spec();
  stream_info.stream_url = stream->stream_url().spec();
  stream_info.tab_id = stream->tab_id();
  stream_info.embedded = stream->embedded();

  if (stream->response_headers()) {
    size_t iter = 0;
    std::string name;
    std::string value;
    while (stream->response_headers()->EnumerateHeaderLines(&iter, &name,
                                                            &value)) {
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

}  // namespace extensions
