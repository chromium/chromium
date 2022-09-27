// Copyright 2014 The Chromium Authors
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

RemoteDOMWindow::RemoteDOMWindow(RemoteFrame& frame) : DOMWindow(frame) {}

void RemoteDOMWindow::FrameDetached() {
  DisconnectFromFrame();
}

void RemoteDOMWindow::SchedulePostMessage(PostedMessage* posted_message) {
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
  posted_message->source
      ->GetTaskRunner(TaskType::kInternalPostMessageForwarding)
      ->PostTask(FROM_HERE, WTF::BindOnce(&RemoteDOMWindow::ForwardPostMessage,
                                          WrapPersistent(this),
                                          WrapPersistent(posted_message)));
}

void RemoteDOMWindow::ForwardPostMessage(PostedMessage* posted_message) {
  // If the target frame was detached after the message was scheduled,
  // don't deliver the message.
  if (!GetFrame())
    return;

  LocalFrame* source_frame = posted_message->source->GetFrame();
  scoped_refptr<const SecurityOrigin> source_origin =
      posted_message->source_origin;
  scoped_refptr<const SecurityOrigin> target_origin =
      posted_message->target_origin;
  GetFrame()->ForwardPostMessage(
      std::move(*posted_message).ToBlinkTransferableMessage(), source_frame,
      std::move(source_origin), std::move(target_origin));
}

}  // namespace blink
