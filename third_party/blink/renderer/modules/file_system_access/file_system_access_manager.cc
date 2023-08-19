// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_manager.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

// static
const char FileSystemAccessManager::kSupplementName[] =
    "FileSystemAccessManager";

// static
FileSystemAccessManager& FileSystemAccessManager::From(
    ExecutionContext* context) {
  FileSystemAccessManager* manager =
      Supplement<ExecutionContext>::From<FileSystemAccessManager>(context);
  if (!manager) {
    manager = MakeGarbageCollected<FileSystemAccessManager>(context);
    Supplement<ExecutionContext>::ProvideTo(*context, manager);
  }
  manager->EnsureConnection();
  return *manager;
}

FileSystemAccessManager::FileSystemAccessManager(ExecutionContext* context)
    : Supplement<ExecutionContext>(*context),
      ExecutionContextClient(context),
      remote_(context) {}

void FileSystemAccessManager::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void FileSystemAccessManager::EnsureConnection() {
  CHECK(GetExecutionContext());

  if (remote_.is_bound()) {
    return;
  }

  auto task_runner = GetExecutionContext()->GetTaskRunner(TaskType::kStorage);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_.BindNewPipeAndPassReceiver(std::move(task_runner)));
}

}  // namespace blink
