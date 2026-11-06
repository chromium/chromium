// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_BUCKET_FILE_SYSTEM_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_BUCKET_FILE_SYSTEM_BUILDER_H_

#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/inspector/protocol/file_system.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

class BucketFileSystemAgent;

// This class receives a directory or file handle and builds a corresponding
// `protocol::FileSystem::Directory` or `protocol::FileSystem::File` object.
class MODULES_EXPORT BucketFileSystemBuilder final
    : public GarbageCollected<BucketFileSystemBuilder>,
      public ExecutionContextLifecycleObserver,
      public mojom::blink::FileSystemAccessDirectoryEntriesListener {
 public:
  BucketFileSystemBuilder(
      BucketFileSystemAgent* agent,
      ExecutionContext* execution_context,
      const String& name,
      FileSystemDirectoryHandle* directory_handle,
      std::unique_ptr<protocol::FileSystem::Backend::GetDirectoryCallback>
          completion_callback);

  void Dispose();

  // mojom::blink::FileSystemAccessDirectoryEntriesListener
  void DidReadDirectory(mojom::blink::FileSystemAccessErrorPtr result,
                        Vector<mojom::blink::FileSystemAccessEntryPtr> entries,
                        bool has_more_entries) override;

  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  // GarbageCollected
  void Trace(Visitor* visitor) const override;

 private:
  mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryEntriesListener>
  GetListener();

  // This method is called for each file seen to track the `nested_files`
  // encountered.
  void DidBuildFile(const String& name,
                    base::OnceClosure barrier_callback,
                    mojom::blink::FileSystemAccessErrorPtr result,
                    const base::File::Info& info,
                    const scoped_refptr<BlobDataHandle>& blob);

  // This method is called when all the entries have been iterated on and the
  // `nested_files_` and `nested_directories_` are fully created. It builds the
  // `protocol::FileSystem::Directory` and sends it via `completion_callback_`.
  void DidBuildDirectory();

  void OnMojoDisconnect();

  void SendSuccess(std::unique_ptr<protocol::FileSystem::Directory> directory);
  void SendFailure(const protocol::Response& response);

  Member<BucketFileSystemAgent> agent_;
  const String directory_name_;
  // Retained to keep the directory handle's HeapMojoRemote alive while
  // GetEntries() is in flight.
  Member<FileSystemDirectoryHandle> directory_handle_;
  HeapDeque<Member<FileSystemHandle>> file_system_handle_queue_;
  std::unique_ptr<protocol::FileSystem::Backend::GetDirectoryCallback>
      completion_callback_;
  std::unique_ptr<protocol::Array<String>> nested_directories_;
  std::unique_ptr<protocol::Array<protocol::FileSystem::File>> nested_files_;
  HeapMojoReceiver<mojom::blink::FileSystemAccessDirectoryEntriesListener,
                   BucketFileSystemBuilder>
      receiver_;
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_BUCKET_FILE_SYSTEM_BUILDER_H_
