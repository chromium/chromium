// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_INTERNAL_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_INTERNAL_OBSERVER_H_

#include "base/observer_list_types.h"
#include "content/common/content_export.h"

namespace content {

class RenderProcessHostImpl;

// An observer API exposing events that are not suitable for content/public.
class CONTENT_EXPORT RenderProcessHostInternalObserver
    : public base::CheckedObserver {
 public:
  // This method is invoked when the observed RenderProcessHost's value of
  // |GetPriority()| changes.
  virtual void RenderProcessPriorityChanged(RenderProcessHostImpl* host) {}

 protected:
  ~RenderProcessHostInternalObserver() override;
};
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_INTERNAL_OBSERVER_H_
