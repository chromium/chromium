// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/interstitial_page_impl.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/interstitial_page_navigator_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/display_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/buildflags.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/bindings_policy.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/page_transition_types.h"

using blink::WebDragOperation;
using blink::WebDragOperationsMask;

namespace content {

class InterstitialPageImpl::InterstitialPageRVHDelegateView
    : public RenderViewHostDelegateView {
 public:
  explicit InterstitialPageRVHDelegateView(InterstitialPageImpl* page);

  // RenderViewHostDelegateView implementation:
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void ShowPopupMenu(RenderFrameHost* render_frame_host,
                     const gfx::Rect& bounds,
                     int item_height,
                     double item_font_size,
                     int selected_item,
                     const std::vector<MenuItem>& items,
                     bool right_aligned,
                     bool allow_multiple_selection) override;
  void HidePopupMenu() override;
#endif
  void StartDragging(const DropData& drop_data,
                     WebDragOperationsMask operations_allowed,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragCursor(WebDragOperation operation) override;
  void GotFocus(RenderWidgetHostImpl* render_widget_host) override;
  void LostFocus(RenderWidgetHostImpl* render_widget_host) override;
  void TakeFocus(bool reverse) override;
  int GetTopControlsHeight() const override;
  int GetBottomControlsHeight() const override;
  bool DoBrowserControlsShrinkRendererSize() const override;
  virtual void OnFindReply(int request_id,
                           int number_of_matches,
                           const gfx::Rect& selection_rect,
                           int active_match_ordinal,
                           bool final_update);

 private:
  InterstitialPageImpl* interstitial_page_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageRVHDelegateView);
};

// We keep a map of the various blocking pages shown as the UI tests need to
// be able to retrieve them.
typedef std::map<WebContents*, InterstitialPageImpl*> InterstitialPageMap;
static InterstitialPageMap* g_web_contents_to_interstitial_page;

// Initializes g_web_contents_to_interstitial_page in a thread-safe manner.
// Should be called before accessing g_web_contents_to_interstitial_page.
static void InitInterstitialPageMap() {
  if (!g_web_contents_to_interstitial_page)
    g_web_contents_to_interstitial_page = new InterstitialPageMap;
}

InterstitialPage* InterstitialPage::Create(WebContents* web_contents,
                                           bool new_navigation,
                                           const GURL& url,
                                           InterstitialPageDelegate* delegate) {
  return new InterstitialPageImpl(
      web_contents,
      static_cast<RenderWidgetHostDelegate*>(
          static_cast<WebContentsImpl*>(web_contents)),
      new_navigation, url, delegate);
}

InterstitialPage* InterstitialPage::GetInterstitialPage(
    WebContents* web_contents) {
  InitInterstitialPageMap();
  InterstitialPageMap::const_iterator iter =
      g_web_contents_to_interstitial_page->find(web_contents);
  if (iter == g_web_contents_to_interstitial_page->end())
    return nullptr;

  return iter->second;
}

InterstitialPage* InterstitialPage::FromRenderFrameHost(RenderFrameHost* rfh) {
  if (!rfh)
    return nullptr;
  return static_cast<RenderFrameHostImpl*>(rfh)
      ->delegate()
      ->GetAsInterstitialPage();
}

InterstitialPageImpl::InterstitialPageImpl(
    WebContents* web_contents,
    RenderWidgetHostDelegate* render_widget_host_delegate,
    bool new_navigation,
    const GURL& url,
    InterstitialPageDelegate* delegate)
    : underlying_content_observer_(web_contents, this),
      web_contents_(web_contents),
      controller_(static_cast<NavigationControllerImpl*>(
          &web_contents->GetController())),
      render_widget_host_delegate_(render_widget_host_delegate),
      url_(url),
      new_navigation_(new_navigation),
      should_discard_pending_nav_entry_(new_navigation),
      enabled_(true),
      action_taken_(NO_ACTION),
      render_view_host_(nullptr),
      // TODO(nasko): The InterstitialPageImpl will need to provide its own
      // NavigationControllerImpl to the Navigator, which is separate from
      // the WebContents one, so we can enforce no navigation policy here.
      // While we get the code to a point to do this, pass NULL for it.
      // TODO(creis): We will also need to pass delegates for the RVHM as we
      // start to use it.
      frame_tree_(std::make_unique<FrameTree>(
          new InterstitialPageNavigatorImpl(this, controller_),
          this,
          this,
          this,
          static_cast<WebContentsImpl*>(web_contents))),
      original_child_id_(
          web_contents->GetRenderViewHost()->GetProcess()->GetID()),
      original_rvh_id_(web_contents->GetRenderViewHost()->GetRoutingID()),
      should_revert_web_contents_title_(false),
      resource_dispatcher_host_notified_(false),
      rvh_delegate_view_(new InterstitialPageRVHDelegateView(this)),
      create_view_(true),
      pause_throbber_(false),
      delegate_(delegate) {
  InitInterstitialPageMap();
}

InterstitialPageImpl::~InterstitialPageImpl() {
  // RenderViewHostImpl::RenderWidgetLostFocus() will be eventually executed in
  // the destructor of FrameTree. It uses InterstitialPageRVHDelegate, which
  // will be deleted because std::unique_ptr<InterstitialPageRVHDelegateView> is
  // placed after frame_tree_. See bug http://crbug.com/725594.
  frame_tree_.reset();
}

void InterstitialPageImpl::Show() {
  if (!enabled())
    return;

  // If an interstitial is already showing or about to be shown, close it before
  // showing the new one.
  // Be careful not to take an action on the old interstitial more than once.
  InterstitialPageMap::const_iterator iter =
      g_web_contents_to_interstitial_page->find(web_contents_);
  if (iter != g_web_contents_to_interstitial_page->end()) {
    InterstitialPageImpl* interstitial = iter->second;
    if (interstitial->action_taken_ != NO_ACTION) {
      interstitial->Hide();
    } else {
      // If we are currently showing an interstitial page for which we created
      // a transient entry and a new interstitial is shown as the result of a
      // new browser initiated navigation, then that transient entry has already
      // been discarded and a new pending navigation entry created.
      // So we should not discard that new pending navigation entry.
      // See http://crbug.com/9791
      if (new_navigation_ && interstitial->new_navigation_)
        interstitial->should_discard_pending_nav_entry_ = false;
      interstitial->DontProceed();
    }
  }

  // Block the resource requests for the render view host while it is hidden.
  TakeActionOnResourceDispatcher(BLOCK);
  // We need to be notified when the RenderViewHost is destroyed so we can
  // cancel the blocked requests.  We cannot do that on
  // NOTIFY_WEB_CONTENTS_DESTROYED as at that point the RenderViewHost has
  // already been destroyed.
  widget_observer_.Add(
      controller_->delegate()->GetRenderViewHost()->GetWidget());

  // Update the g_web_contents_to_interstitial_page map.
  iter = g_web_contents_to_interstitial_page->find(web_contents_);
  DCHECK(iter == g_web_contents_to_interstitial_page->end());
  (*g_web_contents_to_interstitial_page)[web_contents_] = this;

  if (new_navigation_) {
    std::unique_ptr<NavigationEntryImpl> entry =
        base::WrapUnique(new NavigationEntryImpl);
    entry->SetURL(url_);
    entry->SetVirtualURL(url_);
    entry->set_page_type(PAGE_TYPE_INTERSTITIAL);

    // Give delegates a chance to set some states on the navigation entry.
    delegate_->OverrideEntry(entry.get());

    controller_->SetTransientEntry(std::move(entry));

    static_cast<WebContentsImpl*>(web_contents_)
        ->DidChangeVisibleSecurityState();
  }

  DCHECK(!render_view_host_);
  render_view_host_ = CreateRenderViewHost();
  CreateWebContentsView();

  GURL data_url = GURL("data:text/html;charset=utf-8," +
                       net::EscapePath(delegate_->GetHTMLContents()));
  frame_tree_->root()->current_frame_host()->NavigateToInterstitialURL(
      data_url);
  frame_tree_->root()->current_frame_host()->UpdateAccessibilityMode();

  notification_registrar_.Add(this, NOTIFICATION_NAV_ENTRY_PENDING,
                              Source<NavigationController>(controller_));
}

void InterstitialPageImpl::Hide() {
  // We may have already been hidden, and are just waiting to be deleted.
  // We can't check for enabled() here, because some callers have already
  // called Disable.
  if (!render_view_host_)
    return;

  Disable();

  RenderWidgetHostView* old_view =
      controller_->delegate()->GetRenderViewHost()->GetWidget()->GetView();
  if (controller_->delegate()->GetInterstitialPage() == this && old_view &&
      !old_view->IsShowing() && !controller_->delegate()->IsHidden()) {
    // Show the original RVH since we're going away.  Note it might not exist if
    // the renderer crashed while the interstitial was showing.
    // Note that it is important that we don't call Show() if the view is
    // already showing. That would result in bad things (unparented HWND on
    // Windows for example) happening.
    old_view->Show();
  }

  // Delete this and call Shutdown on the RVH asynchronously, as we may have
  // been called from a RVH delegate method, and we can't delete the RVH out
  // from under itself.
  base::ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE, base::BindOnce(&InterstitialPageImpl::Shutdown,
                                weak_ptr_factory_.GetWeakPtr()));
  bool has_focus = render_view_host_->GetWidget()->GetView() &&
                   render_view_host_->GetWidget()->GetView()->HasFocus();
  render_view_host_ = nullptr;
  frame_tree_->root()->current_frame_host()->ResetChildren();
  controller_->delegate()->DetachInterstitialPage(has_focus);
  // Let's revert to the original title if necessary.
  NavigationEntry* entry = controller_->GetVisibleEntry();
  if (entry && !new_navigation_ && should_revert_web_contents_title_)
    web_contents_->UpdateTitleForEntry(entry, original_web_contents_title_);

