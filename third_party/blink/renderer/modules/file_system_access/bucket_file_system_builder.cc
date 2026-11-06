// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/bucket_file_system_builder.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/file_system_access/bucket_file_system_agent.h"

namespace blink {

BucketFileSystemBuilder::BucketFileSystemBuilder(
    BucketFileSystemAgent* agent,
    ExecutionContext* execution_context,
    const String& name,
    FileSystemDirectoryHandle* directory_handle,
    std::unique_ptr<protocol::FileSystem::Backend::GetDirectoryCallback>
        completion_callback)
    : ExecutionContextLifecycleObserver(execution_context),
      agent_(agent),
      directory_name_(name),
      directory_handle_(directory_handle),
      completion_callback_(std::move(completion_callback)),
      receiver_(this, execution_context) {
  nested_directories_ = std::make_unique<protocol::Array<String>>();
  nested_files_ =
      std::make_unique<protocol::Array<protocol::FileSystem::File>>();
  directory_handle_->MojoHandle()->GetEntries(GetListener());
}

void BucketFileSystemBuilder::Dispose() {
  receiver_.reset();
  directory_handle_ = nullptr;
  file_system_handle_queue_.clear();
  completion_callback_.reset();
}

void BucketFileSystemBuilder::DidReadDirectory(
    mojom::blink::FileSystemAccessErrorPtr result,
    Vector<mojom::blink::FileSystemAccessEntryPtr> entries,
    bool has_more_entries) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context || !result) {
    SendFailure(protocol::Response::ServerError("No result or directory."));
    return;
  }

  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    SendFailure(BucketFileSystemAgent::HandleError(std::move(result)));
    return;
  }

  for (auto& entry : entries) {
    file_system_handle_queue_.push_back(*FileSystemHandle::CreateFromMojoEntry(
        std::move(entry), execution_context));
  }

  // Wait until all entries are iterated over and saved before converting
  // entries into `protocol::FileSystem::File` and
  // `protocol::FileSystem::Directory`.
  if (has_more_entries) {
    return;
  }

  receiver_.reset();
  directory_handle_ = nullptr;

  // Add 1 to the barrier count and invoke `barrier_callback` after the loop so
  // `DidBuildDirectory()` (which calls `Dispose()` and clears
  // `file_system_handle_queue_`) cannot run while the loop is still iterating.
  auto barrier_callback =
      base::BarrierClosure(file_system_handle_queue_.size() + 1,
                           BindOnce(&BucketFileSystemBuilder::DidBuildDirectory,
                                    WrapWeakPersistent(this)));

  for (auto entry : file_system_handle_queue_) {
    if (entry->isFile()) {
      // Directly map a file into a `protocol::FileSystem::File` and save it to
      // the internal list of files discovered (`nested_files_`).
      // Some info is only available via the blob. Retrieve it and build the
      // `protocol::FileSystem::File` with it.
      To<FileSystemFileHandle>(*entry).MojoHandle()->AsBlob(blink::BindOnce(
          &BucketFileSystemBuilder::DidBuildFile, WrapWeakPersistent(this),
          entry->name(), barrier_callback));

    } else if (entry->isDirectory()) {
      nested_directories_->emplace_back(entry->name());
      barrier_callback.Run();
    } else {
      // We should never get here, except if in the future another type of
      // Handle is added.
      NOTREACHED();
    }
  }
  barrier_callback.Run();
}

mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryEntriesListener>
BucketFileSystemBuilder::GetListener() {
  ExecutionContext* execution_context = GetExecutionContext();
  CHECK(execution_context);

  auto remote = receiver_.BindNewPipeAndPassRemote(
      execution_context->GetTaskRunner(TaskType::kInternalInspector));

  receiver_.set_disconnect_handler(BindOnce(
      &BucketFileSystemBuilder::OnMojoDisconnect, WrapWeakPersistent(this)));

  return remote;
}

void BucketFileSystemBuilder::ContextDestroyed() {
  SendFailure(protocol::Response::ServerError("Execution context destroyed."));
}

void BucketFileSystemBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(agent_);
  visitor->Trace(directory_handle_);
  visitor->Trace(receiver_);
  visitor->Trace(file_system_handle_queue_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void BucketFileSystemBuilder::DidBuildFile(
    const String& name,
    base::OnceClosure barrier_callback,
    mojom::blink::FileSystemAccessErrorPtr result,
    const base::File::Info& info,
    const scoped_refptr<BlobDataHandle>& blob) {
  if (!completion_callback_) {
    return;
  }

  if (!result) {
    SendFailure(
        protocol::Response::ServerError("Failed to retrieve blob info"));
    return;
  }

  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    SendFailure(BucketFileSystemAgent::HandleError(std::move(result)));
    return;
  }

  if (!blob) {
    SendFailure(
        protocol::Response::ServerError("Failed to retrieve blob info"));
    return;
  }

  std::unique_ptr<protocol::FileSystem::File> file =
      protocol::FileSystem::File::create()
          .setName(name)
          .setLastModified(info.last_modified.InSecondsFSinceUnixEpoch())
          .setSize(info.size)
          .setType(blob->GetType())
          .build();
  nested_files_->emplace_back(std::move(file));
  std::move(barrier_callback).Run();
}

void BucketFileSystemBuilder::DidBuildDirectory() {
  SendSuccess(protocol::FileSystem::Directory::create()
                  .setName(directory_name_)
                  .setNestedDirectories(std::move(nested_directories_))
                  .setNestedFiles(std::move(nested_files_))
                  .build());
}

void BucketFileSystemBuilder::OnMojoDisconnect() {
  SendFailure(protocol::Response::ServerError("Mojo pipe disconnected."));
}

void BucketFileSystemBuilder::SendSuccess(
    std::unique_ptr<protocol::FileSystem::Directory> directory) {
  if (!completion_callback_) {
    return;
  }
  std::exchange(completion_callback_, nullptr)
      ->sendSuccess(std::move(directory));
  Dispose();
  agent_->OnBuilderComplete(this);
}

void BucketFileSystemBuilder::SendFailure(const protocol::Response& response) {
  if (!completion_callback_) {
    return;
  }
  std::exchange(completion_callback_, nullptr)->sendFailure(response);
  Dispose();
  agent_->OnBuilderComplete(this);
}

}  // namespace blink
