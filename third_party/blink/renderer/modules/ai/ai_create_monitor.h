// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_CREATE_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_CREATE_MONITOR_H_

#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

// The monitor class that serves as the target for the `downloadprogress` event.
class AICreateMonitor final
    : public EventTarget,
      public ExecutionContextClient,
      public mojom::blink::ModelDownloadProgressObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AICreateMonitor(ExecutionContext* context,
                  scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~AICreateMonitor() override = default;

  void Trace(Visitor* visitor) const override;

  // EventTarget implementation
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // mojom::blink::ModelDownloadProgressObserver implementation
  void OnDownloadProgressUpdate(uint64_t downloaded_bytes,
                                uint64_t total_bytes) override;

  mojo::PendingRemote<mojom::blink::ModelDownloadProgressObserver> BindRemote();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(downloadprogress, kDownloadprogress)

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoReceiver<mojom::blink::ModelDownloadProgressObserver, AICreateMonitor>
      receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_CREATE_MONITOR_H_