  static_cast<WebContentsImpl*>(web_contents_)->DidChangeVisibleSecurityState();

  auto iter = g_web_contents_to_interstitial_page->find(web_contents_);
  DCHECK(iter != g_web_contents_to_interstitial_page->end());
  if (iter != g_web_contents_to_interstitial_page->end())
    g_web_contents_to_interstitial_page->erase(iter);

  // Clear the WebContents pointer, because it may now be deleted.
  // This signifies that we are in the process of shutting down.
  web_contents_ = nullptr;
}

void InterstitialPageImpl::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  widget_observer_.Remove(widget_host);
  if (action_taken_ == NO_ACTION) {
    // The RenderViewHost is being destroyed (as part of the tab being
    // closed); make sure we clear the blocked requests.
    RenderViewHost* rvh = RenderViewHost::From(widget_host);
    DCHECK(rvh->GetProcess()->GetID() == original_child_id_ &&
           rvh->GetRoutingID() == original_rvh_id_);
    TakeActionOnResourceDispatcher(CANCEL);
  }
}

void InterstitialPageImpl::Observe(int type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_NAV_ENTRY_PENDING:
      // We are navigating away from the interstitial (the user has typed a URL
      // in the location bar or clicked a bookmark).  Make sure clicking on the
      // interstitial will have no effect.  Also cancel any blocked requests
      // on the ResourceDispatcherHost.  Note that when we get this notification
      // the RenderViewHost has not yet navigated so we'll unblock the
      // RenderViewHost before the resource request for the new page we are
      // navigating arrives in the ResourceDispatcherHost.  This ensures that
      // request won't be blocked if the same RenderViewHost was used for the
      // new navigation.
      Disable();
      TakeActionOnResourceDispatcher(CANCEL);
      break;
    default:
      NOTREACHED();
  }
}

