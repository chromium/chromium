// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/lock_screen/lock_screen_service_impl.h"

#include <map>
#include <memory>

#include "content/browser/lock_screen/lock_screen_storage_impl.h"
#include "content/public/browser/lock_screen_storage.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace content {

LockScreenServiceImpl::LockScreenServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::LockScreenService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      lock_screen_storage_(LockScreenStorageImpl::GetInstance()) {}

LockScreenServiceImpl::~LockScreenServiceImpl() = default;

// static
void LockScreenServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::LockScreenService> receiver) {
  CHECK(render_frame_host);
  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new LockScreenServiceImpl(*render_frame_host, std::move(receiver));
}

void LockScreenServiceImpl::GetKeys(GetKeysCallback callback) {
  if (!IsAllowed()) {
    std::move(callback).Run(std::vector<std::string>());
    return;
  }
  lock_screen_storage_->GetKeys(origin(), std::move(callback));
}

void LockScreenServiceImpl::SetData(const std::string& key,
                                    const std::string& data,
                                    SetDataCallback callback) {
  if (!IsAllowed()) {
    std::move(callback).Run(
        blink::mojom::LockScreenServiceStatus::kNotAllowedFromContext);
    return;
  }
  lock_screen_storage_->SetData(origin(), key, data, std::move(callback));
}

bool LockScreenServiceImpl::IsAllowed() {
  // TODO(crbug.com/40810036): Ideally we wouldn't even need to bind the
  // interface in the cases below.
  if (origin().opaque())
    return false;
  return lock_screen_storage_->IsAllowedBrowserContext(
      render_frame_host().GetProcess()->GetBrowserContext());
}

}  // namespace content
