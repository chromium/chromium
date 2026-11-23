// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/web_contents_devtools_agent_host.h"

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/target_auto_attacher.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {
using WebContentsDevToolsMap =
    std::map<WebContents*, WebContentsDevToolsAgentHost*>;
WebContentsDevToolsMap& GetAgentHostInstances() {
  static base::NoDestructor<WebContentsDevToolsMap> agent_host_instances;
  return *agent_host_instances;
}

WebContentsDevToolsAgentHost* FindAgentHost(WebContents* wc) {
  auto it = GetAgentHostInstances().find(wc);
  return it == GetAgentHostInstances().end() ? nullptr : it->second;
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
    if (web_contents_ && !web_contents_->IsBeingDestroyed()) {
      UpdateAssociatedPages();
    }
    protocol::TargetAutoAttacher::UpdateAutoAttach(std::move(callback));
  }

  base::flat_set<scoped_refptr<DevToolsAgentHost>> UpdateAssociatedPages() {
    base::flat_set<scoped_refptr<DevToolsAgentHost>> hosts;
    if (auto_attach() && web_contents_) {
      CHECK(!web_contents_->IsBeingDestroyed());
      auto* rfh = static_cast<RenderFrameHostImpl*>(
          web_contents_->GetPrimaryMainFrame());
      web_contents_->ForEachRenderFrameHost(
          [&hosts](RenderFrameHost* rfh) { AddFrame(hosts, rfh); });
      // In case the primary main frame has been filtered out by some criteria
      // in AddFrame(), ensure it's added.
      if (!base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
        hosts.insert(RenderFrameDevToolsAgentHost::GetOrCreateFor(
            rfh->frame_tree_node()));
      }
      // Include prerender targets from new-tab prerenders
      // (target_hint="_blank") that live in separate WebContents.
      auto* wci = static_cast<WebContentsImpl*>(web_contents_.get());
      if (auto* registry = wci->GetPrerenderHostRegistry()) {
        for (FrameTreeNode* ftn :
             registry->GetNewTabPrerenderFrameTreeNodes()) {
          hosts.insert(RenderFrameDevToolsAgentHost::GetOrCreateFor(ftn));
        }
      }
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
    // We're interested only in main frames, with the exception of fenced frames
    // and (MPArch-based) guests that are reported as regular subframes via
    // FrameAutoAttacher.
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
  auto* delegate = DevToolsManager::GetInstance()->delegate();
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    if (delegate && !delegate->ShouldReportAsTabTarget(wc).value_or(true)) {
      // Skip the target if delegate explicitly indicates that it should not be
      // reported as Tab.
      continue;
    }
    // Don't create agent hosts for targets that can't be inspected, since
    // clients can't attach to them.
    if (!RenderFrameDevToolsAgentHost::IsInspectable(
            wc->GetPrimaryMainFrame())) {
      continue;
    }
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
  auto prev_entry = GetAgentHostInstances().find(wc);
  if (prev_entry != GetAgentHostInstances().end()) {
    CHECK_NE(prev_entry->second, this);
    prev_entry->second->InnerDetach();
  }
  const bool inserted =
      GetAgentHostInstances().insert(std::make_pair(wc, this)).second;
  CHECK(inserted);
  auto_attacher_->SetWebContents(wc);
  Observe(wc);
  is_unloaded_ = ComputeIsUnloaded();
  // Once created, persist till underlying WC is detached, so that
  // the target id is retained.
  AddRef();
}

void WebContentsDevToolsAgentHost::InnerDetach() {
  CHECK_EQ(this, FindAgentHost(web_contents()), base::NotFatalUntil::M159);
  auto_attacher_->SetWebContents(nullptr);
  GetAgentHostInstances().erase(web_contents());
  Observe(nullptr);
  // We may or may not be destructed here, depending on embedders
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
  CHECK(!web_contents(), base::NotFatalUntil::M159);
}

void WebContentsDevToolsAgentHost::DisconnectWebContents() {
  InnerDetach();
}

void WebContentsDevToolsAgentHost::ConnectWebContents(
    WebContents* web_contents) {
  InnerAttach(web_contents);
}

BrowserContext* WebContentsDevToolsAgentHost::GetBrowserContext() {
  return web_contents() ? web_contents()->GetBrowserContext() : nullptr;
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
  // Without a primary frame host (for example, for an unloaded tab), use the
  // same sources as RenderFrameDevToolsAgentHost::GetTitle() for a primary
  // main frame.
  if (WebContents* wc = web_contents()) {
    if (DevToolsManagerDelegate* delegate =
            DevToolsManager::GetInstance()->delegate()) {
      std::string title = delegate->GetTargetTitle(wc);
      if (!title.empty()) {
        return title;
      }
    }
    return base::UTF16ToUTF8(wc->GetTitle());
  }
  return "";
}

std::string WebContentsDevToolsAgentHost::GetDescription() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetDescription();
  // Match RenderFrameDevToolsAgentHost::GetDescription().
  if (WebContents* wc = web_contents()) {
    if (DevToolsManagerDelegate* delegate =
            DevToolsManager::GetInstance()->delegate()) {
      return delegate->GetTargetDescription(wc);
    }
  }
  return "";
}

GURL WebContentsDevToolsAgentHost::GetURL() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetURL();
  // Match RenderFrameDevToolsAgentHost::GetURL() for a primary main frame, so
  // that the URL does not change when a primary frame host is created.
  if (WebContents* wc = web_contents()) {
    return wc->GetVisibleURL();
  }
  return GURL();
}