bool InterstitialPageImpl::OnMessageReceived(
    RenderFrameHostImpl* render_frame_host,
    const IPC::Message& message) {
  if (render_frame_host->GetRenderViewHost() != render_view_host_) {
    DCHECK(!render_view_host_)
        << "We expect an interstitial page to have only a single RVH";
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(InterstitialPageImpl, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DomOperationResponse,
                        OnDomOperationResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool InterstitialPageImpl::OnMessageReceived(
    RenderViewHostImpl* render_view_host,
    const IPC::Message& message) {
  return false;
}

void InterstitialPageImpl::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Note this is only for subframes in the interstitial, the notification for
  // the main frame happens in RenderViewCreated.
  controller_->delegate()->RenderFrameForInterstitialPageCreated(
      render_frame_host);
}

void InterstitialPageImpl::UpdateTitle(
    RenderFrameHost* render_frame_host,
    const base::string16& title,
    base::i18n::TextDirection title_direction) {
  if (!enabled())
    return;

  RenderViewHost* render_view_host = render_frame_host->GetRenderViewHost();
  DCHECK(render_view_host == render_view_host_);
  NavigationEntry* entry = controller_->GetVisibleEntry();
  if (!entry) {
    // There may be no visible entry if no URL has committed (e.g., after
    // window.open("")).  InterstitialPages with the new_navigation flag create
    // a transient NavigationEntry and thus have a visible entry.  However,
    // interstitials can still be created when there is no visible entry.  For
    // example, the opener window may inject content into the initial blank
    // page, which might trigger a SafeBrowsingBlockingPage.
    return;
  }

  // If this interstitial is shown on an existing navigation entry, we'll need
  // to remember its title so we can revert to it when hidden.
  if (!new_navigation_ && !should_revert_web_contents_title_) {
    original_web_contents_title_ = entry->GetTitle();
    should_revert_web_contents_title_ = true;
  }
  // TODO(evan): make use of title_direction.
  // http://code.google.com/p/chromium/issues/detail?id=27094
  web_contents_->UpdateTitleForEntry(entry, title);
}

InterstitialPage* InterstitialPageImpl::GetAsInterstitialPage() {
  return this;
}

ui::AXMode InterstitialPageImpl::GetAccessibilityMode() {
  if (web_contents_)
    return static_cast<WebContentsImpl*>(web_contents_)->GetAccessibilityMode();
  else
    return ui::AXMode();
}

void InterstitialPageImpl::Cut() {
  auto* input_handler = GetFocusedFrameInputHandler();
  if (!input_handler)
    return;
  input_handler->Cut();
  RecordAction(base::UserMetricsAction("Cut"));
}

void InterstitialPageImpl::ExecuteEditCommand(
    const std::string& command,
    const base::Optional<base::string16>& value) {
  auto* input_handler = GetFocusedFrameInputHandler();
  if (!input_handler)
    return;
  input_handler->ExecuteEditCommand(command, value);
}

void InterstitialPageImpl::Copy() {
  auto* input_handler = GetFocusedFrameInputHandler();
  if (!input_handler)
    return;
  input_handler->Copy();
  RecordAction(base::UserMetricsAction("Copy"));
}

void InterstitialPageImpl::Paste() {
  auto* input_handler = GetFocusedFrameInputHandler();
  if (!input_handler)
    return;
  input_handler->Paste();
  RecordAction(base::UserMetricsAction("Paste"));
}

void InterstitialPageImpl::SelectAll() {
  auto* input_handler = GetFocusedFrameInputHandler();
  if (!input_handler)
    return;
  input_handler->SelectAll();
  RecordAction(base::UserMetricsAction("SelectAll"));
}

RenderViewHostDelegateView* InterstitialPageImpl::GetDelegateView() {
  return rvh_delegate_view_.get();
}

WebContents* InterstitialPageImpl::GetWebContents() {
  return web_contents();
}

const GURL& InterstitialPageImpl::GetMainFrameLastCommittedURL() {
  return url_;
}

void InterstitialPageImpl::RenderViewTerminated(
    RenderViewHost* render_view_host,
    base::TerminationStatus status,
    int error_code) {
  // Our renderer died. This should not happen in normal cases.
  // If we haven't already started shutdown, just dismiss the interstitial.
  // We cannot check for enabled() here, because we may have called Disable
  // without calling Hide.
  if (render_view_host_)
    DontProceed();
}

void InterstitialPageImpl::DidNavigate(
    RenderViewHost* render_view_host,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  // A fast user could have navigated away from the page that triggered the
  // interstitial while the interstitial was loading, that would have disabled
  // us. In that case we can dismiss ourselves.
  if (!enabled()) {
    DontProceed();
    return;
  }
  if (ui::PageTransitionCoreTypeIs(params.transition,
                                   ui::PAGE_TRANSITION_AUTO_SUBFRAME)) {
    // No need to handle navigate message from iframe in the interstitial page.
    return;
  }

  // The interstitial is not loading anymore so stop the throbber.
  pause_throbber_ = true;

  // The RenderViewHost has loaded its contents, we can show it now.
  if (!controller_->delegate()->IsHidden())
    render_view_host_->GetWidget()->GetView()->Show();
  controller_->delegate()->AttachInterstitialPage(this);
  render_view_host_->GetWidget()->GetView()->OnInterstitialPageAttached();

  RenderWidgetHostView* rwh_view =
      controller_->delegate()->GetRenderViewHost()->GetWidget()->GetView();

  // The RenderViewHost may already have crashed before we even get here.
  if (rwh_view) {
    // If the page has focus, focus the interstitial.
    if (rwh_view->HasFocus())
      Focus();

    // Hide the original RVH since we're showing the interstitial instead.
    rwh_view->Hide();
  }
}

WebContents* InterstitialPageImpl::OpenURL(const OpenURLParams& params) {
  NOTREACHED();
  return nullptr;
}

const std::string& InterstitialPageImpl::GetUserAgentOverride() {
  return base::EmptyString();
}

bool InterstitialPageImpl::ShouldOverrideUserAgentInNewTabs() {
  return false;
}

blink::mojom::RendererPreferences InterstitialPageImpl::GetRendererPrefs(
    BrowserContext* browser_context) const {
  delegate_->OverrideRendererPrefs(&renderer_preferences_);
  return renderer_preferences_;
}

void InterstitialPageImpl::RenderWidgetDeleted(
    RenderWidgetHostImpl* render_widget_host) {
  // TODO(creis): Remove this method once we verify the shutdown path is sane.
  CHECK(!web_contents_);
}

KeyboardEventProcessingResult InterstitialPageImpl::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (!enabled())
    return KeyboardEventProcessingResult::NOT_HANDLED;
  return render_widget_host_delegate_->PreHandleKeyboardEvent(event);
}

bool InterstitialPageImpl::PreHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  if (!enabled())
    return false;

  if (event.GetType() == blink::WebInputEvent::Type::kMouseUp &&
      event.button == blink::WebPointerProperties::Button::kBack &&
      controller_->CanGoBack()) {
    controller_->GoBack();
    return true;
  }
  return false;
}

