// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/web_launch_service_impl.h"

#include <memory>

#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/modules/launch/dom_window_launch_queue.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

void WebLaunchServiceImpl::Create(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::WebLaunchService> receiver) {
  DCHECK(frame);

  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<WebLaunchServiceImpl>(*frame->DomWindow()),
      std::move(receiver));
}

WebLaunchServiceImpl::WebLaunchServiceImpl(LocalDOMWindow& window)
    : window_(window) {}

WebLaunchServiceImpl::~WebLaunchServiceImpl() = default;

void WebLaunchServiceImpl::SetLaunchFiles(
    WTF::Vector<mojom::blink::FileSystemAccessEntryPtr> entries) {
  if (!window_)
    return;

  HeapVector<Member<FileSystemHandle>> files;
  for (auto& entry : entries) {
    files.push_back(FileSystemHandle::CreateFromMojoEntry(
        std::move(entry), window_->GetExecutionContext()));
  }

  DOMWindowLaunchQueue::UpdateLaunchFiles(window_, std::move(files));
}

}  // namespace blink
