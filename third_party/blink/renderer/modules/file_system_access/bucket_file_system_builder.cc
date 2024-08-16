// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/bucket_file_system_builder.h"

#include "base/barrier_closure.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink-forward.h"

namespace blink {

// static
void BucketFileSystemBuilder::BuildDirectoryTree(
    ExecutionContext* execution_context,
    const String storage_key,
    const String name,
    DirectoryCallback callback,
    FileSystemDirectoryHandle* handle) {
  CHECK(handle);

  // Create a GarbageCollected instance of this class to iterate of the
  // directory handle. Once the iteration is completed, the completion callback
  // will be called with the `protocol::FileSystem::Directory` object.
  BucketFileSystemBuilder* builder =
      MakeGarbageCollected<BucketFileSystemBuilder>(
          execution_context, storage_key, name, std::move(callback));

  handle->MojoHandle()->GetEntries(builder->GetListener());
}

BucketFileSystemBuilder::BucketFileSystemBuilder(
    ExecutionContext* execution_context,
    const String storage_key,
    const String name,
    DirectoryCallback completion_callback)
    : ExecutionContextClient(execution_context),
      storage_key_(storage_key),
      directory_name_(name),
      /*file_system_handle_queue_(
          std::make_unique<WTF::Deque<FileSystemHandle*>>()),*/
      completion_callback_(std::move(completion_callback)),
      receiver_(this, execution_context) {
  nested_directories_ = std::make_unique<protocol::Array<String>>();
  nested_files_ =
      std::make_unique<protocol::Array<protocol::FileSystem::File>>();
}

void BucketFileSystemBuilder::DidReadDirectory(
    mojom::blink::FileSystemAccessErrorPtr result,
    Vector<mojom::blink::FileSystemAccessEntryPtr> entries,
    bool has_more_entries) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context || !result ||
      result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    std::move(completion_callback_)
        .Run(std::move(result),
             /*std::unique_ptr<protocol::FileSystem::Directory>*/ nullptr);
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

  self_keep_alive_.Clear();

  auto barrier_callback = base::BarrierClosure(
      file_system_handle_queue_.size(),
      WTF::BindOnce(&BucketFileSystemBuilder::DidBuildDirectory,
                    WrapWeakPersistent(this)));

  for (auto entry : file_system_handle_queue_) {
    if (entry->isFile()) {
      // Directly map a file into a `protocol::FileSystem::File` and save it to
      // the internal list of files discovered (`nested_files_`).

      // Some info is only available via the blob. Retrieve it and build the
      // `protocol::FileSystem::File` with it.
      To<FileSystemFileHandle>(*entry).MojoHandle()->AsBlob(WTF::BindOnce(
          [](String name,
             base::OnceCallback<void(
                 mojom::blink::FileSystemAccessErrorPtr,
                 std::unique_ptr<protocol::FileSystem::File>)> callback,
             mojom::blink::FileSystemAccessErrorPtr result,
             const base::File::Info& info,
             const scoped_refptr<BlobDataHandle>& blob) {
            if (!result ||
                result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              std::move(callback).Run(
                  std::move(result),
                  /*std::unique_ptr<protocol::FileSystem::File>*/ nullptr);
              return;
            }

            std::unique_ptr<protocol::FileSystem::File> file =
                protocol::FileSystem::File::create()
                    .setName(name)
                    .setLastModified(
                        info.last_modified.InSecondsFSinceUnixEpoch())
                    .setSize(info.size)
                    .setType(blob->GetType())
                    .build();
            std::move(callback).Run(std::move(result), std::move(file));
          },
          entry->name(),
          WTF::BindOnce(&BucketFileSystemBuilder::DidBuildFile,
                        WrapPersistent(this), barrier_callback)));

    } else if (entry->isDirectory()) {
      nested_directories_->emplace_back(entry->name());
      barrier_callback.Run();
    } else {
      // We should never get here, except if in the future another type of
      // Handle is added.
      NOTREACHED();
    }
  }
}

mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryEntriesListener>
BucketFileSystemBuilder::GetListener() {
  ExecutionContext* execution_context = GetExecutionContext();
  CHECK(execution_context);

  auto remote = receiver_.BindNewPipeAndPassRemote(
      execution_context->GetTaskRunner(TaskType::kInternalInspector));

  // `this` needs to be prevented from being garbage collected while
  // waiting for `DidReadDirectory()` callbacks from the browser, so
  // use self-referential GC root to pin this in memory. To reduce the
  // possibility of an implementation bug introducing a memory leak,
  // also clear the self reference if the Mojo pipe is disconnected.
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &BucketFileSystemBuilder::OnMojoDisconnect, WrapWeakPersistent(this)));

  return remote;
}

void BucketFileSystemBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(file_system_handle_queue_);
  ExecutionContextClient::Trace(visitor);
}

void BucketFileSystemBuilder::DidBuildFile(
    base::OnceClosure barrier_callback,
    mojom::blink::FileSystemAccessErrorPtr result,
    std::unique_ptr<protocol::FileSystem::File> file) {
  if (!result || !file) {
    std::move(completion_callback_)
        .Run(mojom::blink::FileSystemAccessError::New(
                 mojom::blink::FileSystemAccessStatus::kInvalidState,
                 base::File::Error::FILE_ERROR_FAILED,
                 "Failed to retrieve blob info"),
             /*std::unique_ptr<protocol::FileSystem::Directory>*/ nullptr);
    return;
  }

  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    std::move(completion_callback_)
        .Run(std::move(result),
             /*std::unique_ptr<protocol::FileSystem::Directory>*/ nullptr);
    return;
  }

  nested_files_->emplace_back(std::move(file));
  std::move(barrier_callback).Run();
}

void BucketFileSystemBuilder::DidBuildDirectory() {
  std::move(completion_callback_)
      .Run(mojom::blink::FileSystemAccessError::New(
               mojom::blink::FileSystemAccessStatus::kOk, base::File::FILE_OK,
               /*message=*/""),
           protocol::FileSystem::Directory::create()
               .setName(directory_name_)
               .setNestedDirectories(std::move(nested_directories_))
               .setNestedFiles(std::move(nested_files_))
               .build());
}

void BucketFileSystemBuilder::OnMojoDisconnect() {
  self_keep_alive_.Clear();
}

}  // namespace blink