bool InterstitialPageImpl::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  return enabled() && render_widget_host_delegate_->HandleKeyboardEvent(event);
}

WebContents* InterstitialPageImpl::web_contents() const {
  return web_contents_;
}

RenderViewHostImpl* InterstitialPageImpl::CreateRenderViewHost() {
  if (!enabled())
    return nullptr;

  // Interstitial pages don't want to share the session storage so we mint a
  // new one.
  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  scoped_refptr<SiteInstance> site_instance =
      SiteInstance::Create(browser_context);
  DOMStorageContextWrapper* dom_storage_context =
      static_cast<DOMStorageContextWrapper*>(
          BrowserContext::GetStoragePartition(browser_context,
                                              site_instance.get())
              ->GetDOMStorageContext());
  session_storage_namespace_ =
      SessionStorageNamespaceImpl::Create(dom_storage_context);

  // Use the RenderViewHost from our FrameTree.
  frame_tree_->root()->render_manager()->Init(
      site_instance.get(), MSG_ROUTING_NONE, MSG_ROUTING_NONE, MSG_ROUTING_NONE,
      false);
  return frame_tree_->root()->current_frame_host()->render_view_host();
}

WebContentsView* InterstitialPageImpl::CreateWebContentsView() {
  if (!enabled() || !create_view_)
    return nullptr;
  WebContentsView* wcv =
      static_cast<WebContentsImpl*>(web_contents())->GetView();
  RenderWidgetHostViewBase* view =
      wcv->CreateViewForWidget(render_view_host_->GetWidget(), false);
  render_view_host_->GetWidget()->SetView(view);
  render_view_host_->GetMainFrame()->AllowBindings(
      BINDINGS_POLICY_DOM_AUTOMATION);

  render_view_host_->CreateRenderView(MSG_ROUTING_NONE, MSG_ROUTING_NONE,
                                      base::UnguessableToken::Create(),
                                      FrameReplicationState(), false);
  controller_->delegate()->RenderFrameForInterstitialPageCreated(
      frame_tree_->root()->current_frame_host());
  view->SetSize(web_contents()->GetContainerBounds().size());
  // Don't show the interstitial until we have navigated to it.
  view->Hide();
  return wcv;
}

