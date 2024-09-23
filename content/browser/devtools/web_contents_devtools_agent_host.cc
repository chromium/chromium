// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/web_contents_devtools_agent_host.h"

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/target_auto_attacher.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

namespace {
using WebContentsDevToolsMap =
    std::map<WebContents*, WebContentsDevToolsAgentHost*>;
base::LazyInstance<WebContentsDevToolsMap>::Leaky g_agent_host_instances =
    LAZY_INSTANCE_INITIALIZER;

WebContentsDevToolsAgentHost* FindAgentHost(WebContents* wc) {
  if (!g_agent_host_instances.IsCreated())
    return nullptr;
  auto it = g_agent_host_instances.Get().find(wc);
  return it == g_agent_host_instances.Get().end() ? nullptr : it->second;
}

}  // namespace

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForTab(WebContents* wc) {
  return WebContentsDevToolsAgentHost::GetFor(wc);
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetOrCreateForTab(
    WebContents* wc) {
  return WebContentsDevToolsAgentHost::GetOrCreateFor(wc);
}

class WebContentsDevToolsAgentHost::AutoAttacher
    : public protocol::TargetAutoAttacher {
 public:
  AutoAttacher() = default;

  void UpdateChildFrameTrees(bool update_target_info) {
    if (!auto_attach())
      return;
    base::flat_set<scoped_refptr<DevToolsAgentHost>> pages =
        UpdateAssociatedPages();
    if (update_target_info) {
      for (auto& page : pages)
        DispatchTargetInfoChanged(page.get());
    }
  }

  void WillInitiatePrerender(FrameTreeNode* ftn) {
    if (!auto_attach())
      return;
    auto host = RenderFrameDevToolsAgentHost::GetOrCreateFor(ftn);
    DispatchAutoAttach(host.get(), wait_for_debugger_on_start());
  }

  void SetWebContents(WebContents* wc) {
    web_contents_ = wc;
    UpdateAssociatedPages();
  }

 private:
  void UpdateAutoAttach(base::OnceClosure callback) override {
    UpdateAssociatedPages();
    protocol::TargetAutoAttacher::UpdateAutoAttach(std::move(callback));
  }

  base::flat_set<scoped_refptr<DevToolsAgentHost>> UpdateAssociatedPages() {
    base::flat_set<scoped_refptr<DevToolsAgentHost>> hosts;
    if (auto_attach() && web_contents_) {
      auto* rfh = static_cast<RenderFrameHostImpl*>(
          web_contents_->GetPrimaryMainFrame());
      web_contents_->ForEachRenderFrameHost(
          [&hosts](RenderFrameHost* rfh) { AddFrame(hosts, rfh); });
      // In case primary main frame has been filtered out but some criteria
      // in AddFrame(), ensure it's added.
      hosts.insert(
          RenderFrameDevToolsAgentHost::GetOrCreateFor(rfh->frame_tree_node()));
    }
    DispatchSetAttachedTargetsOfType(hosts, DevToolsAgentHost::kTypePage);
    return hosts;
  }

  static void AddFrame(base::flat_set<scoped_refptr<DevToolsAgentHost>>& hosts,
                       RenderFrameHost* rfh) {
    RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(rfh);
    // We do not expose cached hosts as separate targets for now.
    if (rfhi->IsInBackForwardCache())
      return;
    FrameTreeNode* ftn = rfhi->frame_tree_node();
    // We're interested only in main frames, with the expcetion of fenced frames
    // that are reported as regular subframes via FrameAutoAttacher.
    if (!ftn->IsMainFrame())
      return;
    if (ftn->IsFencedFrameRoot())
      return;
    // Ignore other kinds of embedders such as GuestViews.
    if (ftn->render_manager()->GetOuterDelegateNode()) {
      return;
    }

    hosts.insert(RenderFrameDevToolsAgentHost::GetOrCreateFor(ftn));
  }

  raw_ptr<WebContents> web_contents_ = nullptr;
};

// static
WebContentsDevToolsAgentHost* WebContentsDevToolsAgentHost::GetFor(
    WebContents* web_contents) {
  return FindAgentHost(web_contents);
}

// static
WebContentsDevToolsAgentHost* WebContentsDevToolsAgentHost::GetOrCreateFor(
    WebContents* web_contents) {
  if (auto* host = FindAgentHost(web_contents))
    return host;
  return new WebContentsDevToolsAgentHost(web_contents);
}

// static
bool WebContentsDevToolsAgentHost::IsDebuggerAttached(
    WebContents* web_contents) {
  WebContentsDevToolsAgentHost* host = FindAgentHost(web_contents);
  return host && host->IsAttached();
}

// static
void WebContentsDevToolsAgentHost::AddAllAgentHosts(
    DevToolsAgentHost::List* result) {
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    result->push_back(GetOrCreateFor(wc));
  }
}

