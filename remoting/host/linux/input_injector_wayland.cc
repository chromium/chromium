// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector.h"

#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

// static
std::unique_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  NOTIMPLEMENTED();
  return nullptr;
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
