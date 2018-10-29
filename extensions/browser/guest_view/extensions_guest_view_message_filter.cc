// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"

#include "base/guid.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/stream_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::BrowserThread;
using content::NavigationHandle;
using content::NavigationThrottle;
using content::RenderFrameHost;
using content::SiteInstance;
using content::WebContents;
using guest_view::GuestViewManager;
using guest_view::GuestViewManagerDelegate;
using guest_view::GuestViewMessageFilter;

namespace extensions {

namespace {

// Cancels the given navigation handle unconditionally.
class CancelAndIgnoreNavigationForPluginFrameThrottle
    : public NavigationThrottle {
 public:
  explicit CancelAndIgnoreNavigationForPluginFrameThrottle(
      NavigationHandle* handle)
      : NavigationThrottle(handle) {}
  ~CancelAndIgnoreNavigationForPluginFrameThrottle() override {}

  const char* GetNameForLogging() override {
    return "CancelAndIgnoreNavigationForPluginFrameThrottle";
  }
  ThrottleCheckResult WillStartRequest() override { return CANCEL_AND_IGNORE; }
};

// TODO(ekaramad): Remove this once MimeHandlerViewGuest has fully migrated to
// using cross-process-frames.
// Returns true if |child_routing_id| corresponds to a frame which is a direct
// child of |parent_rfh|.
bool AreRoutingIDsConsistent(RenderFrameHost* parent_rfh,
                             int32_t child_routing_id) {
  const bool uses_cross_process_frame =
      content::MimeHandlerViewMode::UsesCrossProcessFrame();
  const bool is_child_routing_id_none = (child_routing_id == MSG_ROUTING_NONE);

  // For cross-process-frame MimeHandlerView, |child_routing_id| cannot be none.
  bool should_shutdown_process =
      (is_child_routing_id_none == uses_cross_process_frame);

  if (!should_shutdown_process && uses_cross_process_frame) {
    // The |child_routing_id| is the routing ID of either a RenderFrame or a
    // proxy in the |parent_rfh|. Therefore, to get the associated RFH we need
    // to go through the FTN first.
    int32_t child_ftn_id = RenderFrameHost::GetFrameTreeNodeIdForRoutingId(
        parent_rfh->GetProcess()->GetID(), child_routing_id);
    // The |child_rfh| is not really used; it is retrieved to verify whether or
    // not what the renderer process says makes any sense.
    auto* child_rfh = content::WebContents::FromRenderFrameHost(parent_rfh)
                          ->UnsafeFindFrameByFrameTreeNodeId(child_ftn_id);
    should_shutdown_process =
        child_rfh && (child_rfh->GetParent() != parent_rfh);
  }
  return !should_shutdown_process;
}

using ProcessIdToFilterMap =
    base::flat_map<int32_t, ExtensionsGuestViewMessageFilter*>;
ProcessIdToFilterMap* GetProcessIdToFilterMap() {
  static base::NoDestructor<ProcessIdToFilterMap> instance;
  return instance.get();
}

// Called on UI thread to remove the entry for a process.
void RemoveProcessIdFromGlobalMap(int32_t process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetProcessIdToFilterMap()->erase(process_id);
}

}  // namespace

const uint32_t ExtensionsGuestViewMessageFilter::kFilteredMessageClasses[] = {
    GuestViewMsgStart, ExtensionsGuestViewMsgStart};

// Helper class which navigates a given FrameTreeNode to "about:blank". This is
// used for scenarios where the plugin element's content frame has a different
// SiteInstance from its parent frame, or, the frame's origin is not
// "about:blank". Since this class triggers a navigation, all the document
// unload events will be dispatched and handled. During the lifetime of this
// helper class, all other navigations for the corresponding FrameTreeNode will
// be throttled and ignored.
class ExtensionsGuestViewMessageFilter::FrameNavigationHelper
    : public content::WebContentsObserver {
 public:
  FrameNavigationHelper(RenderFrameHost* plugin_rfh,
                        int32_t guest_instance_id,
                        int32_t element_instance_id,
                        base::DictionaryValue* attach_params,
                        bool is_full_page_plugin,
                        ExtensionsGuestViewMessageFilter* filter);
  ~FrameNavigationHelper() override;

  void FrameDeleted(RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(NavigationHandle* handle) override;
  void BeforeUnloadFired(bool proceed,
                         const base::TimeTicks& proceed_time) override;

  // During attaching, we should ignore any navigation which is not a navigation
  // to "about:blank" from the parent frame's SiteInstance.
  bool ShouldCancelAndIgnore(NavigationHandle* handle);

  int32_t guest_instance_id() const { return guest_instance_id_; }
  const base::DictionaryValue& attach_params() const { return attach_params_; }
  bool is_full_page_plugin() const { return is_full_page_plugin_; }
  SiteInstance* parent_site_instance() const {
    return parent_site_instance_.get();
  }

 private:
  void NavigateToAboutBlank();

  int32_t frame_tree_node_id_;
  const int32_t guest_instance_id_;
  const int32_t element_instance_id_;
  base::DictionaryValue attach_params_;
  const bool is_full_page_plugin_;
  ExtensionsGuestViewMessageFilter* const filter_;
  scoped_refptr<SiteInstance> parent_site_instance_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationHelper);
};

ExtensionsGuestViewMessageFilter::FrameNavigationHelper::FrameNavigationHelper(
    RenderFrameHost* plugin_rfh,
    int32_t guest_instance_id,
    int32_t element_instance_id,
    base::DictionaryValue* attach_params,
    bool is_full_page_plugin,
    ExtensionsGuestViewMessageFilter* filter)
    : content::WebContentsObserver(
          content::WebContents::FromRenderFrameHost(plugin_rfh)),
      frame_tree_node_id_(plugin_rfh->GetFrameTreeNodeId()),
      guest_instance_id_(guest_instance_id),
      element_instance_id_(element_instance_id),
      is_full_page_plugin_(is_full_page_plugin),
      filter_(filter),
      parent_site_instance_(plugin_rfh->GetParent()->GetSiteInstance()) {
  DCHECK(filter->GetOrCreateGuestViewManager()->GetGuestByInstanceIDSafely(
      guest_instance_id, plugin_rfh->GetParent()->GetProcess()->GetID()));
  attach_params_.Swap(attach_params);
  NavigateToAboutBlank();
}

ExtensionsGuestViewMessageFilter::FrameNavigationHelper::
    ~FrameNavigationHelper() {}

void ExtensionsGuestViewMessageFilter::FrameNavigationHelper::FrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetFrameTreeNodeId() != frame_tree_node_id_)
    return;
  // It is possible that the plugin frame is deleted before a NavigationHandle
  // is created; one such case is to immediately delete the plugin element right
  // after MimeHandlerViewFrameContainer requests to create the
  // MimeHandlerViewGuest on the browser side.
  filter_->ResumeAttachOrDestroy(element_instance_id_,
                                 nullptr /* plugin_rfh */);
}

