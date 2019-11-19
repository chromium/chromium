// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/common/content_export.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class BrowserContext;

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  // DevToolsAgentHost implementation.
  bool AttachClient(DevToolsAgentHostClient* client) override;
  bool DetachClient(DevToolsAgentHostClient* client) override;
  bool DispatchProtocolMessage(DevToolsAgentHostClient* client,
                               const std::string& message) override;
  bool IsAttached() override;
  void InspectElement(RenderFrameHost* frame_host, int x, int y) override;
  std::string GetId() override;
  std::string CreateIOStreamFromData(
      scoped_refptr<base::RefCountedMemory> data) override;
  std::string GetParentId() override;
  std::string GetOpenerId() override;
  std::string GetDescription() override;
  GURL GetFaviconURL() override;
  std::string GetFrontendURL() override;
  base::TimeTicks GetLastActivityTime() override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* wc) override;

  bool Inspect();

  template <typename Handler>
  std::vector<Handler*> HandlersByName(const std::string& name) {
    std::vector<Handler*> result;
    if (sessions_.empty())
      return result;
    for (DevToolsSession* session : sessions_) {
      auto it = session->handlers().find(name);
      if (it != session->handlers().end())
        result.push_back(static_cast<Handler*>(it->second.get()));
    }
    return result;
  }

 protected:
  DevToolsAgentHostImpl(const std::string& id);
  ~DevToolsAgentHostImpl() override;

  static bool ShouldForceCreation();

  // Returning |false| will block the attach.
  virtual bool AttachSession(DevToolsSession* session);
  virtual void DetachSession(DevToolsSession* session);
  virtual void UpdateRendererChannel(bool force);

  void NotifyCreated();
  void NotifyNavigated();
  void NotifyCrashed(base::TerminationStatus status);
  void ForceDetachAllSessions();
  void ForceDetachRestrictedSessions(
      const std::vector<DevToolsSession*>& restricted_sessions);
  DevToolsIOContext* GetIOContext() { return &io_context_; }
  DevToolsRendererChannel* GetRendererChannel() { return &renderer_channel_; }

  const std::vector<DevToolsSession*>& sessions() const { return sessions_; }

 private:
  friend class DevToolsAgentHost;  // for static methods
  friend class DevToolsSession;
  friend class DevToolsRendererChannel;

  bool AttachInternal(std::unique_ptr<DevToolsSession> session);
  void DetachInternal(DevToolsSession* session);
  void NotifyAttached();
  void NotifyDetached();
  void NotifyDestroyed();
  DevToolsSession* SessionByClient(DevToolsAgentHostClient* client);

  const std::string id_;
  std::vector<DevToolsSession*> sessions_;
  base::flat_map<DevToolsAgentHostClient*, std::unique_ptr<DevToolsSession>>
      session_by_client_;
  DevToolsIOContext io_context_;
  DevToolsRendererChannel renderer_channel_;
  static int s_force_creation_count_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgentHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
