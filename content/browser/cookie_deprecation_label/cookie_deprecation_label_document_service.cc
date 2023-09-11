// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_deprecation_label/cookie_deprecation_label_document_service.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "content/browser/cookie_deprecation_label/cookie_deprecation_label_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// static
void CookieDeprecationLabelDocumentService::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CookieDeprecationLabelDocumentService>
        receiver) {
  CHECK(render_frame_host);

  // The object is bound to the lifetime of `render_frame_host`'s logical
  // document by virtue of being a `DocumentService` implementation.
  new CookieDeprecationLabelDocumentService(*render_frame_host,
                                            std::move(receiver));
}

CookieDeprecationLabelDocumentService::CookieDeprecationLabelDocumentService(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::CookieDeprecationLabelDocumentService>
        receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

CookieDeprecationLabelDocumentService::
    ~CookieDeprecationLabelDocumentService() = default;

void CookieDeprecationLabelDocumentService::GetValue(
    GetValueCallback callback) {
  auto* label_manager = static_cast<StoragePartitionImpl*>(
                            render_frame_host().GetStoragePartition())
                            ->GetCookieDeprecationLabelManager();
  if (!label_manager) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(label_manager->GetValue(
      render_frame_host().GetMainFrame()->GetLastCommittedOrigin(),
      render_frame_host().GetLastCommittedOrigin()));
}

}  // namespace content
