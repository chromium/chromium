// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_OBSERVER_H_

#include "base/process/kill.h"
#include "content/common/content_export.h"

namespace content {

class DevToolsAgentHost;

// Observer API notifies interested parties about changes in DevToolsAgentHosts.
class CONTENT_EXPORT DevToolsAgentHostObserver {
 public:
  virtual ~DevToolsAgentHostObserver();

  // If observer returns |true|, DevToolsAgentHost instances are created
  // (and reported in DevToolsAgentHostCreated) for every possible devtools
  // target (e.g. WebContents).
  virtual bool ShouldForceDevToolsAgentHostCreation();

  // Called when DevToolsAgentHost was created and is ready to be used.
  virtual void DevToolsAgentHostCreated(DevToolsAgentHost* agent_host);

  // Called when DevToolsAgentHost was created and is ready to be used.
  virtual void DevToolsAgentHostNavigated(DevToolsAgentHost* agent_host);

  // Called when a process associated with inspected target has changed.
  virtual void DevToolsAgentHostProcessChanged(DevToolsAgentHost* agent_host);

  // Called when client has attached to DevToolsAgentHost.
  virtual void DevToolsAgentHostAttached(DevToolsAgentHost* agent_host);

  // Called when client has detached from DevToolsAgentHost.
  virtual void DevToolsAgentHostDetached(DevToolsAgentHost* agent_host);

  // Called when DevToolsAgentHost crashed.
  virtual void DevToolsAgentHostCrashed(DevToolsAgentHost* agent_host,
                                        base::TerminationStatus status);

  // Called when DevToolsAgentHost was destroyed.
  virtual void DevToolsAgentHostDestroyed(DevToolsAgentHost* agent_host);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_OBSERVER_H_