void ExtensionsGuestViewMessageFilter::FrameNavigationHelper::
    DidFinishNavigation(NavigationHandle* handle) {
  if (handle->GetFrameTreeNodeId() != frame_tree_node_id_)
    return;

  if (!handle->GetURL().IsAboutBlank()) {
    // Another navigation has committed (it started before our navigation). The
    // intended navigation to 'about:blank' should arrive later.
    return;
  }

  filter_->ResumeAttachOrDestroy(element_instance_id_,
                                 handle->GetRenderFrameHost());
}

void ExtensionsGuestViewMessageFilter::FrameNavigationHelper::BeforeUnloadFired(
    bool proceed,
    const base::TimeTicks& proceed_time) {
  if (proceed)
    return;
  // Navigating to "about:blank" would involve unloading the current
  // document/frame if any, and naturally "beforeunload" is a possibility that
  // should be addressed. If the user chooses to stay on the old page after a
  // beforeunload dialog, do not create the plugin frame and clean up the
  // associated MimeHandlerView* classes.
  // TODO(ekaramad): This will lead to a change in the behavior of plugin
  // elements in Chrome for scenarios where the plugin element has a frame and
  // then transitions into a MimeHandlerView. In the BrowserPlugin-based version
  // of MimeHandlerView, loading a MimeHandlerView will not clear the existing
  // frames inside the HTMLPlugInElement. This leads to some bugs including not
  // firing either "unload" or "beforeunload" (naturally so, given that the
  // frame is not detached. See https://crbug.com/776510 for more context). We
  // might need to revisit the logic here if current HTMLPlugInElement bugs with
  // respect to PluginView and FrameView transitions are fixed. There won't be a
  // plugin frame as desired and the guest view will eventually die.
  filter_->ResumeAttachOrDestroy(element_instance_id_,
                                 nullptr /* plugin_rfh */);
}