void InterstitialPageImpl::Proceed() {
  // Don't repeat this if we are already shutting down.  We cannot check for
  // enabled() here, because we may have called Disable without calling Hide.
  if (!render_view_host_)
    return;

  if (action_taken_ != NO_ACTION) {
    NOTREACHED();
    return;
  }
  Disable();
  action_taken_ = PROCEED_ACTION;

  // Resumes the throbber, if applicable.
  pause_throbber_ = false;
  controller_->delegate()->DidProceedOnInterstitial();

  // If this is a new navigation, the old page is going away, so we cancel any
  // blocked requests for it.  If it is not a new navigation, then it means the
  // interstitial was shown as a result of a resource loading in the page.
  // Since the user wants to proceed, we'll let any blocked request go through.
  if (new_navigation_)
    TakeActionOnResourceDispatcher(CANCEL);
  else
    TakeActionOnResourceDispatcher(RESUME);

  // No need to hide if we are a new navigation, we'll get hidden when the
  // navigation is committed.
  if (!new_navigation_) {
    Hide();
    delegate_->OnProceed();
    return;
  }

  delegate_->OnProceed();
}

void InterstitialPageImpl::DontProceed() {
  // Don't repeat this if we are already shutting down.  We cannot check for
  // enabled() here, because we may have called Disable without calling Hide.
  if (!render_view_host_)
    return;
  DCHECK(action_taken_ != DONT_PROCEED_ACTION);

  Disable();
  action_taken_ = DONT_PROCEED_ACTION;

  // If this is a new navigation, we are returning to the original page, so we
  // resume blocked requests for it.  If it is not a new navigation, then it
  // means the interstitial was shown as a result of a resource loading in the
  // page and we won't return to the original page, so we cancel blocked
  // requests in that case.
  if (new_navigation_)
    TakeActionOnResourceDispatcher(RESUME);
  else
    TakeActionOnResourceDispatcher(CANCEL);

  if (should_discard_pending_nav_entry_) {
    // Since no navigation happens we have to discard the transient entry
    // explicitly.  Note that by calling DiscardNonCommittedEntries() we also
    // discard the pending entry, which is what we want, since the navigation is
    // cancelled.
    controller_->DiscardNonCommittedEntries();
  }

  Hide();
  delegate_->OnDontProceed();
}

