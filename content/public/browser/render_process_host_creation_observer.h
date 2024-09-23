// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_CREATION_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_CREATION_OBSERVER_H_

#include "content/common/content_export.h"

namespace content {

class RenderProcessHost;
struct ChildProcessTerminationInfo;

// An observer that gets notified any time a new RenderProcessHost is created.
// This can only be used on the UI thread.
class CONTENT_EXPORT RenderProcessHostCreationObserver {
 public:
  RenderProcessHostCreationObserver(const RenderProcessHostCreationObserver&) =
      delete;
  RenderProcessHostCreationObserver& operator=(
      const RenderProcessHostCreationObserver&) = delete;

  virtual ~RenderProcessHostCreationObserver();

  // This method is invoked when the process was successfully launched. Note
  // that the channel may or may not have been connected when this is invoked.
  // A RenderProcessHost can be reused for a different renderer process (for
  // instance in the case of a renderer process crash). In this case,
  // `OnRenderProcessHostCreated` will be called again for the same
  // `RenderProcessHost` when the new process is launched, without having been
  // destroyed (i.e. `RenderProcessHostObserver::RenderProcessHostDestroyed` is
  // not called).
  virtual void OnRenderProcessHostCreated(RenderProcessHost* process_host) = 0;

  // This method is invoked when a renderer process failed to launch
  // successfully, and `OnRenderProcessHostCreated` hasn't yet been fired for
  // the `RenderProcessHost`.
  virtual void OnRenderProcessHostCreationFailed(
      RenderProcessHost* host,
      const ChildProcessTerminationInfo& info);

 protected:
  RenderProcessHostCreationObserver();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_CREATION_OBSERVER_H_
