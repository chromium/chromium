// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/in_process_child_thread_params.h"
#include "base/task/single_thread_task_runner.h"

namespace content {

InProcessChildThreadParams::InProcessChildThreadParams(
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    mojo::OutgoingInvitation* mojo_invitation,
    scoped_refptr<base::SingleThreadTaskRunner> child_io_runner)
    : io_runner_(std::move(io_runner)),
      mojo_invitation_(mojo_invitation),
      child_io_runner_(std::move(child_io_runner)) {}

InProcessChildThreadParams::InProcessChildThreadParams(
    const InProcessChildThreadParams& other) = default;

InProcessChildThreadParams::~InProcessChildThreadParams() {
}

}  // namespace content