void InterstitialPageImpl::CancelForNavigation() {
  // The user is trying to navigate away.  We should unblock the renderer and
  // disable the interstitial, but keep it visible until the navigation
  // completes.
  Disable();
  // If this interstitial was shown for a new navigation, allow any navigations
  // on the original page to resume (e.g., subresource requests, XHRs, etc).
  // Otherwise, cancel the pending, possibly dangerous navigations.
  if (new_navigation_)
    TakeActionOnResourceDispatcher(RESUME);
  else
    TakeActionOnResourceDispatcher(CANCEL);
}

void InterstitialPageImpl::SetSize(const gfx::Size& size) {
  if (!enabled())
    return;
#if !defined(OS_MACOSX)
  // When a tab is closed, we might be resized after our view was NULLed
  // (typically if there was an info-bar).
  if (render_view_host_->GetWidget()->GetView())
    render_view_host_->GetWidget()->GetView()->SetSize(size);
#else
  // TODO(port): Does Mac need to SetSize?
  NOTIMPLEMENTED();
#endif
}

void InterstitialPageImpl::Focus() {
  // Focus the native window.
  if (!enabled())
    return;
  render_view_host_->GetWidget()->GetView()->Focus();
}

void InterstitialPageImpl::FocusThroughTabTraversal(bool reverse) {
  if (!enabled())
    return;
  render_view_host_->SetInitialFocus(reverse);
}

RenderWidgetHostView* InterstitialPageImpl::GetView() {
  return render_view_host_->GetWidget()->GetView();
}

RenderFrameHostImpl* InterstitialPageImpl::GetMainFrame() {
  if (!render_view_host_)
    return nullptr;
  return static_cast<RenderFrameHostImpl*>(render_view_host_->GetMainFrame());
}

InterstitialPageDelegate* InterstitialPageImpl::GetDelegateForTesting() {
  return delegate_.get();
}

void InterstitialPageImpl::DontCreateViewForTesting() {
  create_view_ = false;
}

RenderFrameHostDelegate* InterstitialPageImpl::CreateNewWindow(
    RenderFrameHost* opener,
    const mojom::CreateNewWindowParams& params,
    bool is_new_browsing_instance,
    bool has_user_gesture,
    SessionStorageNamespace* session_storage_namespace) {
  NOTREACHED() << "InterstitialPage does not support showing popups.";
  return nullptr;
}

void InterstitialPageImpl::SetFocusedFrame(FrameTreeNode* node,
                                           SiteInstance* source) {
  frame_tree_->SetFocusedFrame(node, source);

  if (web_contents_) {
    static_cast<WebContentsImpl*>(web_contents_)
        ->SetAsFocusedWebContentsIfNecessary();
  }
}

Visibility InterstitialPageImpl::GetVisibility() {
  // Interstitials always occlude the underlying web content.
  return Visibility::OCCLUDED;
}

void InterstitialPageImpl::CreateNewWidget(
    int32_t render_process_id,
    int32_t route_id,
    mojo::PendingRemote<mojom::Widget> widget,
    RenderViewHostImpl* render_view_host) {
  NOTREACHED() << "InterstitialPage does not support showing drop-downs.";
}

void InterstitialPageImpl::CreateNewFullscreenWidget(
    int32_t render_process_id,
    int32_t route_id,
    mojo::PendingRemote<mojom::Widget> widget,
    RenderViewHostImpl* render_view_host) {
  NOTREACHED()
      << "InterstitialPage does not support showing full screen popups.";
}

void InterstitialPageImpl::ShowCreatedWindow(int process_id,
                                             int main_frame_widget_route_id,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_rect,
                                             bool user_gesture) {
  NOTREACHED() << "InterstitialPage does not support showing popups.";
}

