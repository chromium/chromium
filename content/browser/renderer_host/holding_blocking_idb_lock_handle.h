// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_HOLDING_BLOCKING_IDB_LOCK_HANDLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_HOLDING_BLOCKING_IDB_LOCK_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHostImpl;

// Used to notify that a document is blocking another IDB transaction, which
// means that it must not be frozen.
class CONTENT_EXPORT HoldingBlockingIDBLockHandle {
 public:
  HoldingBlockingIDBLockHandle();
  HoldingBlockingIDBLockHandle(HoldingBlockingIDBLockHandle&& other);
  HoldingBlockingIDBLockHandle& operator=(HoldingBlockingIDBLockHandle&& other);
  ~HoldingBlockingIDBLockHandle();

  HoldingBlockingIDBLockHandle(const HoldingBlockingIDBLockHandle&) = delete;
  HoldingBlockingIDBLockHandle& operator=(const HoldingBlockingIDBLockHandle&) =
      delete;

  bool IsValid() const;

  void Reset();

 private:
  friend class RenderFrameHostImpl;
  explicit HoldingBlockingIDBLockHandle(RenderFrameHostImpl* render_frame_host);

  base::WeakPtr<RenderFrameHostImpl> render_frame_host_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_HOLDING_BLOCKING_IDB_LOCK_HANDLE_H_
