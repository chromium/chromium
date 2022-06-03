// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_level_context.h"

#include <utility>

#include "content/browser/host_zoom_map_impl.h"

namespace content {

HostZoomLevelContext::HostZoomLevelContext(
    std::unique_ptr<ZoomLevelDelegate> zoom_level_delegate)
    : host_zoom_map_impl_(new HostZoomMapImpl()),
      zoom_level_delegate_(std::move(zoom_level_delegate)) {
  if (zoom_level_delegate_)
    zoom_level_delegate_->InitHostZoomMap(host_zoom_map_impl_.get());
}

HostZoomLevelContext::~HostZoomLevelContext() = default;

}  // namespace content