void InterstitialPageImpl::ShowCreatedWidget(int process_id,
                                             int route_id,
                                             const gfx::Rect& initial_rect) {
  NOTREACHED() << "InterstitialPage does not support showing drop-downs.";
}

void InterstitialPageImpl::ShowCreatedFullscreenWidget(int process_id,
                                                       int route_id) {
  NOTREACHED()
      << "InterstitialPage does not support showing full screen popups.";
}

SessionStorageNamespace* InterstitialPageImpl::GetSessionStorageNamespace(
    SiteInstance* instance) {
  return session_storage_namespace_.get();
}

FrameTree* InterstitialPageImpl::GetFrameTree() {
  return frame_tree_.get();
}

void InterstitialPageImpl::Disable() {
  enabled_ = false;

  // Also let the InterstitialPageNavigatorImpl know.
  static_cast<InterstitialPageNavigatorImpl*>(frame_tree_->root()->navigator())
      ->Disable();
}

void InterstitialPageImpl::Shutdown() {
  delete this;
}

void InterstitialPageImpl::OnNavigatingAwayOrTabClosing() {
  // Notify the RenderWidgetHostView so it can clean up interstitial resources
  // before the WebContents is fully destroyed.
  if (render_view_host_ && render_view_host_->GetWidget() &&
      render_view_host_->GetWidget()->GetView()) {
    render_view_host_->GetWidget()->GetView()->OnInterstitialPageGoingAway();
  }
  if (action_taken_ == NO_ACTION) {
    // We are navigating away from the interstitial or closing a tab with an
    // interstitial.  Default to DontProceed(). We don't just call Hide as
    // subclasses will almost certainly override DontProceed to do some work
    // (ex: close pending connections).
    DontProceed();
  } else {
    // User decided to proceed and either the navigation was committed or
    // the tab was closed before that.
    Hide();
  }
}

void InterstitialPageImpl::TakeActionOnResourceDispatcher(
    ResourceRequestAction action) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (action == CANCEL || action == RESUME) {
    if (resource_dispatcher_host_notified_)
      return;
    resource_dispatcher_host_notified_ = true;
  }

  // The tab might not have a render_view_host if it was closed (in which case,
  // we have taken care of the blocked requests when processing
  // NOTIFY_RENDER_WIDGET_HOST_DESTROYED.
  RenderViewHostImpl* rvh =
      RenderViewHostImpl::FromID(original_child_id_, original_rvh_id_);
  if (!rvh)
    return;

  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(rvh->GetMainFrame());
  // Note, the RenderViewHost can lose its main frame if a new RenderFrameHost
  // commits with a new RenderViewHost. Additionally, RenderViewHosts for OOPIF
  // don't have main frames.
  if (!rfh)
    return;

  switch (action) {
    case BLOCK:
      rfh->BlockRequestsForFrame();
      break;
    case RESUME:
      rfh->ResumeBlockedRequestsForFrame();
      break;
    default:
      DCHECK_EQ(action, CANCEL);
      rfh->CancelBlockedRequestsForFrame();
      break;
  }
}

void InterstitialPageImpl::OnDomOperationResponse(
    RenderFrameHostImpl* source,
    const std::string& json_string) {
  std::string json = json_string;
  // Needed by test code.
  NotificationService::current()->Notify(NOTIFICATION_DOM_OPERATION_RESPONSE,
                                         Source<WebContents>(web_contents()),
                                         Details<std::string>(&json));

  if (!enabled())
    return;
  delegate_->CommandReceived(json_string);
}

