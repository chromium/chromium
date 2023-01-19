// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector.h"

#include "base/notreached.h"
#include "remoting/host/input_injector_x11.h"
#include "remoting/host/linux/input_injector_wayland.h"
#include "remoting/host/linux/wayland_utils.h"

namespace remoting {

// static
std::unique_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  if (IsRunningWayland()) {
    return std::make_unique<InputInjectorWayland>(main_task_runner);
  }
  auto injector = std::make_unique<InputInjectorX11>(main_task_runner);
  injector->Init();
  return std::move(injector);
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
