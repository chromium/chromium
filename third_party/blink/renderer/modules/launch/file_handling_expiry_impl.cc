// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/file_handling_expiry_impl.h"

#include <memory>

#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/modules/launch/dom_window_launch_queue.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

// static
const char FileHandlingExpiryImpl::kSupplementName[] = "FileHandlingExpiryImpl";

// static
FileHandlingExpiryImpl* FileHandlingExpiryImpl::From(LocalDOMWindow& window) {
  return Supplement<LocalDOMWindow>::From<FileHandlingExpiryImpl>(window);
}

// static
void FileHandlingExpiryImpl::BindReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::FileHandlingExpiry>
        receiver) {
  DCHECK(frame && frame->DomWindow());

  auto* expiry_service = FileHandlingExpiryImpl::From(*frame->DomWindow());
  if (!expiry_service) {
    expiry_service = MakeGarbageCollected<FileHandlingExpiryImpl>(
        base::PassKey<FileHandlingExpiryImpl>(), frame->DomWindow());
    Supplement<LocalDOMWindow>::ProvideTo(*frame->DomWindow(), expiry_service);
  }
  expiry_service->Bind(std::move(receiver));
}

FileHandlingExpiryImpl::FileHandlingExpiryImpl(
    base::PassKey<FileHandlingExpiryImpl>,
    LocalDOMWindow* window)
    : Supplement<LocalDOMWindow>(*window), receivers_(this, window) {}

void FileHandlingExpiryImpl::Bind(
    mojo::PendingAssociatedReceiver<mojom::blink::FileHandlingExpiry>
        receiver) {
  receivers_.Add(std::move(receiver),
                 GetSupplementable()->GetFrame()->GetTaskRunner(
                     TaskType::kMiscPlatformAPI));
}

void FileHandlingExpiryImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receivers_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

FileHandlingExpiryImpl::~FileHandlingExpiryImpl() = default;

void FileHandlingExpiryImpl::RequestOriginTrialExpiryTime(
    RequestOriginTrialExpiryTimeCallback callback) {
  auto* origin_trials =
      GetSupplementable()->GetExecutionContext()->GetOriginTrialContext();

  base::Time expiry_time =
      origin_trials->GetFeatureExpiry(OriginTrialFeature::kFileHandling);
  std::move(callback).Run(expiry_time);
}

}  // namespace blink
