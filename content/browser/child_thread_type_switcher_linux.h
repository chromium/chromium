// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_THREAD_TYPE_SWITCHER_LINUX_H_
#define CONTENT_BROWSER_CHILD_THREAD_TYPE_SWITCHER_LINUX_H_

#include "base/process/process_handle.h"
#include "content/common/thread_type_switcher.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

// Browser-side implementation of mojom::ThreadTypeSwitcher which allows a
// sandboxed process's threads to change their priority (which can't be done
// inside the sandbox).
class ChildThreadTypeSwitcher : public mojom::ThreadTypeSwitcher {
 public:
  // Constructs an unbound ChildThreadTypeSwitcher.
  explicit ChildThreadTypeSwitcher();

  ChildThreadTypeSwitcher(const ChildThreadTypeSwitcher&) = delete;
  ChildThreadTypeSwitcher& operator=(const ChildThreadTypeSwitcher&) = delete;

  ~ChildThreadTypeSwitcher() override;

  // This method binds `this` to `receiver`. If the pid hasn't yet been set,
  // `receiver_` is paused until SetPid() is called, as it's impossible to
  // change another process's thread's priority on Linux without knowing the
  // process's Pid.
  bool Bind(mojo::PendingReceiver<mojom::ThreadTypeSwitcher> receiver);

  // Sets the pid of the child process. If Bind() has already been called, this
  // unpauses `receiver_`.
  void SetPid(base::ProcessId child_pid);

  // mojom::ThreadTypeSwitcher:
  void SetThreadType(int32_t ns_tid, base::ThreadType thread_type) override;

 private:
  base::ProcessId child_pid_ = base::kNullProcessHandle;
  mojo::Receiver<mojom::ThreadTypeSwitcher> receiver_{this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_THREAD_TYPE_SWITCHER_LINUX_H_
