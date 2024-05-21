// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_IN_PROCESS_CHILD_THREAD_PARAMS_H_
#define CONTENT_COMMON_IN_PROCESS_CHILD_THREAD_PARAMS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/invitation.h"

namespace content {

// Tells ChildThreadImpl to run in in-process mode. There are a couple of
// parameters to run in the mode: An emulated io task runner used by
// ChnanelMojo, an IPC channel name to open. `child_io_runner` can be passed
// with the current IO thread to allow it to be shared by the child process.
class CONTENT_EXPORT InProcessChildThreadParams {
 public:
  InProcessChildThreadParams(
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      mojo::OutgoingInvitation* mojo_invitation,
      scoped_refptr<base::SingleThreadTaskRunner> child_io_runner = nullptr);
  InProcessChildThreadParams(const InProcessChildThreadParams& other);
  ~InProcessChildThreadParams();

  const scoped_refptr<base::SingleThreadTaskRunner>& io_runner() const {
    return io_runner_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& child_io_runner() const {
    return child_io_runner_;
  }

  mojo::OutgoingInvitation* mojo_invitation() const { return mojo_invitation_; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;
  const raw_ptr<mojo::OutgoingInvitation> mojo_invitation_;
  scoped_refptr<base::SingleThreadTaskRunner> child_io_runner_;
};

}  // namespace content

#endif  // CONTENT_COMMON_IN_PROCESS_CHILD_THREAD_PARAMS_H_
