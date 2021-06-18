// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_dom_window.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ExecutionContext* RemoteDOMWindow::GetExecutionContext() const {
  return nullptr;
}

void RemoteDOMWindow::Trace(Visitor* visitor) const {
  DOMWindow::Trace(visitor);
}

void RemoteDOMWindow::blur() {
  // FIXME: Implement.
}

RemoteDOMWindow::RemoteDOMWindow(RemoteFrame& frame) : DOMWindow(frame) {}

void RemoteDOMWindow::FrameDetached() {
  DisconnectFromFrame();
}

void RemoteDOMWindow::SchedulePostMessage(
    MessageEvent* event,
    scoped_refptr<const SecurityOrigin> target,
    LocalDOMWindow* source) {
  // To match same-process behavior, the IPC to forward postMessage
  // cross-process should only be sent after the current script finishes
  // running, to preserve relative ordering of IPCs.  See
  // https://crbug.com/828529.
  //
  // TODO(alexmos, kenrb): PostTask isn't sufficient in some cases, such as
  // when script triggers a layout change after calling postMessage(), which
  // should also be observable by the target frame prior to receiving the
  // postMessage. We might consider forcing layout in ForwardPostMessage or
  // further delaying postMessage forwarding until after the next BeginFrame.
  source->GetTaskRunner(TaskType::kPostedMessage)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&RemoteDOMWindow::ForwardPostMessage,
                           WrapPersistent(this), WrapPersistent(event),
                           std::move(target), WrapPersistent(source)));
}

void RemoteDOMWindow::ForwardPostMessage(
    MessageEvent* event,
    scoped_refptr<const SecurityOrigin> target,
    LocalDOMWindow* source) {
  // If the target frame was detached after the message was scheduled,
  // don't deliver the message.
  if (!GetFrame())
    return;

  absl::optional<base::UnguessableToken> agent_cluster;
  if (event->IsLockedToAgentCluster())
    agent_cluster = source->GetAgentClusterID();
  GetFrame()->ForwardPostMessage(event, agent_cluster, std::move(target),
                                 source->GetFrame());
}

}  // namespace blink
