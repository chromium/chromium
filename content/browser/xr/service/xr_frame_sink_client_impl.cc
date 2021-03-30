// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/xr_frame_sink_client_impl.h"

#include <memory>

#include "base/callback.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"

namespace content {
XrFrameSinkClientImpl::XrFrameSinkClientImpl()
    : ui_thread_task_runner_(GetUIThreadTaskRunner({})) {
  DCHECK(IsOnUiThread())
      << "XrFrameSinkClientImpl must be constructed on the UI thread.";
}

XrFrameSinkClientImpl::~XrFrameSinkClientImpl() {
  DCHECK(IsOnUiThread())
      << "XrFrameSinkClientImpl must be destructed on the UI thread.";
  if (!initialized_)
    return;

  SurfaceDestroyed();
}

bool XrFrameSinkClientImpl::IsOnUiThread() const {
  return ui_thread_task_runner_->BelongsToCurrentThread();
}

void XrFrameSinkClientImpl::SurfaceDestroyed() {
  DCHECK(IsOnUiThread());
  if (!initialized_)
    return;

  auto* frame_sink_manager = GetHostFrameSinkManager();

  // Since this code can be run during destruction, it's theoretically possible,
  // though unlikely, that the FrameSinkManager no longer exists.
  if (frame_sink_manager)
    frame_sink_manager->InvalidateFrameSinkId(root_frame_sink_id_);

  // Reset the initialized state and the root FrameSinkId to an invalid value.
  initialized_ = false;
  root_frame_sink_id_ = viz::FrameSinkId();
}

void XrFrameSinkClientImpl::InitializeRootCompositorFrameSink(
    viz::mojom::RootCompositorFrameSinkParamsPtr root_params,
    base::OnceClosure on_initialized) {
  DCHECK(!initialized_);
  DVLOG(1) << __func__;

  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&XrFrameSinkClientImpl::InitializeOnUiThread,
                     weak_ptr_factory_.GetWeakPtr(), std::move(root_params),
                     std::move(on_initialized)));
}

void XrFrameSinkClientImpl::InitializeOnUiThread(
    viz::mojom::RootCompositorFrameSinkParamsPtr root_params,
    base::OnceClosure on_initialized) {
  // AllocateFrameSinkId needs to be called from the UI thread.
  DCHECK(IsOnUiThread());
  DVLOG(1) << __func__;

  root_frame_sink_id_ = AllocateFrameSinkId();
  root_params->frame_sink_id = root_frame_sink_id_;

  GetHostFrameSinkManager()->RegisterFrameSinkId(
      root_params->frame_sink_id, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params));

  initialized_ = true;
  std::move(on_initialized).Run();
}

}  // namespace content
