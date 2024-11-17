
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fileapi/file_backed_blob_factory_dispatcher.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/blob/file_backed_blob_factory.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {
namespace {

mojom::blink::FileBackedBlobFactory* GetFileBackedBlobFactory(
    HeapMojoRemote<mojom::blink::FileBackedBlobFactory>& remote,
    ExecutionContext* execution_context) {
  if (!remote.is_bound()) {
    mojo::PendingReceiver<mojom::blink::FileBackedBlobFactory> receiver =
        remote.BindNewPipeAndPassReceiver(execution_context->GetTaskRunner(
            blink::TaskType::kMiscPlatformAPI));
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        std::move(receiver));
  }
  return remote.get();
}

mojom::blink::FileBackedBlobFactory* GetFileBackedBlobFactory(
    HeapMojoAssociatedRemote<mojom::blink::FileBackedBlobFactory>& remote,
    ExecutionContext* execution_context,
    LocalFrame* frame) {
  if (!remote.is_bound()) {
    mojo::PendingAssociatedReceiver<mojom::blink::FileBackedBlobFactory>
        receiver = remote.BindNewEndpointAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
    frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
        std::move(receiver));
  }
  return remote.get();
}

}  // namespace

FileBackedBlobFactoryDispatcher::FileBackedBlobFactoryDispatcher(
    ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      ExecutionContextClient(&context),
      frame_remote_(&context),
      worker_remote_(&context) {}

// static
mojom::blink::FileBackedBlobFactory*
FileBackedBlobFactoryDispatcher::GetFileBackedBlobFactory(
    ExecutionContext* context) {
  if (!context) {
    return nullptr;
  }
  return From(*context)->GetFileBackedBlobFactory();
}

void FileBackedBlobFactoryDispatcher::SetFileBackedBlobFactoryForTesting(
    mojo::PendingAssociatedRemote<mojom::blink::FileBackedBlobFactory>
        factory) {
  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return;
  }

  frame_remote_.Bind(std::move(factory), execution_context->GetTaskRunner(
                                             TaskType::kMiscPlatformAPI));
}

void FileBackedBlobFactoryDispatcher::FlushForTesting() {
  if (frame_remote_.is_bound()) {
    frame_remote_.FlushForTesting();
  }
  if (worker_remote_.is_bound()) {
    worker_remote_.FlushForTesting();
  }
}

void FileBackedBlobFactoryDispatcher::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(frame_remote_);
  visitor->Trace(worker_remote_);
}

// static
FileBackedBlobFactoryDispatcher* FileBackedBlobFactoryDispatcher::From(
    ExecutionContext& context) {
  auto* dispatcher =
      Supplement<ExecutionContext>::From<FileBackedBlobFactoryDispatcher>(
          &context);
  if (!dispatcher) {
    dispatcher = MakeGarbageCollected<FileBackedBlobFactoryDispatcher>(context);
    Supplement<ExecutionContext>::ProvideTo(context, dispatcher);
  }
  return dispatcher;
}

mojom::blink::FileBackedBlobFactory*
FileBackedBlobFactoryDispatcher::GetFileBackedBlobFactory() {
  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return nullptr;
  }

  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    if (auto* frame = window->GetFrame()) {
      return blink::GetFileBackedBlobFactory(frame_remote_, execution_context,
                                             frame);
    } else {
      return nullptr;
    }
  }
  return blink::GetFileBackedBlobFactory(worker_remote_, execution_context);
}

// static
const char FileBackedBlobFactoryDispatcher::kSupplementName[] =
    "FileBackedBlobFactoryDispatcher";

}  // namespace blink
