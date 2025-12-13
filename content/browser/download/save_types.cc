// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_types.h"

#include "base/files/file_path.h"

namespace content {

SaveFileCreateInfo::SaveFileCreateInfo(const base::FilePath& path,
                                       const GURL& url,
                                       SaveItemId save_item_id,
                                       SavePackageId save_package_id,
                                       int render_process_id,
                                       int render_frame_routing_id,
                                       SaveFileSource save_source)
    : path(path),
      url(url),
      save_item_id(save_item_id),
      save_package_id(save_package_id),
      render_process_id(render_process_id),
      render_frame_routing_id(render_frame_routing_id),
      save_source(save_source) {}

SaveFileCreateInfo::SaveFileCreateInfo(const GURL& url,
                                       const GURL& final_url,
                                       SaveItemId save_item_id,
                                       SavePackageId save_package_id,
                                       int render_process_id,
                                       int render_frame_routing_id,
                                       const std::string& content_disposition)
    : url(url),
      final_url(final_url),
      save_item_id(save_item_id),
      save_package_id(save_package_id),
      render_process_id(render_process_id),
      render_frame_routing_id(render_frame_routing_id),
      content_disposition(content_disposition),
      save_source(SaveFileCreateInfo::SAVE_FILE_FROM_NET) {}

SaveFileCreateInfo::SaveFileCreateInfo(const SaveFileCreateInfo& other)
    : path(other.path),
      url(other.url),
      final_url(other.final_url),
      save_item_id(other.save_item_id),
      save_package_id(other.save_package_id),
      render_process_id(other.render_process_id),
      render_frame_routing_id(other.render_frame_routing_id),
      content_disposition(other.content_disposition),
      save_source(other.save_source) {
  // The quarantine_callback is intentionally not copied.
}

SaveFileCreateInfo::~SaveFileCreateInfo() {}

}  // namespace content
