// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_access/storage_access_handle.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/permission_controller.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

// static
void StorageAccessHandle::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::StorageAccessHandle> receiver) {
  CHECK(host);
  // If the Storage Access permission has not been granted then we should refuse
  // to bind this interface. For more see:
  // third_party/blink/renderer/modules/storage_access/README.md
  //
  // NOTE: This handles the general permissions check for the entire interface.
  // Specific binding sights (e.g., IndexedDB) should not need their own
  // additional checks once the StorageAccessHandle interface has been bound.
  blink::mojom::PermissionStatus status =
      host->GetProcess()
          ->GetBrowserContext()
          ->GetPermissionController()
          ->GetPermissionStatusForCurrentDocument(
              blink::PermissionType::STORAGE_ACCESS_GRANT, host);
  if (status != blink::mojom::PermissionStatus::GRANTED) {
    // TODO(crbug.com/1484966): Consider using ReportBadMessage once we have
    // a better sense of race conditions and general stability here.
    return;
  }
  new StorageAccessHandle(*host, std::move(receiver));
}

void StorageAccessHandle::BindIndexedDB(
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  render_frame_host().GetProcess()->BindIndexedDB(
      blink::StorageKey::CreateFirstParty(
          render_frame_host().GetStorageKey().origin()),
      render_frame_host().GetGlobalId(), std::move(receiver));
}

void StorageAccessHandle::BindLocks(
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  render_frame_host().GetProcess()->CreateLockManager(
      blink::StorageKey::CreateFirstParty(
          render_frame_host().GetStorageKey().origin()),
      std::move(receiver));
}

StorageAccessHandle::StorageAccessHandle(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::StorageAccessHandle> receiver)
    : DocumentService(host, std::move(receiver)) {}

StorageAccessHandle::~StorageAccessHandle() = default;

}  // namespace content