bool ExtensionsGuestViewMessageFilter::FrameNavigationHelper::
    ShouldCancelAndIgnore(NavigationHandle* handle) {
  if (handle->GetFrameTreeNodeId() != frame_tree_node_id_)
    return false;

  if (handle->GetRenderFrameHost()->GetSiteInstance() ==
          parent_site_instance_ &&
      handle->GetURL().IsAboutBlank()) {
    // This is either the navigation which was triggered by this class, or a
    // freebie. As long as such a navigation successfully commits, we are on the
    // right track for attaching WebContentses.
    return false;
  }
  return true;
}

void ExtensionsGuestViewMessageFilter::FrameNavigationHelper::
    NavigateToAboutBlank() {
  // Immediately start a navigation to "about:blank".
  GURL about_blank(url::kAboutBlankURL);
  content::NavigationController::LoadURLParams params(about_blank);
  params.frame_tree_node_id = frame_tree_node_id_;
  // The goal is to have a plugin frame which is same-origin with parent, i.e.,
  // 'about:blank' and share the same SiteInstance.
  params.source_site_instance = parent_site_instance_;
  // The renderer (parent of the plugin frame) tries to load a MimeHandlerView
  // and therefore this navigation should be treated as renderer initiated.
  params.is_renderer_initiated = true;
  web_contents()->GetController().LoadURLWithParams(params);
}

// static
std::unique_ptr<NavigationThrottle>
ExtensionsGuestViewMessageFilter::MaybeCreateThrottle(
    NavigationHandle* handle) {
  DCHECK(content::MimeHandlerViewMode::UsesCrossProcessFrame());
  if (!handle->GetParentFrame()) {
    // A plugin element cannot be the FrameOwner to a main frame.
    return nullptr;
  }
  int32_t parent_process_id = handle->GetParentFrame()->GetProcess()->GetID();
  auto& map = *GetProcessIdToFilterMap();
  if (!base::ContainsKey(map, parent_process_id) || !map[parent_process_id]) {
    // This happens if the RenderProcessHost has not been initialized yet.
    return nullptr;
  }

  for (auto& pair : map[parent_process_id]->frame_navigation_helpers_) {
    if (!pair.second->ShouldCancelAndIgnore(handle))
      continue;
    // Any navigation of the corresponding FrameTreeNode which is not to
    // "about:blank" or is not initiated by parent SiteInstance should be
    // ignored.
    return std::make_unique<CancelAndIgnoreNavigationForPluginFrameThrottle>(
        handle);
  }
  return nullptr;
}

ExtensionsGuestViewMessageFilter::ExtensionsGuestViewMessageFilter(
    int render_process_id,
    BrowserContext* context)
    : GuestViewMessageFilter(kFilteredMessageClasses,
                             arraysize(kFilteredMessageClasses),
                             render_process_id,
                             context),
      content::BrowserAssociatedInterface<mojom::GuestView>(this, this) {
  GetProcessIdToFilterMap()->insert_or_assign(render_process_id_, this);
}

ExtensionsGuestViewMessageFilter::~ExtensionsGuestViewMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // This map is created and accessed on the UI thread. Remove the reference to
  // |this| here so that it will not be accessed again; but leave erasing the
  // key from the global map to UI thread to avoid races when accessing the
  // underlying data structure (https:/crbug.com/869791).
  (*GetProcessIdToFilterMap())[render_process_id_] = nullptr;
  base::PostTaskWithTraits(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(RemoveProcessIdFromGlobalMap, render_process_id_));
}

void ExtensionsGuestViewMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionsGuestViewHostMsg_ResizeGuest::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      GuestViewMessageFilter::OverrideThreadForMessage(message, thread);
  }
}

bool ExtensionsGuestViewMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionsGuestViewMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionsGuestViewHostMsg_CanExecuteContentScriptSync,
                        OnCanExecuteContentScript)
    IPC_MESSAGE_HANDLER(ExtensionsGuestViewHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_UNHANDLED(
        handled = GuestViewMessageFilter::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()
  return handled;
}

GuestViewManager* ExtensionsGuestViewMessageFilter::
    GetOrCreateGuestViewManager() {
  auto* manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager) {
    manager = GuestViewManager::CreateWithDelegate(
        browser_context_,
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
            browser_context_));
  }
  return manager;
}

void ExtensionsGuestViewMessageFilter::OnCanExecuteContentScript(
    int render_view_id,
    int script_id,
    bool* allowed) {
  WebViewRendererState::WebViewInfo info;
  WebViewRendererState::GetInstance()->GetInfo(render_process_id_,
                                               render_view_id, &info);

  *allowed =
      info.content_script_ids.find(script_id) != info.content_script_ids.end();
}

