// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_observation.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_change_record.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_observation_collection.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_observer.h"

namespace blink {

FileSystemObservation::FileSystemObservation(
    ExecutionContext* context,
    FileSystemObserver* observer,
    mojo::PendingReceiver<mojom::blink::FileSystemAccessObserver>
        observation_receiver)
    : observer_(observer),
      execution_context_(context),
      receiver_(this, context) {
  CHECK(execution_context_);
  receiver_.Bind(std::move(observation_receiver),
                 execution_context_->GetTaskRunner(TaskType::kStorage));

  // Add a disconnect handler so we can cleanup appropriately.
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &FileSystemObservation::OnRemoteDisconnected, WrapWeakPersistent(this)));
}

void FileSystemObservation::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(execution_context_);
  visitor->Trace(receiver_);
}

void FileSystemObservation::DisconnectReceiver() {
  receiver_.reset();
}

void FileSystemObservation::OnFileChanges(
    WTF::Vector<mojom::blink::FileSystemAccessChangePtr> mojo_changes) {
  observer_->OnFileChanges(std::move(mojo_changes));
}

void FileSystemObservation::OnRemoteDisconnected() {
  FileSystemObservationCollection::From(execution_context_)
      ->RemoveObservation(observer_, this);
}

}  // namespace blink
