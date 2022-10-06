// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/session_terminator.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/chromeos/ash_proxy.h"

namespace remoting {

namespace {

class SessionTerminatorChromeOs : public SessionTerminator {
 public:
  explicit SessionTerminatorChromeOs(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
      : ui_task_runner_(ui_task_runner) {}

  SessionTerminatorChromeOs(const SessionTerminatorChromeOs&) = delete;
  SessionTerminatorChromeOs& operator=(const SessionTerminatorChromeOs&) =
      delete;
  ~SessionTerminatorChromeOs() override {
    LOG(INFO) << "Force signing out of the current user as the CRD curtained "
                 "session is terminating.";
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AshProxy::RequestSignOut,
                                  base::Unretained(&AshProxy::Get())));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

}  // namespace

// static
std::unique_ptr<SessionTerminator> SessionTerminator::Create(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return std::make_unique<SessionTerminatorChromeOs>(ui_task_runner);
}

}  // namespace remoting
