// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/forwarding_agent_host.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/browser/devtools/protocol/security_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"

namespace content {

namespace {
typedef std::map<std::string, DevToolsAgentHostImpl*> DevToolsMap;
base::LazyInstance<DevToolsMap>::Leaky g_devtools_instances =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ObserverList<DevToolsAgentHostObserver>::Unchecked>::
    Leaky g_devtools_observers = LAZY_INSTANCE_INITIALIZER;

// Returns a list of all active hosts on browser targets.
DevToolsAgentHost::List GetBrowserAgentHosts() {
  DevToolsAgentHost::List result;
  for (const auto& id_host : g_devtools_instances.Get()) {
    if (id_host.second->GetType() == DevToolsAgentHost::kTypeBrowser)
      result.push_back(id_host.second);
  }
  return result;
}

// Notify the provided agent host of a certificate error. Returns true if one of
// the host's handlers will handle the certificate error.
bool NotifyCertificateError(
    DevToolsAgentHost* host,
    int cert_error,
    const GURL& request_url,
    const DevToolsAgentHostImpl::CertErrorCallback& callback) {
  DevToolsAgentHostImpl* host_impl = static_cast<DevToolsAgentHostImpl*>(host);
  for (auto* security_handler :
       protocol::SecurityHandler::ForAgentHost(host_impl)) {
    if (security_handler->NotifyCertificateError(cert_error, request_url,
                                                 callback)) {
      return true;
    }
  }
  return false;
}
}  // namespace

const char DevToolsAgentHost::kTypePage[] = "page";
const char DevToolsAgentHost::kTypeFrame[] = "iframe";
const char DevToolsAgentHost::kTypeSharedWorker[] = "shared_worker";
const char DevToolsAgentHost::kTypeServiceWorker[] = "service_worker";
const char DevToolsAgentHost::kTypeBrowser[] = "browser";
const char DevToolsAgentHost::kTypeGuest[] = "webview";
const char DevToolsAgentHost::kTypeOther[] = "other";
int DevToolsAgentHostImpl::s_force_creation_count_ = 0;

// static
std::string DevToolsAgentHost::GetProtocolVersion() {
  // TODO(dgozman): generate this.
  return "1.3";
}

// static
bool DevToolsAgentHost::IsSupportedProtocolVersion(const std::string& version) {
  // TODO(dgozman): generate this.
  return version == "1.0" || version == "1.1" || version == "1.2" ||
         version == "1.3";
}

// static
DevToolsAgentHost::List DevToolsAgentHost::GetOrCreateAll() {
  List result;
  SharedWorkerDevToolsAgentHost::List shared_list;
  SharedWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&shared_list);
  for (const auto& host : shared_list)
    result.push_back(host);

  ServiceWorkerDevToolsAgentHost::List service_list;
  ServiceWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&service_list);
  for (const auto& host : service_list)
    result.push_back(host);

  RenderFrameDevToolsAgentHost::AddAllAgentHosts(&result);

#if DCHECK_IS_ON()
  for (auto it : result) {
    DevToolsAgentHostImpl* host = static_cast<DevToolsAgentHostImpl*>(it.get());
    DCHECK(g_devtools_instances.Get().find(host->id_) !=
           g_devtools_instances.Get().end());
  }
#endif

  return result;
}

DevToolsAgentHostImpl::DevToolsAgentHostImpl(const std::string& id)
    : id_(id), renderer_channel_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

DevToolsAgentHostImpl::~DevToolsAgentHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotifyDestroyed();
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForId(
    const std::string& id) {
  if (!g_devtools_instances.IsCreated())
    return nullptr;
  auto it = g_devtools_instances.Get().find(id);
  if (it == g_devtools_instances.Get().end())
    return nullptr;
  return it->second;
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::Forward(
    const std::string& id,
    std::unique_ptr<DevToolsExternalAgentProxyDelegate> delegate) {
  scoped_refptr<DevToolsAgentHost> result = DevToolsAgentHost::GetForId(id);
  if (result)
    return result;
  return new ForwardingAgentHost(id, std::move(delegate));
}

// static
bool DevToolsAgentHostImpl::HandleCertificateError(WebContents* web_contents,
                                                   int cert_error,
                                                   const GURL& request_url,
                                                   CertErrorCallback callback) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetOrCreateFor(web_contents).get();
  if (NotifyCertificateError(agent_host.get(), cert_error, request_url,
                             callback)) {
    // Only allow a single agent host to handle the error.
    callback.Reset();
  }

  for (scoped_refptr<DevToolsAgentHost> browser_agent_host :
       GetBrowserAgentHosts()) {
    if (NotifyCertificateError(browser_agent_host.get(), cert_error,
                               request_url, callback)) {
      // Only allow a single agent host to handle the error.
      callback.Reset();
    }
  }

  return !callback;
}

