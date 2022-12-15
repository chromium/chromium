// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_OBSERVER_H_

#include "base/observer_list_types.h"
#include "content/common/content_export.h"

namespace content {

// An observer API implemented by classes which would like to observe
// RenderFrameHost state changed events.
//
// This API is appropriate for observer classes extending
// |content::DocumentUserData| (which have a 1-1 relationship and are owned by
// RenderFrameHost) to track the state of a single RenderFrameHost instead of
// the whole frame tree (see WebContentsObserver::RenderFrameHostStateChanged)
class CONTENT_EXPORT RenderFrameHostObserver : public base::CheckedObserver {
 public:
  // This method is invoked whenever the RenderFrameHost enters
  // BackForwardCache.
  virtual void DidEnterBackForwardCache() {}

  // This method is invoked whenever the RenderFrameHost is restored from
  // BackForwardCache.
  virtual void DidRestoreFromBackForwardCache() {}

 protected:
  ~RenderFrameHostObserver() override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_OBSERVER_H_