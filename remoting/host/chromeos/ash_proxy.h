// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_ASH_PROXY_H_
#define REMOTING_HOST_CHROMEOS_ASH_PROXY_H_

#include <cstdint>
#include <vector>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/display/display.h"

namespace ash::curtain {
class SecurityCurtainController;
}  // namespace ash::curtain

namespace aura {
class ScopedWindowCaptureRequest;
class Window;
}  // namespace aura

namespace viz {
class FrameSinkId;
namespace mojom {
class FrameSinkVideoCapturer;
}  // namespace mojom
}  // namespace viz

namespace remoting {

using DisplayId = int64_t;

// Utility proxy class that abstracts away all ash related actions on ChromeOs,
// to prevent our code from directly calling `ash::Shell` so we can mock things
// during unittests.
class AshProxy {
 public:
  static AshProxy& Get();

  // The caller is responsible to ensure this given instance lives long enough.
  // To unset call this method again with nullptr.
  static void SetInstanceForTesting(AshProxy* instance);

  // Convert the scale factor to DPI.
  static int ScaleFactorToDpi(float scale_factor);

  virtual ~AshProxy();

  virtual DisplayId GetPrimaryDisplayId() const = 0;
  virtual const std::vector<display::Display>& GetActiveDisplays() const = 0;
  virtual const display::Display* GetDisplayForId(
      DisplayId display_id) const = 0;
  virtual aura::Window* GetSelectFileContainer() = 0;

  virtual ash::curtain::SecurityCurtainController&
  GetSecurityCurtainController() = 0;

  virtual void CreateVideoCapturer(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer>
          video_capturer) = 0;

  // Ensure the given display can be captured through the frame sink video
  // capturer. The caller must keep the returned object alive for as long as
  // they are capturing.
  virtual aura::ScopedWindowCaptureRequest MakeDisplayCapturable(
      DisplayId source_display_id) = 0;

  virtual viz::FrameSinkId GetFrameSinkId(DisplayId source_display_id) = 0;

  // Requests signing out all users, ending the current session.
  virtual void RequestSignOut() = 0;

  // Returns whether the accessibility screen reader is enabled.
  virtual bool IsScreenReaderEnabled() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_ASH_PROXY_H_