DevToolsSession* DevToolsAgentHostImpl::SessionByClient(
    DevToolsAgentHostClient* client) {
  auto it = session_by_client_.find(client);
  return it == session_by_client_.end() ? nullptr : it->second.get();
}

bool DevToolsAgentHostImpl::InnerAttachClient(DevToolsAgentHostClient* client,
                                              TargetRegistry* registry) {
  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  auto session = std::make_unique<DevToolsSession>(this, client);
  if (!AttachSession(session.get(), registry))
    return false;
  renderer_channel_.AttachSession(session.get());
  sessions_.push_back(session.get());
  session_by_client_[client] = std::move(session);
  if (sessions_.size() == 1)
    NotifyAttached();
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->ClientAttached(this, client);
  return true;
}

bool DevToolsAgentHostImpl::AttachClient(DevToolsAgentHostClient* client) {
  if (SessionByClient(client))
    return false;
  return InnerAttachClient(client, nullptr);
}

void DevToolsAgentHostImpl::AttachSubtargetClient(
    DevToolsAgentHostClient* client,
    TargetRegistry* registry) {
  if (SessionByClient(client))
    return;
  InnerAttachClient(client, registry);
}

bool DevToolsAgentHostImpl::DetachClient(DevToolsAgentHostClient* client) {
  if (!SessionByClient(client))
    return false;

  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  InnerDetachClient(client);
  return true;
}

bool DevToolsAgentHostImpl::DispatchProtocolMessage(
    DevToolsAgentHostClient* client,
    const std::string& message) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(message);
  return DispatchProtocolMessage(client, message,
                                 base::DictionaryValue::From(std::move(value)));
}

bool DevToolsAgentHostImpl::DispatchProtocolMessage(
    DevToolsAgentHostClient* client,
    const std::string& message,
    std::unique_ptr<base::DictionaryValue> parsed_message) {
  DevToolsSession* session = SessionByClient(client);
  if (!session)
    return false;
  session->DispatchProtocolMessage(message, std::move(parsed_message));
  return true;
}

void DevToolsAgentHostImpl::InnerDetachClient(DevToolsAgentHostClient* client) {
  std::unique_ptr<DevToolsSession> session =
      std::move(session_by_client_[client]);
  // Make sure we dispose session prior to reporting it to the host.
  session->Dispose();
  sessions_.erase(
      std::remove(sessions_.begin(), sessions_.end(), session.get()));
  session_by_client_.erase(client);
  DetachSession(session.get());
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->ClientDetached(this, client);
  if (sessions_.empty()) {
    io_context_.DiscardAllStreams();
    NotifyDetached();
  }
}

bool DevToolsAgentHostImpl::IsAttached() {
  return !sessions_.empty();
}

void DevToolsAgentHostImpl::InspectElement(RenderFrameHost* frame_host,
                                           int x,
                                           int y) {}

std::string DevToolsAgentHostImpl::GetId() {
  return id_;
}

std::string DevToolsAgentHostImpl::GetParentId() {
  return std::string();
}

std::string DevToolsAgentHostImpl::GetOpenerId() {
  return std::string();
}

std::string DevToolsAgentHostImpl::GetDescription() {
  return std::string();
}

GURL DevToolsAgentHostImpl::GetFaviconURL() {
  return GURL();
}

std::string DevToolsAgentHostImpl::GetFrontendURL() {
  return std::string();
}

base::TimeTicks DevToolsAgentHostImpl::GetLastActivityTime() {
  return base::TimeTicks();
}

BrowserContext* DevToolsAgentHostImpl::GetBrowserContext() {
  return nullptr;
}

WebContents* DevToolsAgentHostImpl::GetWebContents() {
  return nullptr;
}

void DevToolsAgentHostImpl::DisconnectWebContents() {
}

