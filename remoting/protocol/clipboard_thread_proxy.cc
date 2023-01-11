// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/clipboard_thread_proxy.h"

#include "base/functional/bind.h"
#include "remoting/proto/event.pb.h"

namespace remoting::protocol {

ClipboardThreadProxy::~ClipboardThreadProxy() = default;

ClipboardThreadProxy::ClipboardThreadProxy(
    const base::WeakPtr<ClipboardStub>& clipboard_stub,
    scoped_refptr<base::TaskRunner> clipboard_stub_task_runner)
    : clipboard_stub_(clipboard_stub),
      clipboard_stub_task_runner_(clipboard_stub_task_runner) {}

void ClipboardThreadProxy::InjectClipboardEvent(const ClipboardEvent& event) {
  clipboard_stub_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ClipboardThreadProxy::InjectClipboardEventStatic,
                     clipboard_stub_, event));
}

void ClipboardThreadProxy::InjectClipboardEventStatic(
    const base::WeakPtr<ClipboardStub>& clipboard_stub,
    const ClipboardEvent& event) {
  if (clipboard_stub.get()) {
    clipboard_stub->InjectClipboardEvent(event);
  }
}

}  // namespace remoting::protocol
