// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_XR_FRAME_SINK_CLIENT_IMPL_H_
#define CONTENT_BROWSER_XR_SERVICE_XR_FRAME_SINK_CLIENT_IMPL_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom-forward.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
// Concrete implementation of XrFrameSinkClient to manage interactions with the
// HostFrameSinkManager. Callbacks from HostFrameSinkClient will be received on
// the UI thread. Must be created and Destroyed on the UI thread.
class XrFrameSinkClientImpl : public device::XrFrameSinkClient,
                              viz::HostFrameSinkClient {
 public:
  XrFrameSinkClientImpl(int32_t render_process_id, int32_t render_frame_id);
  ~XrFrameSinkClientImpl() override;

  // device::XrFrameSinkClient:
  void InitializeRootCompositorFrameSink(
      viz::mojom::RootCompositorFrameSinkParamsPtr root_params,
      device::DomOverlaySetup dom_setup,
      base::OnceClosure on_initialized) override;
  void SurfaceDestroyed() override;
  std::optional<viz::SurfaceId> GetDOMSurface() override;
  viz::FrameSinkId FrameSinkId() override;

 private:
  bool IsOnUiThread() const;
  void InitializeOnUiThread(
      viz::mojom::RootCompositorFrameSinkParamsPtr root_params,
      device::DomOverlaySetup dom_setup,
      base::OnceClosure on_initialized);
  void OnSurfaceIdUpdated(const viz::SurfaceId& surface_id);
  void ConfigureDOMOverlay();

  // viz::HostFrameSinkClient:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {
  }
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override {}

  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;
  int32_t render_process_id_;
  int32_t render_frame_id_;

  viz::FrameSinkId root_frame_sink_id_;
  bool initialized_ = false;

  std::optional<viz::SurfaceId> dom_surface_id_;
  base::Lock dom_surface_lock_;
#if BUILDFLAG(IS_ANDROID)
  base::CallbackListSubscription surface_id_changed_subscription_;
#endif

  // Must be last so that it will be invalidated before any other members.
  base::WeakPtrFactory<XrFrameSinkClientImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_XR_FRAME_SINK_CLIENT_IMPL_H_
