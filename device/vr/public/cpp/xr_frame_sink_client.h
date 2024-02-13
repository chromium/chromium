// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_CPP_XR_FRAME_SINK_CLIENT_H_
#define DEVICE_VR_PUBLIC_CPP_XR_FRAME_SINK_CLIENT_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom-forward.h"

namespace viz {
class FrameSinkId;
class SurfaceId;
}  // namespace viz
namespace device {

// Helper enum used to describe if DOMOverlay should attempt to be setup, and if
// it should, if initialization should fail if it is unable to be setup.
enum DomOverlaySetup {
  kNone,
  kRequired,
  kOptional,
};

// There are a handful of methods to create/register RootCompositorFrameSinks
// that must be run on the UI thread; however, the interfaces that need to be
// called are restricted to //content. This interface is designed to allow
// injection and encapsulation of those calls for XR Runtimes that are creating
// a RootCompositorFrameSink.
class COMPONENT_EXPORT(VR_PUBLIC_CPP) XrFrameSinkClient {
 public:
  XrFrameSinkClient();
  virtual ~XrFrameSinkClient();

  // Registers/sets up a RootCompositorFrameSink. Note that on_initialized will
  // be run on the UI thread. If a SurfaceId is returned via on_initialized, it
  // means that the compositor can handle rendering DOM content.
  virtual void InitializeRootCompositorFrameSink(
      viz::mojom::RootCompositorFrameSinkParamsPtr root_params,
      DomOverlaySetup dom_setup,
      base::OnceClosure on_initialized) = 0;

  // Used to shutdown a RootCompositorFrameSink upon its drawing surface being
  // destroyed. This must be called from the UI thread.
  virtual void SurfaceDestroyed() = 0;

  // Used to get the SurfaceId of the DOM content to be rendered.
  // May be called from any thread.
  virtual std::optional<viz::SurfaceId> GetDOMSurface() = 0;

  virtual viz::FrameSinkId FrameSinkId() = 0;
};

// This factory must be run on the UI thread, so that the XrFrameSinkClient can
// be created and destroyed on the UI thread.
using XrFrameSinkClientFactory = base::RepeatingCallback<std::unique_ptr<
    XrFrameSinkClient>(int32_t render_process_id, int32_t render_frame_id)>;
}  // namespace device

#endif  // DEVICE_VR_PUBLIC_CPP_XR_FRAME_SINK_CLIENT_H_
