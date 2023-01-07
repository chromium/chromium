// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/web_launch_service_impl.h"

#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/launch/dom_window_launch_queue.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {
// static
const char WebLaunchServiceImpl::kSupplementName[] = "WebLaunchServiceImpl";

// static
WebLaunchServiceImpl* WebLaunchServiceImpl::From(LocalDOMWindow& window) {
  return Supplement<LocalDOMWindow>::From<WebLaunchServiceImpl>(window);
}

// static
void WebLaunchServiceImpl::BindReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::WebLaunchService> receiver) {
  DCHECK(frame);
  auto* service = WebLaunchServiceImpl::From(*frame->DomWindow());
  if (!service) {
    service = MakeGarbageCollected<WebLaunchServiceImpl>(
        base::PassKey<WebLaunchServiceImpl>(), *frame->DomWindow());
    Supplement<LocalDOMWindow>::ProvideTo(*frame->DomWindow(), service);
  }
  service->Bind(std::move(receiver));
}

WebLaunchServiceImpl::WebLaunchServiceImpl(base::PassKey<WebLaunchServiceImpl>,
                                           LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), receiver_(this, &window) {}

WebLaunchServiceImpl::~WebLaunchServiceImpl() = default;

void WebLaunchServiceImpl::Bind(
    mojo::PendingAssociatedReceiver<mojom::blink::WebLaunchService> receiver) {
  // This interface only has a single method with no reply. The calling side
  // doesn't keep this around, so it is re-requested on demand every time;
  // however, there should never be multiple callers bound at a time.
  receiver_.reset();
  receiver_.Bind(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kMiscPlatformAPI));
}

void WebLaunchServiceImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void WebLaunchServiceImpl::SetLaunchFiles(
    WTF::Vector<mojom::blink::FileSystemAccessEntryPtr> entries) {
  HeapVector<Member<FileSystemHandle>> files;
  for (auto& entry : entries) {
    files.push_back(FileSystemHandle::CreateFromMojoEntry(
        std::move(entry), GetSupplementable()->GetExecutionContext()));
  }

  UseCounter::Count(GetSupplementable()->GetExecutionContext(),
                    WebFeature::kFileHandlingLaunch);
  DOMWindowLaunchQueue::UpdateLaunchFiles(GetSupplementable(),
                                          std::move(files));
}

void WebLaunchServiceImpl::EnqueueLaunchParams(const KURL& launch_url) {
  DOMWindowLaunchQueue::EnqueueLaunchParams(GetSupplementable(), launch_url);
}

}  // namespace blink
