// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/bucket_file_system_agent.h"

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/protocol/file_system.h"
#include "third_party/blink/renderer/modules/buckets/storage_bucket.h"
#include "third_party/blink/renderer/modules/buckets/storage_bucket_manager.h"
#include "third_party/blink/renderer/modules/file_system_access/bucket_file_system_builder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

BucketFileSystemAgent::BucketFileSystemAgent(InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

BucketFileSystemAgent::~BucketFileSystemAgent() = default;

// static
protocol::Response BucketFileSystemAgent::HandleError(
    mojom::blink::FileSystemAccessErrorPtr error) {
  if (!error) {
    return protocol::Response::InternalError();
  }

  if (error->status == mojom::blink::FileSystemAccessStatus::kOk) {
    return protocol::Response::Success();
  }

  return protocol::Response::ServerError(error->message.Utf8());
}

void BucketFileSystemAgent::getDirectory(
    std::unique_ptr<protocol::FileSystem::BucketFileSystemLocator>
        file_system_locator,
    std::unique_ptr<protocol::FileSystem::Backend::GetDirectoryCallback>
        callback) {
  String storage_key = file_system_locator->getStorageKey();
  StorageBucket* storage_bucket = GetStorageBucket(
      storage_key, file_system_locator->getBucketName(kDefaultBucketName));
  if (storage_bucket == nullptr) {
    callback->sendFailure(
        protocol::Response::InvalidRequest("Storage Bucket not found."));
    return;
  }

  LocalFrame* frame = inspected_frames_->FrameWithStorageKey(storage_key);
  if (!frame) {
    callback->sendFailure(protocol::Response::ServerError("Frame not found."));
    return;
  }

  ExecutionContext* execution_context =
      frame->DomWindow()->GetExecutionContext();

  Vector<String> path_components;
  for (const auto& component : *file_system_locator->getPathComponents()) {
    path_components.push_back(component);
  }

  String directory_name =
      path_components.empty() ? g_empty_string : path_components.back();
  storage_bucket->GetDirectoryForDevTools(
      execution_context, path_components,
      WTF::BindOnce(&BucketFileSystemAgent::DidGetDirectoryHandle,
                    WrapWeakPersistent(this),
                    WrapWeakPersistent(execution_context), storage_key,
                    directory_name, std::move(callback)));
}

void BucketFileSystemAgent::DidGetDirectoryHandle(
    ExecutionContext* execution_context,
    const String& storage_key,
    const String& directory_name,
    std::unique_ptr<protocol::FileSystem::Backend::GetDirectoryCallback>
        callback,
    mojom::blink::FileSystemAccessErrorPtr result,
    FileSystemDirectoryHandle* handle) {
  if (!result) {
    callback->sendFailure(protocol::Response::ServerError("No result."));
    return;
  }

  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    callback->sendFailure(
        BucketFileSystemAgent::HandleError(std::move(result)));
    return;
  }

  if (!handle) {
    callback->sendFailure(protocol::Response::ServerError("No handle."));
    return;
  }

  BucketFileSystemBuilder::BuildDirectoryTree(
      execution_context, storage_key, directory_name,
      WTF::BindOnce(
          [](std::unique_ptr<
                 protocol::FileSystem::Backend::GetDirectoryCallback> callback,
             mojom::blink::FileSystemAccessErrorPtr result,
             std::unique_ptr<protocol::FileSystem::Directory> directory) {
            if (!result || !directory) {
              callback->sendFailure(
                  protocol::Response::ServerError("No result or directory."));
              return;
            }

            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              callback->sendFailure(
                  BucketFileSystemAgent::HandleError(std::move(result)));
              return;
            }

            callback->sendSuccess(std::move(directory));
          },
          std::move(callback)),
      handle);
}

void BucketFileSystemAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

StorageBucket* BucketFileSystemAgent::GetStorageBucket(
    const String& storage_key,
    const String& bucket_name) {
  LocalFrame* frame = inspected_frames_->FrameWithStorageKey(storage_key);
  if (!frame) {
    return nullptr;
  }

  Navigator* navigator = frame->DomWindow()->navigator();
  StorageBucketManager* storage_bucket_manager =
      StorageBucketManager::storageBuckets(*navigator);
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    return nullptr;
  }
  return storage_bucket_manager->GetBucketForDevtools(script_state,
                                                      bucket_name);
}

}  // namespace blink
