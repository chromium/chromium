// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_
#define CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"

namespace viz {
class HostFrameSinkManager;
}

namespace content {

CONTENT_EXPORT viz::FrameSinkId AllocateFrameSinkId();

CONTENT_EXPORT viz::HostFrameSinkManager* GetHostFrameSinkManager();

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_