std::string WebContentsDevToolsAgentHost::GetSubtype() {
  // Compute the value, so that it is correct also if no notification has
  // updated `is_unloaded_` yet.
  return ComputeIsUnloaded() ? "unloaded" : "";
}

GURL WebContentsDevToolsAgentHost::GetFaviconURL() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetFaviconURL();
  // Match RenderFrameDevToolsAgentHost::GetFaviconURL() for a primary main
  // frame.
  if (WebContents* wc = web_contents()) {
    NavigationEntry* entry = wc->GetController().GetLastCommittedEntry();
    return entry ? entry->GetFavicon().url : GURL();
  }
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
  // Match RenderFrameDevToolsAgentHost::GetLastActivityTime().
  if (WebContents* wc = web_contents()) {
    return wc->GetLastActiveTimeTicks();
  }
  return base::TimeTicks();
}

std::optional<network::CrossOriginEmbedderPolicy>
WebContentsDevToolsAgentHost::cross_origin_embedder_policy(
    const std::string& id) {
  NOTREACHED();
}

std::optional<network::CrossOriginOpenerPolicy>
WebContentsDevToolsAgentHost::cross_origin_opener_policy(
    const std::string& id) {
  NOTREACHED();
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
  // Detach auto-attacher early so it doesn't do anything weird on a
  // half-destroyed WC while sessions are being detached below.
  auto_attacher_->SetWebContents(nullptr);
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

void WebContentsDevToolsAgentHost::WasDiscarded() {
  // WebContentsImpl::Discard() sends this notification also if
  // FrameTree::Discard() ignored the request, because a navigation is pending
  // commit. ComputeIsUnloaded() reports only a discard that occurred.
  UpdateIsUnloaded();
}

void WebContentsDevToolsAgentHost::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // After a renderer crash, the primary main frame has no document.
  UpdateIsUnloaded();
}

// DevToolsAgentHostImpl overrides.
DevToolsSession::Mode WebContentsDevToolsAgentHost::GetSessionMode() {
  return DevToolsSession::Mode::kSupportsTabTarget;
}

bool WebContentsDevToolsAgentHost::AttachSession(DevToolsSession* session) {
  auto* wc = web_contents();
  if (wc && !RenderFrameDevToolsAgentHost::ShouldAllowSession(
                wc->GetPrimaryMainFrame(), session)) {
    return false;
  }
  // Force WebContents to render if it does not have a live renderer.
  // This is a sign that the page is backgrounded and unloaded
  // from memory usually when re-opening a saved browser session.
  // We should not force a reload if there is already an uncommitted navigation,
  // as this can re-enter the navigation controller and crash.
  if (wc && !wc->GetPrimaryMainFrame()->IsRenderFrameLive() &&
      !wc->HasUncommittedNavigationInPrimaryMainFrame()) {
    wc->GetController().SetNeedsReload();
    wc->GetController().LoadIfNecessary();
  }

  session->SetBrowserOnly(true);
  const bool may_attach_to_browser = session->GetClient()->IsTrusted();
  session->CreateAndAddHandler<protocol::TargetHandler>(
      may_attach_to_browser
          ? protocol::TargetHandler::AccessMode::kRegular
          : protocol::TargetHandler::AccessMode::kAutoAttachOnly,
      GetId(), auto_attacher_.get(), session);
  DevToolsSession* root_session = session->GetRootSession();
  CHECK(root_session);
  session->CreateAndAddHandler<protocol::IOHandler>(GetIOContext());
  session->CreateAndAddHandler<protocol::TracingHandler>(
      this, GetIOContext(), root_session, session->GetClient()->IsTrusted());
  return true;
}

protocol::TargetAutoAttacher* WebContentsDevToolsAgentHost::auto_attacher() {
  CHECK(auto_attacher_, base::NotFatalUntil::M159);
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

void WebContentsDevToolsAgentHost::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    scoped_refptr<WebContentsDevToolsAgentHost> retain_this =
        RevalidateSessionAccess();
    // A new document replaces the unloaded page. A navigation that does not
    // commit (for example, a 204 response) does not get here.
    UpdateIsUnloaded();
  }
}

bool WebContentsDevToolsAgentHost::ComputeIsUnloaded() {
  WebContents* wc = web_contents();
  if (!wc) {
    return false;
  }
  auto* rfh = static_cast<RenderFrameHostImpl*>(wc->GetPrimaryMainFrame());
  // An in-place discard (WebContents::Discard()) marks the document as
  // discarded. If the renderer process is shared with other frames, the
  // process stays alive, and the frame keeps its committed state.
  if (rfh->document_associated_data().is_discarded()) {
    return true;
  }
  // After a replacement discard, after session restore without load, and when
  // the renderer process exits (because of a crash, or after an in-place
  // discard in a dedicated process), the primary main frame has not committed
  // a navigation. Exclude new tabs, which only have the initial entry.
  if (rfh->has_committed_any_navigation()) {
    return false;
  }
  NavigationEntry* entry = wc->GetController().GetLastCommittedEntry();
  return entry && !entry->IsInitialEntry();
}

void WebContentsDevToolsAgentHost::UpdateIsUnloaded() {
  const bool is_unloaded = ComputeIsUnloaded();
  if (is_unloaded == is_unloaded_) {
    return;
  }
  scoped_refptr<WebContentsDevToolsAgentHost> retain_this(this);
  is_unloaded_ = is_unloaded;
  if (web_contents()) {
    NotifyNavigated();
  }
}
}  // namespace content
