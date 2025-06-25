// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/create_monitor.h"

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

CreateMonitor::CreateMonitor(
    ExecutionContext* context,
    AbortSignal* abort_signal,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      abort_signal_(abort_signal),
      task_runner_(task_runner),
      receiver_(this, context) {}

void CreateMonitor::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(abort_signal_);
  visitor->Trace(receiver_);
}

const AtomicString& CreateMonitor::InterfaceName() const {
  return event_target_names::kCreateMonitor;
}

ExecutionContext* CreateMonitor::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void CreateMonitor::OnDownloadProgressUpdate(uint64_t downloaded_bytes,
                                             uint64_t total_bytes) {
  if (abort_signal_ && abort_signal_->aborted()) {
    return;
  }

  CHECK_EQ(total_bytes, kNormalizedDownloadProgressMax);
  CHECK_LE(downloaded_bytes, kNormalizedDownloadProgressMax);
  CHECK_GE(downloaded_bytes, 0u);

  bool first_update = !last_downloaded_bytes_.has_value();

  if (!first_update && downloaded_bytes <= last_downloaded_bytes_) {
    return;
  }

  last_downloaded_bytes_ = downloaded_bytes;

  if (first_update) {
    // The first update should always be zero.
    CHECK_EQ(downloaded_bytes, 0u);
  }

  const double normalized_downloaded_bytes =
      std::min(downloaded_bytes / static_cast<double>(total_bytes), 1.0);
  DispatchEvent(*ProgressEvent::Create(event_type_names::kDownloadprogress,
                                       true, normalized_downloaded_bytes, 1));
}

mojo::PendingRemote<mojom::blink::ModelDownloadProgressObserver>
CreateMonitor::BindRemote() {
  return receiver_.BindNewPipeAndPassRemote(task_runner_);
}

}  // namespace blink
