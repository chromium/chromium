// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_OBSERVER_H_

#include "base/observer_list_types.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "content/common/content_export.h"

namespace content {

class RenderProcessHost;
struct ChildProcessTerminationInfo;

// An observer API implemented by classes which are interested
// in RenderProcessHost lifecycle events. Note that this does not allow
// observing the creation of a RenderProcessHost. There is a separate observer
// for that: RenderProcessHostCreationObserver.
class CONTENT_EXPORT RenderProcessHostObserver : public base::CheckedObserver {
 public:
  // This method is invoked when the process was launched and the channel was
  // connected. This is the earliest time it is safe to call Shutdown on the
  // RenderProcessHost.
  virtual void RenderProcessReady(RenderProcessHost* host) {}

  // This method is invoked when the process of the observed RenderProcessHost
  // exits (either normally or with a crash). To determine if the process closed
  // normally or crashed, examine the |status| parameter.
  //
  // A new render process may be spawned for this RenderProcessHost, but there
  // are no guarantees (e.g. if shutdown is occurring, the HostDestroyed
  // callback will happen soon and that will be it, but if the renderer crashed
  // and the user clicks 'reload', a new render process will be spawned).
  //
  // This will cause a call to WebContentsObserver::RenderProcessGone() for the
  // active renderer process for the top-level frame; for code that needs to be
  // a WebContentsObserver anyway, consider whether that API might be a better
  // choice.
  //
  // This is not called in --single-process mode.
  virtual void RenderProcessExited(RenderProcessHost* host,
                                   const ChildProcessTerminationInfo& info) {}

  // This is the equivalent to the `RenderProcessExited` notification above but
  // for --single-process mode only. This is invoked just before calling
  // `RenderProcessHostDestroyed`. Useful for observers that needs the two-step
  // destruction mechanism of RenderProcessHost objects, even in
  // --single--process mode, allowing the logic to be shared between both modes.
  virtual void InProcessRendererExiting(RenderProcessHost* host) {}

  // This method is invoked when the observed RenderProcessHost itself is
  // destroyed. This is guaranteed to be the last call made to the observer, so
  // if the observer is tied to the observed RenderProcessHost, it is safe to
  // delete it.
  virtual void RenderProcessHostDestroyed(RenderProcessHost* host) {}

 protected:
  ~RenderProcessHostObserver() override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_OBSERVER_H_
