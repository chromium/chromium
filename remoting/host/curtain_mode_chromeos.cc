// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode_chromeos.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

bool CurtainModeChromeOs::Activate() {
  // TODO(b/236687774): Implement curtain mode on ChromeOS.
  return true;
}

// static
std::unique_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<CurtainModeChromeOs>();
}

}  // namespace remoting
