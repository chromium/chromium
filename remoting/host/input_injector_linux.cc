// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector.h"

#include "base/notreached.h"

#if defined(REMOTING_USE_X11)
#include "remoting/host/input_injector_x11.h"
#endif
#if defined(REMOTING_USE_WAYLAND)
#include "remoting/host/linux/input_injector_wayland.h"
#endif
#include "remoting/host/linux/wayland_utils.h"

namespace remoting {

// static
std::unique_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  std::unique_ptr<InputInjector> input_injector;
#if defined(REMOTING_USE_WAYLAND)
  if (IsRunningWayland()) {
    input_injector = std::make_unique<InputInjectorWayland>(main_task_runner);
  }
#elif defined(REMOTING_USE_X11)
  auto injector = std::make_unique<InputInjectorX11>(main_task_runner);
  injector->Init();
  input_injector = std::move(injector);
#else
#error "Should use either wayland or X11."
#endif
  return input_injector;
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
