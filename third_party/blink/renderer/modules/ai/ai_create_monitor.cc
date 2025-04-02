// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"

#include <algorithm>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"

namespace blink {

AICreateMonitor::AICreateMonitor(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      task_runner_(task_runner),
      receiver_(this, context) {}

void AICreateMonitor::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(receiver_);
}

const AtomicString& AICreateMonitor::InterfaceName() const {
  return event_target_names::kAICreateMonitor;
}

ExecutionContext* AICreateMonitor::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void AICreateMonitor::OnDownloadProgressUpdate(uint64_t downloaded_bytes,
                                               uint64_t total_bytes) {
  CHECK_EQ(total_bytes, kNormalizedDownloadProgressMax);
  // Dispatch a synthetic start event, as needed, for spec compliance.
  if (!(dispatched_start_ |= (downloaded_bytes == 0))) {
    OnDownloadProgressUpdate(0, total_bytes);
  }
  // Refrain from dispatching events after the end event.
  if (!dispatched_end_) {
    const double normalized_downloaded_bytes =
        std::min(downloaded_bytes / static_cast<double>(total_bytes), 1.0);
    DispatchEvent(*ProgressEvent::Create(event_type_names::kDownloadprogress,
                                         true, normalized_downloaded_bytes, 1));
  }
  dispatched_end_ |= (downloaded_bytes >= total_bytes);
}

mojo::PendingRemote<mojom::blink::ModelDownloadProgressObserver>
AICreateMonitor::BindRemote() {
  return receiver_.BindNewPipeAndPassRemote(task_runner_);
}

}  // namespace blink
