
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
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// static
mojom::blink::FileBackedBlobFactory*
FileBackedBlobFactoryDispatcher::GetFileBackedBlobFactory(
    ExecutionContext* context) {
  if (!context) {
    return nullptr;
  }
  return From(*context)->GetFileBackedBlobFactory();
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

FileBackedBlobFactoryDispatcher::FileBackedBlobFactoryDispatcher(
    ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      ExecutionContextClient(&context),
      remote_(&context) {}

mojom::blink::FileBackedBlobFactory*
FileBackedBlobFactoryDispatcher::GetFileBackedBlobFactory() {
  if (!base::FeatureList::IsEnabled(
          blink::features::kEnableFileBackedBlobFactory)) {
    return nullptr;
  }

  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return nullptr;
  }

  if (!remote_.is_bound()) {
    if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
      if (auto* frame = window->GetFrame()) {
        mojo::PendingAssociatedReceiver<mojom::blink::FileBackedBlobFactory>
            receiver = remote_.BindNewEndpointAndPassReceiver(
                execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
        frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
            std::move(receiver));
      }
    }
  }

  if (remote_.is_bound()) {
    return remote_.get();
  }

  // TODO(b/288508845): Currently we are only handling a frame context, and by
  // returning a nullptr here we fallback to a BlobRegistry registration. We
  // probably want to stop relying on BlobRegistry at some point.
  return nullptr;
}

void FileBackedBlobFactoryDispatcher::SetFileBackedBlobFactoryForTesting(
    mojo::PendingAssociatedRemote<mojom::blink::FileBackedBlobFactory>
        factory) {
  auto* execution_context = GetExecutionContext();
  if (!execution_context) {
    return;
  }

  remote_.Bind(std::move(factory),
               GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void FileBackedBlobFactoryDispatcher::FlushForTesting() {
  remote_.FlushForTesting();
}

void FileBackedBlobFactoryDispatcher::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(remote_);
}

// static
const char FileBackedBlobFactoryDispatcher::kSupplementName[] =
    "FileBackedBlobFactoryDispatcher";

}  // namespace blink
