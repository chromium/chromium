// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_observation_collection.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_observation.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_observer.h"

namespace blink {

// static
const char FileSystemObservationCollection::kSupplementName[] =
    "FileSystemObservationCollection";

// static
FileSystemObservationCollection* FileSystemObservationCollection::From(
    ExecutionContext* context) {
  DCHECK(context);
  DCHECK(context->IsContextThread());

  FileSystemObservationCollection* data =
      Supplement<ExecutionContext>::From<FileSystemObservationCollection>(
          context);
  if (!data) {
    data = MakeGarbageCollected<FileSystemObservationCollection>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, data);
  }

  return data;
}

FileSystemObservationCollection::FileSystemObservationCollection(
    ExecutionContext& context)
    : Supplement<ExecutionContext>(context), execution_context_(context) {}

void FileSystemObservationCollection::AddObservation(
    FileSystemObserver* observer,
    mojo::PendingReceiver<mojom::blink::FileSystemAccessObserver>
        observer_receiver) {
  if (!observation_map_.Contains(observer)) {
    observation_map_.insert(
        observer,
        MakeGarbageCollected<HeapHashSet<Member<FileSystemObservation>>>());
  }
  observation_map_.at(observer)->insert(
      MakeGarbageCollected<FileSystemObservation>(
          execution_context_, observer, std::move(observer_receiver)));
}

void FileSystemObservationCollection::RemoveObservation(
    FileSystemObserver* observer,
    FileSystemObservation* observation) {
  if (!observation_map_.Contains(observer)) {
    return;
  }

  observation_map_.at(observer)->erase(observation);

  // Remove the observer if it has no more observations.
  if (observation_map_.at(observer)->empty()) {
    observation_map_.erase(observer);
  }
}

void FileSystemObservationCollection::RemoveObserver(
    FileSystemObserver* observer) {
  if (!observation_map_.Contains(observer)) {
    return;
  }

  // Explicitly disconnect all observation receivers for the observer. This
  // prevents file changes arriving before the observation can be garbage
  // collected.
  for (auto& observation : *observation_map_.at(observer)) {
    observation->DisconnectReceiver();
  }
  observation_map_.erase(observer);
}

void FileSystemObservationCollection::Trace(Visitor* visitor) const {
  visitor->Trace(observation_map_);
  visitor->Trace(execution_context_);
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
