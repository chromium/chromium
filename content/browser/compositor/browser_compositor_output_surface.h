// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/service/display/output_surface.h"
#include "content/common/content_export.h"

namespace cc {
class SoftwareOutputDevice;
}

namespace gfx {
enum class SwapResult;
}

namespace content {
class ReflectorImpl;

class CONTENT_EXPORT BrowserCompositorOutputSurface
    : public viz::OutputSurface {
 public:
  ~BrowserCompositorOutputSurface() override;

  // viz::OutputSurface implementation.
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
  void SetUpdateVSyncParametersCallback(
      viz::UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;

  bool IsSoftwareMirrorMode() const override;
  void SetReflector(ReflectorImpl* reflector);

  // Called when |reflector_| was updated.
  virtual void OnReflectorChanged();

 protected:
  // Constructor used by the accelerated implementation.
  BrowserCompositorOutputSurface(scoped_refptr<viz::ContextProvider> context);

  // Constructor used by the software implementation.
  explicit BrowserCompositorOutputSurface(
      std::unique_ptr<viz::SoftwareOutputDevice> software_device);

  viz::UpdateVSyncParametersCallback update_vsync_parameters_callback_;
  ReflectorImpl* reflector_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