void DevToolsAgentHostImpl::ConnectWebContents(WebContents* wc) {
}

bool DevToolsAgentHostImpl::Inspect() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate()) {
    manager->delegate()->Inspect(this);
    return true;
  }
  return false;
}

void DevToolsAgentHostImpl::ForceDetachAllSessions() {
  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  while (!sessions_.empty()) {
    DevToolsAgentHostClient* client = (*sessions_.begin())->client();
    DetachClient(client);
    client->AgentHostClosed(this);
  }
}

void DevToolsAgentHostImpl::ForceDetachRestrictedSessions(
    const std::vector<DevToolsSession*>& restricted_sessions) {
  scoped_refptr<DevToolsAgentHostImpl> protect(this);

  for (DevToolsSession* session : restricted_sessions) {
    DevToolsAgentHostClient* client = session->client();
    DetachClient(client);
    client->AgentHostClosed(this);
  }
}

bool DevToolsAgentHostImpl::AttachSession(DevToolsSession* session,
                                          TargetRegistry* registry) {
  return false;
}

void DevToolsAgentHostImpl::DetachSession(DevToolsSession* session) {}

void DevToolsAgentHostImpl::UpdateRendererChannel(bool force) {}

// static
void DevToolsAgentHost::DetachAllClients() {
  if (!g_devtools_instances.IsCreated())
    return;

  // Make a copy, since detaching may lead to agent destruction, which
  // removes it from the instances.
  std::vector<scoped_refptr<DevToolsAgentHostImpl>> copy;
  for (auto it(g_devtools_instances.Get().begin());
       it != g_devtools_instances.Get().end(); ++it)
    copy.push_back(it->second);
  for (auto it(copy.begin()); it != copy.end(); ++it)
    it->get()->ForceDetachAllSessions();
}

// static
void DevToolsAgentHost::AddObserver(DevToolsAgentHostObserver* observer) {
  if (observer->ShouldForceDevToolsAgentHostCreation()) {
    if (!DevToolsAgentHostImpl::s_force_creation_count_) {
      // Force all agent hosts when first observer is added.
      DevToolsAgentHost::GetOrCreateAll();
    }
    DevToolsAgentHostImpl::s_force_creation_count_++;
  }

  g_devtools_observers.Get().AddObserver(observer);
  for (const auto& id_host : g_devtools_instances.Get())
    observer->DevToolsAgentHostCreated(id_host.second);
}

// static
void DevToolsAgentHost::RemoveObserver(DevToolsAgentHostObserver* observer) {
  if (observer->ShouldForceDevToolsAgentHostCreation())
    DevToolsAgentHostImpl::s_force_creation_count_--;
  g_devtools_observers.Get().RemoveObserver(observer);
}

// static
bool DevToolsAgentHostImpl::ShouldForceCreation() {
  return !!s_force_creation_count_;
}

void DevToolsAgentHostImpl::NotifyCreated() {
  DCHECK(g_devtools_instances.Get().find(id_) ==
         g_devtools_instances.Get().end());
  g_devtools_instances.Get()[id_] = this;
  for (auto& observer : g_devtools_observers.Get())
    observer.DevToolsAgentHostCreated(this);
}

void DevToolsAgentHostImpl::NotifyNavigated() {
  for (auto& observer : g_devtools_observers.Get())
    observer.DevToolsAgentHostNavigated(this);
}

void DevToolsAgentHostImpl::NotifyAttached() {
  for (auto& observer : g_devtools_observers.Get())
    observer.DevToolsAgentHostAttached(this);
}

void DevToolsAgentHostImpl::NotifyDetached() {
  for (auto& observer : g_devtools_observers.Get())
    observer.DevToolsAgentHostDetached(this);
}

void DevToolsAgentHostImpl::NotifyCrashed(base::TerminationStatus status) {
  for (auto& observer : g_devtools_observers.Get())
    observer.DevToolsAgentHostCrashed(this, status);
}

void DevToolsAgentHostImpl::NotifyDestroyed() {
  DCHECK(g_devtools_instances.Get().find(id_) !=
         g_devtools_instances.Get().end());
  for (auto& observer : g_devtools_observers.Get())
    observer.DevToolsAgentHostDestroyed(this);
  g_devtools_instances.Get().erase(id_);
}

}  // namespace content