InterstitialPageImpl::InterstitialPageRVHDelegateView::
    InterstitialPageRVHDelegateView(InterstitialPageImpl* page)
    : interstitial_page_(page) {}

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
void InterstitialPageImpl::InterstitialPageRVHDelegateView::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<MenuItem>& items,
    bool right_aligned,
    bool allow_multiple_selection) {
  NOTREACHED() << "InterstitialPage does not support showing popup menus.";
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::HidePopupMenu() {
  NOTREACHED() << "InterstitialPage does not support showing popup menus.";
}
#endif

void InterstitialPageImpl::InterstitialPageRVHDelegateView::StartDragging(
    const DropData& drop_data,
    WebDragOperationsMask allowed_operations,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info,
    RenderWidgetHostImpl* source_rwh) {
  interstitial_page_->render_view_host_->GetWidget()
      ->DragSourceSystemDragEnded();
  DVLOG(1) << "InterstitialPage does not support dragging yet.";
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::UpdateDragCursor(
    WebDragOperation) {
  NOTREACHED() << "InterstitialPage does not support dragging yet.";
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::GotFocus(
    RenderWidgetHostImpl* render_widget_host) {
  WebContents* web_contents = interstitial_page_->web_contents();
  if (web_contents) {
    static_cast<WebContentsImpl*>(web_contents)
        ->NotifyWebContentsFocused(render_widget_host);
  }
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::LostFocus(
    RenderWidgetHostImpl* render_widget_host) {
  WebContents* web_contents = interstitial_page_->web_contents();
  if (web_contents) {
    static_cast<WebContentsImpl*>(web_contents)
        ->NotifyWebContentsLostFocus(render_widget_host);
  }
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::TakeFocus(
    bool reverse) {
  if (!interstitial_page_->web_contents())
    return;
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(interstitial_page_->web_contents());
  if (!web_contents->GetDelegateView())
    return;

  web_contents->GetDelegateView()->TakeFocus(reverse);
}

int InterstitialPageImpl::InterstitialPageRVHDelegateView::
    GetTopControlsHeight() const {
  if (!interstitial_page_->web_contents())
    return 0;
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(interstitial_page_->web_contents());
  if (!web_contents || !web_contents->GetDelegateView())
    return 0;
  return web_contents->GetDelegateView()->GetTopControlsHeight();
}

int InterstitialPageImpl::InterstitialPageRVHDelegateView::
    GetBottomControlsHeight() const {
  if (!interstitial_page_->web_contents())
    return 0;
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(interstitial_page_->web_contents());
  if (!web_contents || !web_contents->GetDelegateView())
    return 0;
  return web_contents->GetDelegateView()->GetBottomControlsHeight();
}

bool InterstitialPageImpl::InterstitialPageRVHDelegateView::
    DoBrowserControlsShrinkRendererSize() const {
  if (!interstitial_page_->web_contents())
    return false;
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(interstitial_page_->web_contents());
  if (!web_contents || !web_contents->GetDelegateView())
    return false;
  return web_contents->GetDelegateView()->DoBrowserControlsShrinkRendererSize();
}

void InterstitialPageImpl::InterstitialPageRVHDelegateView::OnFindReply(
    int request_id,
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {}

InterstitialPageImpl::UnderlyingContentObserver::UnderlyingContentObserver(
    WebContents* web_contents,
    InterstitialPageImpl* interstitial)
    : WebContentsObserver(web_contents), interstitial_(interstitial) {}

void InterstitialPageImpl::UnderlyingContentObserver::NavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {
  interstitial_->OnNavigatingAwayOrTabClosing();
}

void InterstitialPageImpl::UnderlyingContentObserver::WebContentsDestroyed() {
  interstitial_->OnNavigatingAwayOrTabClosing();
}

TextInputManager* InterstitialPageImpl::GetTextInputManager() {
  return !web_contents_ ? nullptr
                        : static_cast<WebContentsImpl*>(web_contents_)
                              ->GetTextInputManager();
}

RenderWidgetHostInputEventRouter* InterstitialPageImpl::GetInputEventRouter() {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents_);
  if (!web_contents_impl)
    return nullptr;

  return web_contents_impl->GetInputEventRouter();
}

BrowserAccessibilityManager*
InterstitialPageImpl::GetRootBrowserAccessibilityManager() {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents_);
  if (!web_contents_impl)
    return nullptr;

  return web_contents_impl->GetRootBrowserAccessibilityManager();
}

BrowserAccessibilityManager*
InterstitialPageImpl::GetOrCreateRootBrowserAccessibilityManager() {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents_);
  if (!web_contents_impl)
    return nullptr;

  return web_contents_impl->GetOrCreateRootBrowserAccessibilityManager();
}

void InterstitialPageImpl::AudioContextPlaybackStarted(RenderFrameHost* host,
                                                       int context_id) {
  // Interstitial pages should not be playing any sound via WebAudio
  NOTREACHED();
}

void InterstitialPageImpl::AudioContextPlaybackStopped(RenderFrameHost* host,
                                                       int context_id) {
  // Interstitial pages should not be playing any sound via WebAudio.
  NOTREACHED();
}

mojom::FrameInputHandler* InterstitialPageImpl::GetFocusedFrameInputHandler() {
  FrameTreeNode* focused_node = frame_tree_->GetFocusedFrame();
  if (!focused_node)
    return nullptr;

  return focused_node->current_frame_host()->GetFrameInputHandler();
}

}  // namespace content
