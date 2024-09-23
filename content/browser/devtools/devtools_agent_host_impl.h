// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/common/content_export.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/devtools_agent_host.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

class BrowserContext;

namespace protocol {
class TargetAutoAttacher;
}  // namespace protocol

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  // Returns DevToolsAgentHost with a given |id| or nullptr of it doesn't exist.
  static scoped_refptr<DevToolsAgentHostImpl> GetForId(const std::string& id);

  DevToolsAgentHostImpl(const DevToolsAgentHostImpl&) = delete;
  DevToolsAgentHostImpl& operator=(const DevToolsAgentHostImpl&) = delete;

  // DevToolsAgentHost implementation.
  bool AttachClient(DevToolsAgentHostClient* client) override;
  bool AttachClientWithoutWakeLock(DevToolsAgentHostClient* client) override;
  bool DetachClient(DevToolsAgentHostClient* client) override;
  void DispatchProtocolMessage(DevToolsAgentHostClient* client,
                               base::span<const uint8_t> message) override;
  bool IsAttached() override;
  void InspectElement(RenderFrameHost* frame_host, int x, int y) override;
  std::string GetId() override;
  std::string CreateIOStreamFromData(
      scoped_refptr<base::RefCountedMemory> data) override;
  std::string GetParentId() override;
  std::string GetOpenerId() override;
  std::string GetOpenerFrameId() override;
  bool CanAccessOpener() override;
  std::string GetDescription() override;
  GURL GetFaviconURL() override;
  std::string GetFrontendURL() override;
  base::TimeTicks GetLastActivityTime() override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* wc) override;
  RenderProcessHost* GetProcessHost() override;

  struct NetworkLoaderFactoryParamsAndInfo {
    NetworkLoaderFactoryParamsAndInfo();
    NetworkLoaderFactoryParamsAndInfo(
        url::Origin,
        net::SiteForCookies,
        network::mojom::URLLoaderFactoryParamsPtr);
    NetworkLoaderFactoryParamsAndInfo(NetworkLoaderFactoryParamsAndInfo&&);
    ~NetworkLoaderFactoryParamsAndInfo();
    url::Origin origin;
    net::SiteForCookies site_for_cookies;
    network::mojom::URLLoaderFactoryParamsPtr factory_params;
  };
  // Creates network factory parameters for devtools-initiated subresource
  // requests.
  virtual NetworkLoaderFactoryParamsAndInfo
  CreateNetworkFactoryParamsForDevTools();

  virtual DevToolsSession::Mode GetSessionMode();

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

  virtual std::optional<network::CrossOriginEmbedderPolicy>
  cross_origin_embedder_policy(const std::string& id);
  virtual std::optional<network::CrossOriginOpenerPolicy>
  cross_origin_opener_policy(const std::string& id);
  virtual std::optional<
      std::vector<network::mojom::ContentSecurityPolicyHeader>>
  content_security_policy(const std::string& id);

  virtual protocol::TargetAutoAttacher* auto_attacher();
  virtual std::string GetSubtype();

  base::ProcessId GetProcessId() const { return process_id_; }

 protected:
  explicit DevToolsAgentHostImpl(const std::string& id);
  ~DevToolsAgentHostImpl() override;

  static bool ShouldForceCreation();

  // Returning |false| will block the attach.
  virtual bool AttachSession(DevToolsSession* session, bool acquire_wake_lock);
  virtual void DetachSession(DevToolsSession* session);
  virtual void UpdateRendererChannel(bool force);

  void NotifyCreated();
  void NotifyNavigated();
  void NotifyCrashed(base::TerminationStatus status);

  void SetProcessId(base::ProcessId process_id);
  void ProcessHostChanged();

  void ForceDetachRestrictedSessions(
      const std::vector<DevToolsSession*>& restricted_sessions);
  DevToolsIOContext* GetIOContext() { return &io_context_; }
  DevToolsRendererChannel* GetRendererChannel() { return &renderer_channel_; }

  const std::vector<raw_ptr<DevToolsSession, VectorExperimental>>& sessions()
      const {
    return sessions_;
  }
  // Returns refptr retaining `this`. All other references may be removed
  // at this point, so `this` will become invalid as soon as returned refptr
  // gets destroyed.
  [[nodiscard]] scoped_refptr<DevToolsAgentHost> ForceDetachAllSessionsImpl();

  // Called when the corresponding renderer process notifies that the main
  // thread debugger is paused or resumed.
  // TODO(crbug.com/40269649): Remove this method when we collect enough
  // data to understand how likely that situation could happen.
  virtual void MainThreadDebuggerPaused();
  virtual void MainThreadDebuggerResumed();

 private:
  // Note that calling this may result in the instance being deleted,
  // as instance may be owned by client sessions. This should not be
  // used by methods of derived classes, use `ForceDetachAllSessionsImpl()`
  // above instead.
  void ForceDetachAllSessions() override;

  friend class DevToolsAgentHost;  // for static methods
  friend class DevToolsSession;
  friend class DevToolsRendererChannel;

  bool AttachInternal(std::unique_ptr<DevToolsSession> session);
  bool AttachInternal(std::unique_ptr<DevToolsSession> session,
                      bool acquire_wake_lock);
  void DetachInternal(DevToolsSession* session);
  void NotifyAttached();
  void NotifyDetached();
  void NotifyDestroyed();
  DevToolsSession* SessionByClient(DevToolsAgentHostClient* client);

  const std::string id_;
  std::vector<raw_ptr<DevToolsSession, VectorExperimental>> sessions_;
  base::flat_map<DevToolsAgentHostClient*, std::unique_ptr<DevToolsSession>>
      session_by_client_;
  DevToolsIOContext io_context_;
  DevToolsRendererChannel renderer_channel_;
  base::ProcessId process_id_ = base::kNullProcessId;

  static int s_force_creation_count_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
