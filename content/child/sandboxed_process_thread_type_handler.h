// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SANDBOXED_PROCESS_THREAD_TYPE_HANDLER_H_
#define CONTENT_CHILD_SANDBOXED_PROCESS_THREAD_TYPE_HANDLER_H_

#include "base/threading/platform_thread.h"
#include "base/threading/thread_type_delegate.h"
#include "content/common/thread_type_switcher.mojom-forward.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace content {

// This class handles thread type changes for sandboxed processes, which
// supports proxying the thread type changes to browser process.
class SandboxedProcessThreadTypeHandler : public base::ThreadTypeDelegate {
 public:
  SandboxedProcessThreadTypeHandler(const SandboxedProcessThreadTypeHandler&) =
      delete;
  SandboxedProcessThreadTypeHandler& operator=(
      const SandboxedProcessThreadTypeHandler&) = delete;

  ~SandboxedProcessThreadTypeHandler() override;

  // Creates a SandboxedProcessThreadTypeHandler instance and stores it to
  // g_instance. Make sure the g_instance doesn't exist before creation.
  static void Create();

  // Invoked when ChildThread is created. If g_instance exists,
  // g_instance.ConnectThreadTypeSwitcher() is called.
  static void NotifyMainChildThreadCreated();

  // Returns nullptr if Create() hasn't been called, e.g. in an unsandboxed
  // process.
  static SandboxedProcessThreadTypeHandler* Get();

  // Overridden from base::ThreadTypeDelegate.
  bool HandleThreadTypeChange(base::PlatformThreadId thread_id,
                              base::ThreadType thread_type) override;

 private:
  SandboxedProcessThreadTypeHandler();

  // Use the ChildThread (which must be valid) to bind `thread_type_switcher_`.
  void ConnectThreadTypeSwitcher();

  mojo::SharedRemote<mojom::ThreadTypeSwitcher> thread_type_switcher_;
  // Holds the ThreadTypeSwitcher receiver until it can be bound by the browser
  // process via ChildThreadImpl.
  mojo::PendingReceiver<mojom::ThreadTypeSwitcher>
      thread_type_switcher_receiver_;
};

}  // namespace content

#endif  // CONTENT_CHILD_SANDBOXED_PROCESS_THREAD_TYPE_HANDLER_H_
