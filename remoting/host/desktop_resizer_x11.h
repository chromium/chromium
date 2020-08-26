// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_RESIZER_X11_H_
#define REMOTING_HOST_DESKTOP_RESIZER_X11_H_

#include <string.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "remoting/base/logging.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/linux/x11_util.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/x11.h"

namespace remoting {

// Wrapper class for the XRRScreenResources struct.
class ScreenResources {
 public:
  ScreenResources();
  ~ScreenResources();

  bool Refresh(x11::RandR* randr, x11::Window window);

  x11::RandR::Mode GetIdForMode(const std::string& name);

  // For now, assume we're only ever interested in the first output.
  x11::RandR::Output GetOutput();

  // For now, assume we're only ever interested in the first crtc.
  x11::RandR::Crtc GetCrtc();

  x11::RandR::GetScreenResourcesCurrentReply* get();

 private:
  std::unique_ptr<x11::RandR::GetScreenResourcesCurrentReply> resources_;
};

class DesktopResizerX11 : public DesktopResizer,
                          public x11::Connection::Delegate {
 public:
  DesktopResizerX11();
  DesktopResizerX11(const DesktopResizerX11&) = delete;
  DesktopResizerX11& operator=(const DesktopResizerX11&) = delete;
  ~DesktopResizerX11() override;

  // DesktopResizer interface
  ScreenResolution GetCurrentResolution() override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred) override;
  void SetResolution(const ScreenResolution& resolution) override;
  void RestoreResolution(const ScreenResolution& original) override;

 private:
  // x11::Connection::Delegate:
  bool ShouldContinueStream() const override;
  void DispatchXEvent(x11::Event* event) override;

  // Add a mode matching the specified resolution and switch to it.
  void SetResolutionNewMode(const ScreenResolution& resolution);

  // Attempt to switch to an existing mode matching the specified resolution
  // using RandR, if such a resolution exists. Otherwise, do nothing.
  void SetResolutionExistingMode(const ScreenResolution& resolution);

  // Create a mode, and attach it to the primary output. If the mode already
  // exists, it is left unchanged.
  void CreateMode(const char* name, int width, int height);

  // Remove the specified mode from the primary output, and delete it. If the
  // mode is in use, it is not deleted.
  void DeleteMode(const char* name);

  // Switch the primary output to the specified mode. If name is nullptr, the
  // primary output is disabled instead, which is required before changing
  // its resolution.
  void SwitchToMode(const char* name);

  x11::Connection connection_;
  x11::RandR* const randr_ = nullptr;
  const x11::Screen* const screen_ = nullptr;
  x11::Window root_;
  ScreenResources resources_;
  bool exact_resize_;
  bool has_randr_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_RESIZER_X11_H_
