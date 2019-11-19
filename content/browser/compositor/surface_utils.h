// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_
#define CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"

namespace viz {
class FrameSinkManagerImpl;
class HostFrameSinkManager;
}

namespace content {

CONTENT_EXPORT viz::FrameSinkId AllocateFrameSinkId();

CONTENT_EXPORT viz::HostFrameSinkManager* GetHostFrameSinkManager();

namespace surface_utils {

// Directly connects HostFrameSinkManager to FrameSinkManagerImpl without Mojo.
CONTENT_EXPORT void ConnectWithLocalFrameSinkManager(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    viz::FrameSinkManagerImpl* frame_sink_manager_impl,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner = nullptr);

}  // namespace surface_utils

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SURFACE_UTILS_H_
