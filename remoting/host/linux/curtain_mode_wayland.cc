// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode.h"

#include "base/callback.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

// static
std::unique_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace remoting
