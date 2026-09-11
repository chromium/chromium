// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/holding_blocking_idb_lock_handle.h"

#include <utility>

#include "base/check.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

HoldingBlockingIDBLockHandle::HoldingBlockingIDBLockHandle() = default;

HoldingBlockingIDBLockHandle::HoldingBlockingIDBLockHandle(
    HoldingBlockingIDBLockHandle&& other) = default;

HoldingBlockingIDBLockHandle& HoldingBlockingIDBLockHandle::operator=(
    HoldingBlockingIDBLockHandle&& other) {
  if (this != &other) {
    Reset();
    render_frame_host_ = std::move(other.render_frame_host_);
  }
  return *this;
}

HoldingBlockingIDBLockHandle::HoldingBlockingIDBLockHandle(
    RenderFrameHostImpl* render_frame_host)
    : render_frame_host_(render_frame_host->GetWeakPtr()) {
  CHECK(render_frame_host_);
  render_frame_host_->OnStartHoldingBlockingIDBLock();
}

HoldingBlockingIDBLockHandle::~HoldingBlockingIDBLockHandle() {
  Reset();
}

bool HoldingBlockingIDBLockHandle::IsValid() const {
  return render_frame_host_.get();
}

void HoldingBlockingIDBLockHandle::Reset() {
  if (render_frame_host_) {
    render_frame_host_->OnStopHoldingBlockingIDBLock();
  }
  render_frame_host_ = nullptr;
}

}  // namespace content