void ExtensionsGuestViewMessageFilter::CreateMimeHandlerViewGuest(
    int32_t render_frame_id,
    const std::string& view_id,
    int32_t element_instance_id,
    const gfx::Size& element_size,
    mime_handler::BeforeUnloadControlPtr before_unload_control,
    int32_t plugin_frame_routing_id) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ExtensionsGuestViewMessageFilter::
                         CreateMimeHandlerViewGuestOnUIThread,
                     this, render_frame_id, view_id, element_instance_id,
                     element_size, before_unload_control.PassInterface(),
                     plugin_frame_routing_id, false));
}

void ExtensionsGuestViewMessageFilter::CreateMimeHandlerViewGuestOnUIThread(
    int render_frame_id,
    const std::string& view_id,
    int element_instance_id,
    const gfx::Size& element_size,
    mime_handler::BeforeUnloadControlPtrInfo before_unload_control,
    int32_t plugin_frame_routing_id,
    bool is_full_page_plugin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* manager = GetOrCreateGuestViewManager();

  auto* rfh = RenderFrameHost::FromID(render_process_id_, render_frame_id);
  auto* embedder_web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!embedder_web_contents)
    return;

  if (!AreRoutingIDsConsistent(rfh, plugin_frame_routing_id)) {
    bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                    bad_message::MHVG_INVALID_PLUGIN_FRAME_ID);
    return;
  }

  GuestViewManager::WebContentsCreatedCallback callback = base::BindOnce(
      &ExtensionsGuestViewMessageFilter::MimeHandlerViewGuestCreatedCallback,
      this, element_instance_id, render_process_id_, render_frame_id,
      plugin_frame_routing_id, element_size, std::move(before_unload_control),
      is_full_page_plugin);

  base::DictionaryValue create_params;
  create_params.SetString(mime_handler_view::kViewId, view_id);
  create_params.SetInteger(guest_view::kElementWidth, element_size.width());
  create_params.SetInteger(guest_view::kElementHeight, element_size.height());
  manager->CreateGuest(MimeHandlerViewGuest::Type, embedder_web_contents,
                       create_params, std::move(callback));
}

void ExtensionsGuestViewMessageFilter::OnResizeGuest(
    int render_frame_id,
    int element_instance_id,
    const gfx::Size& new_size) {
  // We should have a GuestViewManager at this point. If we don't then the
  // embedder is misbehaving.
  auto* manager = GetGuestViewManagerOrKill();
  if (!manager)
    return;

  auto* guest_web_contents =
      manager->GetGuestByInstanceID(render_process_id_, element_instance_id);
  auto* mhvg = MimeHandlerViewGuest::FromWebContents(guest_web_contents);
  if (!mhvg)
    return;

  guest_view::SetSizeParams set_size_params;
  set_size_params.enable_auto_size.reset(new bool(false));
  set_size_params.normal_size.reset(new gfx::Size(new_size));
  mhvg->SetSize(set_size_params);
}

void ExtensionsGuestViewMessageFilter::CreateEmbeddedMimeHandlerViewGuest(
    int32_t render_frame_id,
    int32_t tab_id,
    const GURL& original_url,
    int32_t element_instance_id,
    const gfx::Size& element_size,
    content::mojom::TransferrableURLLoaderPtr transferrable_url_loader,
    int32_t plugin_frame_routing_id) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ExtensionsGuestViewMessageFilter::
                           CreateEmbeddedMimeHandlerViewGuest,
                       this, render_frame_id, tab_id, original_url,
                       element_instance_id, element_size,
                       base::Passed(&transferrable_url_loader),
                       plugin_frame_routing_id));
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          RenderFrameHost::FromID(render_process_id_, render_frame_id));
  if (!web_contents)
    return;

  auto* browser_context = web_contents->GetBrowserContext();
  std::string extension_id = transferrable_url_loader->url.host();
  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return;

  MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (!handler || !handler->HasPlugin()) {
    NOTREACHED();
    return;
  }

  GURL handler_url(Extension::GetBaseURLFromExtensionId(extension_id).spec() +
                   handler->handler_url());

  std::string view_id = base::GenerateGUID();
  std::unique_ptr<StreamContainer> stream_container(new StreamContainer(
      nullptr, tab_id, true /* embedded */, handler_url, extension_id,
      std::move(transferrable_url_loader), original_url));
  MimeHandlerStreamManager::Get(browser_context)
      ->AddStream(view_id, std::move(stream_container),
                  -1 /* frame_tree_node_id*/, render_process_id_,
                  render_frame_id);

  CreateMimeHandlerViewGuestOnUIThread(render_frame_id, view_id,
                                       element_instance_id, element_size,
                                       nullptr, plugin_frame_routing_id, false);
}

