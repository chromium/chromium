// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DATA_AURA_H_
#define CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DATA_AURA_H_

#include <memory>

#include "build/build_config.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
namespace client {
class CursorShapeClient;
class DefaultCaptureClient;
class FocusClient;
class WindowParentingClient;
}  // namespace client
}  // namespace aura

#if BUILDFLAG(IS_OZONE)
namespace aura {
class ScreenOzone;
}
#endif

namespace gfx {
class Size;
}

namespace content {

class ShellPlatformDataAura {
 public:
  explicit ShellPlatformDataAura(const gfx::Size& initial_size);

  ShellPlatformDataAura(const ShellPlatformDataAura&) = delete;
  ShellPlatformDataAura& operator=(const ShellPlatformDataAura&) = delete;

  ~ShellPlatformDataAura();

  void ShowWindow();
  void ResizeWindow(const gfx::Size& size);

  aura::WindowTreeHost* host() { return host_.get(); }

 private:
#if BUILDFLAG(IS_OZONE)
  std::unique_ptr<aura::ScreenOzone> screen_;
#endif

  std::unique_ptr<aura::WindowTreeHost> host_;
  std::unique_ptr<aura::client::FocusClient> focus_client_;
  std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;
  std::unique_ptr<aura::client::WindowParentingClient> window_parenting_client_;
  std::unique_ptr<aura::client::CursorShapeClient> cursor_shape_client_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_PLATFORM_DATA_AURA_H_