WebContentsDevToolsAgentHost::WebContentsDevToolsAgentHost(WebContents* wc)
    : DevToolsAgentHostImpl(base::UnguessableToken::Create().ToString()),
      auto_attacher_(std::make_unique<AutoAttacher>()) {
  InnerAttach(wc);
  NotifyCreated();
}

void WebContentsDevToolsAgentHost::InnerAttach(WebContents* wc) {
  CHECK(!web_contents());
  // With ConnectWebContents(), we may be attaching to a WC that has
  // a different host created.
  // TODO(caseq): find a better solution. See also a similar comment in
  // RenderFrameDevToolsAgentHost::SetFrameTreeNode();
  auto prev_entry = g_agent_host_instances.Get().find(wc);
  if (prev_entry != g_agent_host_instances.Get().end()) {
    CHECK_NE(prev_entry->second, this);
    prev_entry->second->InnerDetach();
  }
  const bool inserted =
      g_agent_host_instances.Get().insert(std::make_pair(wc, this)).second;
  CHECK(inserted);
  auto_attacher_->SetWebContents(wc);
  Observe(wc);
  // Once created, persist till underlying WC is detached, so that
  // the target id is retained.
  AddRef();
}

void WebContentsDevToolsAgentHost::InnerDetach() {
  DCHECK_EQ(this, FindAgentHost(web_contents()));
  auto_attacher_->SetWebContents(nullptr);
  g_agent_host_instances.Get().erase(web_contents());
  Observe(nullptr);
  // We may or may not be destruced here, depending on embedders
  // potentially retaining references.
  Release();
}

void WebContentsDevToolsAgentHost::WillInitiatePrerender(FrameTreeNode* ftn) {
  auto_attacher_->WillInitiatePrerender(ftn);
  for (auto* tracing : protocol::TracingHandler::ForAgentHost(this)) {
    tracing->WillInitiatePrerender(ftn);
  }
}

void WebContentsDevToolsAgentHost::UpdateChildFrameTrees(
    bool update_target_info) {
  auto_attacher_->UpdateChildFrameTrees(update_target_info);
}

void WebContentsDevToolsAgentHost::InspectElement(RenderFrameHost* frame_host,
                                                  int x,
                                                  int y) {
  if (auto host = GetOrCreatePrimaryFrameAgent()) {
    host->InspectElement(frame_host, x, y);
  }
}

WebContentsDevToolsAgentHost::~WebContentsDevToolsAgentHost() {
  DCHECK(!web_contents());
}

void WebContentsDevToolsAgentHost::DisconnectWebContents() {
  InnerDetach();
}

void WebContentsDevToolsAgentHost::ConnectWebContents(
    WebContents* web_contents) {
  InnerAttach(web_contents);
}

BrowserContext* WebContentsDevToolsAgentHost::GetBrowserContext() {
  return web_contents()->GetBrowserContext();
}

WebContents* WebContentsDevToolsAgentHost::GetWebContents() {
  return web_contents();
}

std::string WebContentsDevToolsAgentHost::GetParentId() {
  return std::string();
}

std::string WebContentsDevToolsAgentHost::GetOpenerId() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetOpenerId();
  return "";
}

std::string WebContentsDevToolsAgentHost::GetOpenerFrameId() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetOpenerFrameId();
  return "";
}

bool WebContentsDevToolsAgentHost::CanAccessOpener() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->CanAccessOpener();
  return false;
}

std::string WebContentsDevToolsAgentHost::GetType() {
  return kTypeTab;
}

std::string WebContentsDevToolsAgentHost::GetTitle() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetTitle();
  return "";
}

std::string WebContentsDevToolsAgentHost::GetDescription() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetDescription();
  return "";
}

GURL WebContentsDevToolsAgentHost::GetURL() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetURL();
  return GURL();
}

GURL WebContentsDevToolsAgentHost::GetFaviconURL() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetFaviconURL();
  return GURL();
}

bool WebContentsDevToolsAgentHost::Activate() {
  if (auto host = GetOrCreatePrimaryFrameAgent()) {
    return host->Activate();
  }
  return false;
}