void ExtensionsGuestViewMessageFilter::MimeHandlerViewGuestCreatedCallback(
    int element_instance_id,
    int embedder_render_process_id,
    int embedder_render_frame_id,
    int32_t plugin_frame_routing_id,
    const gfx::Size& element_size,
    mime_handler::BeforeUnloadControlPtrInfo before_unload_control,
    bool is_full_page_plugin,
    WebContents* web_contents) {
  auto* guest_view = MimeHandlerViewGuest::FromWebContents(web_contents);
  if (!guest_view)
    return;

  guest_view->SetBeforeUnloadController(std::move(before_unload_control));
  int guest_instance_id = guest_view->guest_instance_id();
  auto* rfh = RenderFrameHost::FromID(embedder_render_process_id,
                                      embedder_render_frame_id);
  if (!rfh)
    return;

  guest_view->SetEmbedderFrame(embedder_render_process_id,
                               embedder_render_frame_id);

  base::DictionaryValue attach_params;
  attach_params.SetInteger(guest_view::kElementWidth, element_size.width());
  attach_params.SetInteger(guest_view::kElementHeight, element_size.height());
  auto uses_cross_process_frame =
      content::MimeHandlerViewMode::UsesCrossProcessFrame();
  if (uses_cross_process_frame) {
    auto* plugin_rfh = RenderFrameHost::FromID(embedder_render_process_id,
                                               plugin_frame_routing_id);
    if (!plugin_rfh) {
      // The plugin element has a proxy instead.
      plugin_rfh = RenderFrameHost::FromPlaceholderId(
          embedder_render_process_id, plugin_frame_routing_id);
    }
    if (!plugin_rfh) {
      // This should only happen if the original plugin frame was cross-process
      // and a concurrent navigation in its process won the race and ended up
      // destroying the proxy whose routing ID was sent here by the
      // MimeHandlerViewFrameContainer. We should ask the embedder to retry
      // creating the guest.
      guest_view->GetEmbedderFrame()->Send(
          new ExtensionsGuestViewMsg_RetryCreatingMimeHandlerViewGuest(
              element_instance_id));
      guest_view->Destroy(true);
      return;
    }

    if (plugin_rfh->GetSiteInstance() !=
            plugin_rfh->GetParent()->GetSiteInstance() ||
        !plugin_rfh->GetLastCommittedURL().IsAboutBlank()) {
      // The current API for attaching guests requires the frame in outer
      // WebContents to be same-origin with parent. Also, to respect before
      // unload handlers in the current plugin frame's document we should first
      // navigate the plugin frame to "about:blank".
      frame_navigation_helpers_[element_instance_id] =
          std::make_unique<FrameNavigationHelper>(
              plugin_rfh, guest_view->guest_instance_id(), element_instance_id,
              &attach_params, is_full_page_plugin, this);
      return;
    }

    AttachToEmbedderFrame(plugin_frame_routing_id, element_instance_id,
                          guest_instance_id, attach_params,
                          is_full_page_plugin);
    return;
  }

  auto* manager = GuestViewManager::FromBrowserContext(browser_context_);
  CHECK(manager);
  manager->AttachGuest(embedder_render_process_id, element_instance_id,
                       guest_instance_id, attach_params);
  rfh->Send(new ExtensionsGuestViewMsg_CreateMimeHandlerViewGuestACK(
      element_instance_id));
}

void ExtensionsGuestViewMessageFilter::ResumeAttachOrDestroy(
    int32_t element_instance_id,
    RenderFrameHost* plugin_rfh) {
  auto helper = std::move(frame_navigation_helpers_[element_instance_id]);
  frame_navigation_helpers_.erase(element_instance_id);
  if (plugin_rfh) {
    DCHECK(plugin_rfh->GetLastCommittedURL().IsAboutBlank());
    AttachToEmbedderFrame(plugin_rfh->GetRoutingID(), element_instance_id,
                          helper->guest_instance_id(), helper->attach_params(),
                          helper->is_full_page_plugin());
  } else if (auto* guest_view =
                 MimeHandlerViewGuest::From(
                     helper->parent_site_instance()->GetProcess()->GetID(),
                     helper->guest_instance_id())
                     ->As<MimeHandlerViewGuest>()) {
    guest_view->GetEmbedderFrame()->Send(
        new ExtensionsGuestViewMsg_DestroyFrameContainer(element_instance_id));
    guest_view->Destroy(true);
  }
  frame_navigation_helpers_.erase(element_instance_id);
}

}  // namespace extensions
