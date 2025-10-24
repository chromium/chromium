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

  // This method is invoked when the process host is being initialized, and the
  // renderer process has been requested to be launched. Note that the channel
  // may or may not have been connected when this is invoked. Use this instead
  // of `OnRenderProcessHostLaunched` when information needs to be sent to the
  // renderer process as soon as possible before e.g. the renderer process
  // commits a navigation, etc. (which can possibly be triggered before the
  // OnRenderProcessLaunched signal if there is no IPC channel pausing).
  // A RenderProcessHost can be reused for a different renderer process (for
  // instance in the case of a renderer process crash). In this case,
  // `OnRenderProcessHostCreated` will be called again for the same
  // `RenderProcessHost` when initializing the new process, without having been
  // destroyed (i.e. `RenderProcessHostObserver::RenderProcessHostDestroyed` is
  // not called).
  // TODO(crbug.com/448511116): Rename this to reflect that this is triggered
  // when initializing and requesting to launch the renderer process, instead
  // of when the `process_host` itself being created.
  virtual void OnRenderProcessHostCreated(RenderProcessHost* process_host) {}

  // This method is invoked when the renderer process for the given host was
  // successfully launched. Use this instead of `OnRenderProcessHostCreated`
  // when information from the launched process is needed, e.g. the PID. Similar
  // to `OnRenderProcessHostCreated`, this can be triggered multiple times for
  // the same host.
  // TODO(crbug.com/448511116): Move this to RenderProcessHostObserver instead.
  virtual void OnRenderProcessLaunched(RenderProcessHost* process_host) {}

  // This method is invoked when a renderer process failed to launch
  // successfully, and `OnRenderProcessHostLaunched` hasn't yet been fired for
  // the `RenderProcessHost`.
  virtual void OnRenderProcessHostCreationFailed(
      RenderProcessHost* host,
      const ChildProcessTerminationInfo& info);

 protected:
  RenderProcessHostCreationObserver();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_CREATION_OBSERVER_H_