void WebContentsDevToolsAgentHost::Reload() {
  if (auto host = GetOrCreatePrimaryFrameAgent()) {
    host->Reload();
  }
}

bool WebContentsDevToolsAgentHost::Close() {
  if (auto host = GetOrCreatePrimaryFrameAgent()) {
    return host->Close();
  }
  return false;
}

base::TimeTicks WebContentsDevToolsAgentHost::GetLastActivityTime() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetLastActivityTime();
  return base::TimeTicks();
}

std::optional<network::CrossOriginEmbedderPolicy>
WebContentsDevToolsAgentHost::cross_origin_embedder_policy(
    const std::string& id) {
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

std::optional<network::CrossOriginOpenerPolicy>
WebContentsDevToolsAgentHost::cross_origin_opener_policy(
    const std::string& id) {
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

DevToolsAgentHostImpl* WebContentsDevToolsAgentHost::GetPrimaryFrameAgent() {
  if (WebContents* wc = web_contents()) {
    return RenderFrameDevToolsAgentHost::GetFor(
        static_cast<RenderFrameHostImpl*>(wc->GetPrimaryMainFrame()));
  }
  return nullptr;
}

scoped_refptr<DevToolsAgentHost>
WebContentsDevToolsAgentHost::GetOrCreatePrimaryFrameAgent() {
  if (WebContents* wc = web_contents()) {
    return RenderFrameDevToolsAgentHost::GetOrCreateFor(
        static_cast<WebContentsImpl*>(wc)->GetPrimaryFrameTree().root());
  }
  return nullptr;
}

void WebContentsDevToolsAgentHost::WebContentsDestroyed() {
  auto retain_this = ForceDetachAllSessionsImpl();
  InnerDetach();
}

void WebContentsDevToolsAgentHost::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  CHECK(web_contents());
  if (new_host == web_contents()->GetPrimaryMainFrame()) {
    std::ignore = RevalidateSessionAccess();
    // `this` is invalid at this point!
  }
}

void WebContentsDevToolsAgentHost::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(web_contents());
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  for (auto* tracing : protocol::TracingHandler::ForAgentHost(this)) {
    tracing->ReadyToCommitNavigation(request);
  }
}

void WebContentsDevToolsAgentHost::FrameDeleted(
    FrameTreeNodeId frame_tree_node_id) {
  for (auto* tracing : protocol::TracingHandler::ForAgentHost(this)) {
    tracing->FrameDeleted(frame_tree_node_id);
  }
}

// DevToolsAgentHostImpl overrides.
DevToolsSession::Mode WebContentsDevToolsAgentHost::GetSessionMode() {
  return DevToolsSession::Mode::kSupportsTabTarget;
}

bool WebContentsDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                                 bool acquire_wake_lock) {
  if (web_contents() && !RenderFrameDevToolsAgentHost::ShouldAllowSession(
                            web_contents()->GetPrimaryMainFrame(), session)) {
    return false;
  }
  session->SetBrowserOnly(true);
  const bool may_attach_to_brower = session->GetClient()->IsTrusted();
  session->CreateAndAddHandler<protocol::TargetHandler>(
      may_attach_to_brower
          ? protocol::TargetHandler::AccessMode::kRegular
          : protocol::TargetHandler::AccessMode::kAutoAttachOnly,
      GetId(), auto_attacher_.get(), session);
  DevToolsSession* root_session = session->GetRootSession();
  CHECK(root_session);
  session->CreateAndAddHandler<protocol::IOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::TracingHandler>(this, GetIOContext(),
                                                         root_session);
  return true;
}

protocol::TargetAutoAttacher* WebContentsDevToolsAgentHost::auto_attacher() {
  DCHECK(auto_attacher_);
  return auto_attacher_.get();
}

scoped_refptr<WebContentsDevToolsAgentHost>
WebContentsDevToolsAgentHost::RevalidateSessionAccess() {
  scoped_refptr<WebContentsDevToolsAgentHost> retain_this(this);
  WebContents* wc = web_contents();
  if (!wc) {
    return retain_this;
  }
  std::vector<DevToolsSession*> restricted_sessions;
  for (DevToolsSession* session : sessions()) {
    if (!RenderFrameDevToolsAgentHost::ShouldAllowSession(
            wc->GetPrimaryMainFrame(), session)) {
      restricted_sessions.push_back(session);
    }
  }
  if (!restricted_sessions.empty()) {
    ForceDetachRestrictedSessions(restricted_sessions);
  }
  return retain_this;
}

}  // namespace content
