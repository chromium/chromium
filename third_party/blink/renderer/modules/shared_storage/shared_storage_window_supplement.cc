// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_window_supplement.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace blink {

// static
SharedStorageWindowSupplement* SharedStorageWindowSupplement::From(
    LocalDOMWindow& window) {
  SharedStorageWindowSupplement* supplement =
      window.GetSharedStorageWindowSupplement();
  if (!supplement) {
    supplement = MakeGarbageCollected<SharedStorageWindowSupplement>(window);
    window.SetSharedStorageWindowSupplement(supplement);
  }

  return supplement;
}

void SharedStorageWindowSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(shared_storage_document_service_);
  visitor->Trace(local_dom_window_);
}

SharedStorageWindowSupplement::SharedStorageWindowSupplement(
    LocalDOMWindow& window)
    : local_dom_window_(window) {}

mojom::blink::SharedStorageDocumentService*
SharedStorageWindowSupplement::GetSharedStorageDocumentService() {
  if (!shared_storage_document_service_.is_bound()) {
    LocalDOMWindow* window = local_dom_window_;
    LocalFrame* frame = window->GetFrame();
    DCHECK(frame);

    frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
        shared_storage_document_service_.BindNewEndpointAndPassReceiver(
            window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return shared_storage_document_service_.get();
}

}  // namespace blink
