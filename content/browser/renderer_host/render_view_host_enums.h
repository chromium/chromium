// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_ENUMS_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_ENUMS_H_

#include "content/common/content_export.h"

namespace content {

// Case for creating a RenderViewHost.
enum class CreateRenderViewHostCase {
  // This is a RenderViewHost created for a speculative RenderFrameHost for a
  // main-frame cross-document navigation. Currently this only applies to
  // same-SiteInstanceGroup navigations. When RenderDocument is enabled, it
  // will be possible to have cross-document same-SiteInstanceGroup navigations
  // use new RenderViewHosts (as well as new RenderFrameHosts).
  kSpeculative,

  // This covers all other cases for RenderViewHost creation, including for
  // subframe navigations, opener proxy creation and frame creation. Currently
  // this also covers main-frame navigations that don't create speculative
  // RenderViewHosts.
  kDefault,
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_ENUMS_H_
