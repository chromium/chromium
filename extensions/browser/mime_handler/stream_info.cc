// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/stream_info.h"

#include <utility>

#include "base/check.h"
#include "extensions/browser/mime_handler/mime_handler_stream_delegate.h"
#include "extensions/browser/mime_handler/stream_container.h"

namespace extensions {

StreamInfo::StreamInfo(const std::string& embed_internal_id,
                       std::unique_ptr<StreamContainer> stream_container,
                       std::unique_ptr<MimeHandlerStreamDelegate> delegate)
    : internal_id_(embed_internal_id),
      stream_(std::move(stream_container)),
      delegate_(std::move(delegate)) {
  CHECK(delegate_);
  // Make sure 0 is never used because some APIs
  // (particularly WebRequest) have special meaning for 0 IDs.
  static int32_t next_instance_id = 0;
  instance_id_ = ++next_instance_id;
}

StreamInfo::~StreamInfo() = default;

void StreamInfo::SetDidExtensionFinishNavigation() {
  CHECK(!did_extension_finish_navigation_);
  did_extension_finish_navigation_ = true;
}

bool StreamInfo::DidPdfExtensionStartNavigation() const {
  return !!extension_host_frame_tree_node_id_;
}

bool StreamInfo::DidPdfContentNavigate() const {
  return container_manager_.is_bound();
}

}  // namespace extensions
