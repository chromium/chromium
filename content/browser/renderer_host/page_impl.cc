// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_impl.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

PageImpl::PageImpl(RenderFrameHostImpl& rfh) : main_document_(rfh) {}

PageImpl::~PageImpl() = default;

}  // namespace content
