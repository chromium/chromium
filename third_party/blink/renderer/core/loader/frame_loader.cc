/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Kris Jordan <krisjordan@gmail.com>
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/frame_loader.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_mixed_content.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/events/page_transition_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/navigation_initiator_impl.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/form_submission.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/core/page/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/mhtml/archive_resource.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

bool IsBackForwardLoadType(WebFrameLoadType type) {
  return type == WebFrameLoadType::kBackForward;
}

bool IsReloadLoadType(WebFrameLoadType type) {
  return type == WebFrameLoadType::kReload ||
         type == WebFrameLoadType::kReloadBypassingCache;
}

static bool NeedsHistoryItemRestore(WebFrameLoadType type) {
  return type == WebFrameLoadType::kBackForward || IsReloadLoadType(type);
}

ResourceRequest FrameLoader::ResourceRequestForReload(
    WebFrameLoadType frame_load_type,
    ClientRedirectPolicy client_redirect_policy) {
  DCHECK(IsReloadLoadType(frame_load_type));
  const auto cache_mode =
      frame_load_type == WebFrameLoadType::kReloadBypassingCache
          ? mojom::FetchCacheMode::kBypassCache
          : mojom::FetchCacheMode::kValidateCache;
  if (!document_loader_ || !document_loader_->GetHistoryItem())
    return ResourceRequest();

  ResourceRequest request =
      document_loader_->GetHistoryItem()->GenerateResourceRequest(cache_mode);
  request.SetRequestorOrigin(frame_->GetSecurityContext()->GetSecurityOrigin());

  // ClientRedirectPolicy is an indication that this load was triggered by some
  // direct interaction with the page. If this reload is not a client redirect,
  // we should reuse the referrer from the original load of the current
  // document. If this reload is a client redirect (e.g., location.reload()), it
  // was initiated by something in the current document and should therefore
  // show the current document's url as the referrer.
  // TODO(domfarolino): Stop storing ResourceRequest's generated referrer as a
  // header and instead use a separate member. See https://crbug.com/850813.
  if (client_redirect_policy == ClientRedirectPolicy::kClientRedirect) {
    request.SetHttpReferrer(SecurityPolicy::GenerateReferrer(
        frame_->GetDocument()->GetReferrerPolicy(),
        frame_->GetDocument()->GetSecurityOrigin(),
        frame_->GetDocument()->Url(),
        frame_->GetDocument()->OutgoingReferrer()));
  }

  request.SetSkipServiceWorker(frame_load_type ==
                               WebFrameLoadType::kReloadBypassingCache);
  return request;
}

FrameLoader::FrameLoader(LocalFrame* frame)
    : frame_(frame),
      progress_tracker_(MakeGarbageCollected<ProgressTracker>(frame)),
      in_restore_scroll_(false),
      forced_sandbox_flags_(WebSandboxFlags::kNone),
      dispatching_did_clear_window_object_in_main_world_(false),
      detached_(false),
      virtual_time_pauser_(
          frame_->GetFrameScheduler()->CreateWebScopedVirtualTimePauser(
              "FrameLoader",
              WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant)) {
  DCHECK(frame_);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID("loading", "FrameLoader", this);
  TakeObjectSnapshot();
}

FrameLoader::~FrameLoader() {
  DCHECK(detached_);
}

void FrameLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(progress_tracker_);
  visitor->Trace(document_loader_);
  visitor->Trace(provisional_document_loader_);
  visitor->Trace(last_origin_document_csp_);
}

void FrameLoader::Init() {
  ScriptForbiddenScope forbid_scripts;

  auto navigation_params = std::make_unique<WebNavigationParams>();
  navigation_params->url = KURL(g_empty_string);
  navigation_params->frame_policy =
      frame_->Owner() ? base::make_optional(frame_->Owner()->GetFramePolicy())
                      : base::nullopt;

  provisional_document_loader_ = Client()->CreateDocumentLoader(
      frame_, kWebNavigationTypeOther, std::move(navigation_params),
      nullptr /* extra_data */);
  provisional_document_loader_->StartLoading();

  CommitDocumentLoader(provisional_document_loader_.Release(), base::nullopt,
                       false /* dispatch_did_start */, base::DoNothing::Once(),
                       false /* dispatch_did_commit */);

  frame_->GetDocument()->CancelParsing();

  state_machine_.AdvanceTo(
      FrameLoaderStateMachine::kDisplayingInitialEmptyDocument);

  // Suppress finish notifications for initial empty documents, since they don't
  // generate start notifications.
  document_loader_->SetSentDidFinishLoad();
  if (frame_->GetPage()->Paused())
    frame_->SetLifecycleState(mojom::FrameLifecycleState::kPaused);

  TakeObjectSnapshot();
}

LocalFrameClient* FrameLoader::Client() const {
  return frame_->Client();
}

void FrameLoader::SetDefersLoading(bool defers) {
  if (frame_->GetDocument())
    frame_->GetDocument()->Fetcher()->SetDefersLoading(defers);
  if (document_loader_)
    document_loader_->SetDefersLoading(defers);
  if (provisional_document_loader_)
    provisional_document_loader_->SetDefersLoading(defers);
}

void FrameLoader::SaveScrollAnchor() {
  if (!document_loader_ || !document_loader_->GetHistoryItem() ||
      !frame_->View())
    return;

  // Shouldn't clobber anything if we might still restore later.
  if (NeedsHistoryItemRestore(document_loader_->LoadType()) &&
      !document_loader_->GetInitialScrollState().was_scrolled_by_user)
    return;

  HistoryItem* history_item = document_loader_->GetHistoryItem();
  if (ScrollableArea* layout_scrollable_area =
          frame_->View()->LayoutViewport()) {
    ScrollAnchor* scroll_anchor = layout_scrollable_area->GetScrollAnchor();
    DCHECK(scroll_anchor);

    const SerializedAnchor& serialized_anchor =
        scroll_anchor->GetSerializedAnchor();
    if (serialized_anchor.IsValid()) {
      history_item->SetScrollAnchorData(
          {serialized_anchor.selector,
           WebFloatPoint(serialized_anchor.relative_offset.X(),
                         serialized_anchor.relative_offset.Y()),
           serialized_anchor.simhash});
    }
  }
}

void FrameLoader::SaveScrollState() {
  if (!document_loader_ || !document_loader_->GetHistoryItem() ||
      !frame_->View())
    return;

  // Shouldn't clobber anything if we might still restore later.
  if (NeedsHistoryItemRestore(document_loader_->LoadType()) &&
      !document_loader_->GetInitialScrollState().was_scrolled_by_user)
    return;

  HistoryItem* history_item = document_loader_->GetHistoryItem();
  // For performance reasons, we don't save scroll anchors as often as we save
  // scroll offsets. In order to avoid keeping around a stale anchor, we clear
  // it when the saved scroll offset changes.
  history_item->SetScrollAnchorData(ScrollAnchorData());
  if (ScrollableArea* layout_scrollable_area = frame_->View()->LayoutViewport())
    history_item->SetScrollOffset(layout_scrollable_area->GetScrollOffset());
  history_item->SetVisualViewportScrollOffset(ToScrollOffset(
      frame_->GetPage()->GetVisualViewport().VisibleRect().Location()));

  if (frame_->IsMainFrame())
    history_item->SetPageScaleFactor(frame_->GetPage()->PageScaleFactor());

  Client()->DidUpdateCurrentHistoryItem();
}

void FrameLoader::DispatchUnloadEvent(
    SecurityOrigin* committing_origin,
    base::Optional<Document::UnloadEventTiming>* timing) {
  FrameNavigationDisabler navigation_disabler(*frame_);
  SaveScrollState();

  Document* document = frame_->GetDocument();
  if (document && !SVGImage::IsInSVGImage(document)) {
    document->DispatchUnloadEvents(committing_origin, timing);
    // Remove event listeners if we're firing unload events for a reason other
    // than committing a navigation. In the commit case, we'll determine whether
    // event listeners should be retained when choosing whether to reuse the
    // LocalDOMWindow.
    if (!timing)
      document->RemoveAllEventListenersRecursively();
  }
}

void FrameLoader::DidExplicitOpen() {
  probe::LifecycleEvent(frame_, GetDocumentLoader(), "init",
                        base::TimeTicks::Now().since_origin().InSecondsF());
  // Calling document.open counts as committing the first real document load.
  if (!state_machine_.CommittedFirstRealDocumentLoad())
    state_machine_.AdvanceTo(FrameLoaderStateMachine::kCommittedFirstRealLoad);

  // Only model a document.open() as part of a navigation if its parent is not
  // done or in the process of completing.
  if (Frame* parent = frame_->Tree().Parent()) {
    auto* parent_local_frame = DynamicTo<LocalFrame>(parent);
    if ((parent_local_frame &&
         parent_local_frame->GetDocument()->LoadEventStillNeeded()) ||
        (parent->IsRemoteFrame() && parent->IsLoading())) {
      progress_tracker_->ProgressStarted();
    }
  }
}

void FrameLoader::FinishedParsing() {
  if (state_machine_.CreatingInitialEmptyDocument())
    return;

  progress_tracker_->FinishedParsing();

  if (Client()) {
    ScriptForbiddenScope forbid_scripts;
    Client()->DispatchDidFinishDocumentLoad();
  }

  if (Client()) {
    Client()->RunScriptsAtDocumentReady(
        document_loader_ ? document_loader_->IsCommittedButEmpty() : true);
  }

  if (frame_->View()) {
    ProcessFragment(frame_->GetDocument()->Url(), document_loader_->LoadType(),
                    kNavigationToDifferentDocument);
  }

  frame_->GetDocument()->CheckCompleted();
}

// TODO(dgozman): we are calling this method too often, hoping that it
// does not do anything when navigation is in progress, or when loading
// has finished already. We should call it at the right times.
void FrameLoader::DidFinishNavigation(NavigationFinishState state) {
  // We should have either finished the provisional or committed navigation if
  // this is called. Only delcare the whole frame finished if neither is in
  // progress.
  DCHECK((document_loader_ && document_loader_->SentDidFinishLoad()) ||
         !HasProvisionalNavigation());
  if ((document_loader_ && !document_loader_->SentDidFinishLoad()) ||
      HasProvisionalNavigation()) {
    return;
  }

  // This code in this block is meant to prepare a document for display, but
  // this code may also run when swapping out a provisional frame. In that case,
  // skip the display work.
  if (frame_->IsLoading() && !frame_->IsProvisional()) {
    progress_tracker_->ProgressCompleted();
    // Retry restoring scroll offset since finishing loading disables content
    // size clamping.
    RestoreScrollPositionAndViewState();
    if (document_loader_)
      document_loader_->SetLoadType(WebFrameLoadType::kStandard);
    frame_->FinishedLoading(state);
  }

  // When a subframe finishes loading, the parent should check if *all*
  // subframes have finished loading (which may mean that the parent can declare
  // that the parent itself has finished loading).  This local-subframe-focused
  // code has a remote-subframe equivalent in
  // WebRemoteFrameImpl::DidStopLoading.
  Frame* parent = frame_->Tree().Parent();
  if (parent)
    parent->CheckCompleted();
}

Frame* FrameLoader::Opener() {
  return Client() ? Client()->Opener() : nullptr;
}

void FrameLoader::SetOpener(LocalFrame* opener) {
  // If the frame is already detached, the opener has already been cleared.
  if (Client())
    Client()->SetOpener(opener);
}

bool FrameLoader::AllowPlugins(ReasonForCallingAllowPlugins reason) {
  // With Oilpan, a FrameLoader might be accessed after the Page has been
  // detached. FrameClient will not be accessible, so bail early.
  if (!Client())
    return false;
  Settings* settings = frame_->GetSettings();
  bool allowed = settings && settings->GetPluginsEnabled();
  if (!allowed && reason == kAboutToInstantiatePlugin) {
    if (auto* settings_client = frame_->GetContentSettingsClient())
      settings_client->DidNotAllowPlugins();
  }
  return allowed;
}

void FrameLoader::DetachDocumentLoader(Member<DocumentLoader>& loader,
                                       bool flush_microtask_queue) {
  if (!loader)
    return;

  FrameNavigationDisabler navigation_disabler(*frame_);
  loader->DetachFromFrame(flush_microtask_queue);
  loader = nullptr;
}

void FrameLoader::DidFinishSameDocumentNavigation(
    const KURL& url,
    WebFrameLoadType frame_load_type,
    HistoryItem* history_item) {
  // If we have a state object, we cannot also be a new navigation.
  scoped_refptr<SerializedScriptValue> state_object =
      history_item ? history_item->StateObject() : nullptr;
  DCHECK(!state_object || frame_load_type == WebFrameLoadType::kBackForward);

  // onpopstate might change view state, so stash for later restore.
  base::Optional<HistoryItem::ViewState> view_state;
  if (history_item) {
    view_state = history_item->GetViewState();
  }

  frame_->DomWindow()->StatePopped(state_object
                                       ? std::move(state_object)
                                       : SerializedScriptValue::NullValue());

  if (view_state) {
    RestoreScrollPositionAndViewState(frame_load_type,
                                      true /* is_same_document */, *view_state,
                                      history_item->ScrollRestorationType());
  }

  // We need to scroll to the fragment whether or not a hash change occurred,
  // since the user might have scrolled since the previous navigation.
  ProcessFragment(url, frame_load_type, kNavigationWithinSameDocument);

  TakeObjectSnapshot();
}

WebFrameLoadType FrameLoader::DetermineFrameLoadType(
    const KURL& url,
    const AtomicString& http_method,
    Document* origin_document,
    const KURL& failing_url,
    WebFrameLoadType frame_load_type) {
  // TODO(dgozman): this method is rewriting the load type, which makes it hard
  // to reason about various navigations and their desired load type. We should
  // untangle it and detect the load type at the proper place. See, for example,
  // location.assign() block below.
  // Achieving that is complicated due to similar conditions in many places
  // both in the renderer and in the browser.
  if (frame_load_type == WebFrameLoadType::kStandard ||
      frame_load_type == WebFrameLoadType::kReplaceCurrentItem) {
    if (frame_->Tree().Parent() &&
        !state_machine_.CommittedFirstRealDocumentLoad())
      return WebFrameLoadType::kReplaceCurrentItem;
    if (!frame_->Tree().Parent() && !Client()->BackForwardLength()) {
      if (Opener() && url.IsEmpty())
        return WebFrameLoadType::kReplaceCurrentItem;
      return WebFrameLoadType::kStandard;
    }
  }
  if (frame_load_type != WebFrameLoadType::kStandard)
    return frame_load_type;
  // From the HTML5 spec for location.assign():
  // "If the browsing context's session history contains only one Document,
  // and that was the about:blank Document created when the browsing context
  // was created, then the navigation must be done with replacement enabled."
  if ((!state_machine_.CommittedMultipleRealLoads() &&
       DeprecatedEqualIgnoringCase(frame_->GetDocument()->Url(), BlankURL())))
    return WebFrameLoadType::kReplaceCurrentItem;

  if (url == document_loader_->UrlForHistory()) {
    if (http_method == http_names::kPOST)
      return WebFrameLoadType::kStandard;
    if (!origin_document)
      return WebFrameLoadType::kReload;
    return WebFrameLoadType::kReplaceCurrentItem;
  }

  if (failing_url == document_loader_->UrlForHistory() &&
      document_loader_->LoadType() == WebFrameLoadType::kReload)
    return WebFrameLoadType::kReload;

  if (url.IsEmpty() && failing_url.IsEmpty()) {
    return WebFrameLoadType::kReplaceCurrentItem;
  }

  return WebFrameLoadType::kStandard;
}

bool FrameLoader::AllowRequestForThisFrame(const FrameLoadRequest& request) {
  // If no origin Document* was specified, skip remaining security checks and
  // assume the caller has fully initialized the FrameLoadRequest.
  if (!request.OriginDocument())
    return true;

  const KURL& url = request.GetResourceRequest().Url();
  if (url.ProtocolIsJavaScript()) {
    Document* origin_document = request.OriginDocument();
    // Check the CSP of the caller (the "source browsing context") if required,
    // as per https://html.spec.whatwg.org/C/#javascript-protocol.
    bool javascript_url_is_allowed =
        request.ShouldCheckMainWorldContentSecurityPolicy() ==
            kDoNotCheckContentSecurityPolicy ||
        origin_document->GetContentSecurityPolicy()->AllowInline(
            ContentSecurityPolicy::InlineType::kNavigation,
            frame_->DeprecatedLocalOwner(), url.GetString(),
            String() /* nonce */, origin_document->Url(),
            OrdinalNumber::First());

    if (!javascript_url_is_allowed)
      return false;

    if (frame_->Owner() &&
        ((frame_->Owner()->GetFramePolicy().sandbox_flags &
          WebSandboxFlags::kOrigin) != WebSandboxFlags::kNone))
      return false;
  }

  if (!request.CanDisplay(url)) {
    request.OriginDocument()->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Not allowed to load local resource: " + url.ElidedString()));
    return false;
  }
  return true;
}

static WebNavigationType DetermineNavigationType(
    WebFrameLoadType frame_load_type,
    bool is_form_submission,
    bool have_event) {
  bool is_reload = IsReloadLoadType(frame_load_type);
  bool is_back_forward = IsBackForwardLoadType(frame_load_type);
  if (is_form_submission) {
    return (is_reload || is_back_forward) ? kWebNavigationTypeFormResubmitted
                                          : kWebNavigationTypeFormSubmitted;
  }
  if (have_event)
    return kWebNavigationTypeLinkClicked;
  if (is_reload)
    return kWebNavigationTypeReload;
  if (is_back_forward)
    return kWebNavigationTypeBackForward;
  return kWebNavigationTypeOther;
}

static mojom::RequestContextType DetermineRequestContextFromNavigationType(
    const WebNavigationType navigation_type) {
  switch (navigation_type) {
    case kWebNavigationTypeLinkClicked:
      return mojom::RequestContextType::HYPERLINK;

    case kWebNavigationTypeOther:
      return mojom::RequestContextType::LOCATION;

    case kWebNavigationTypeFormResubmitted:
    case kWebNavigationTypeFormSubmitted:
      return mojom::RequestContextType::FORM;

    case kWebNavigationTypeBackForward:
    case kWebNavigationTypeReload:
      return mojom::RequestContextType::INTERNAL;
  }
  NOTREACHED();
  return mojom::RequestContextType::HYPERLINK;
}

void FrameLoader::StartNavigation(const FrameLoadRequest& passed_request,
                                  WebFrameLoadType frame_load_type) {
  CHECK(!IsBackForwardLoadType(frame_load_type));
  DCHECK(passed_request.GetTriggeringEventInfo() !=
         TriggeringEventInfo::kUnknown);
  DCHECK(frame_->GetDocument());
  if (HTMLFrameOwnerElement* element = frame_->DeprecatedLocalOwner())
    element->CancelPendingLazyLoad();

  FrameLoadRequest request(passed_request);
  ResourceRequest& resource_request = request.GetResourceRequest();
  const KURL& url = resource_request.Url();
  Document* origin_document = request.OriginDocument();

  resource_request.SetHasUserGesture(
      LocalFrame::HasTransientUserActivation(frame_));

  if (!AllowRequestForThisFrame(request))
    return;

  // Block renderer-initiated loads of data: and filesystem: URLs in the top
  // frame.
  //
  // If the mime type of the data URL is supported, the URL will
  // eventually be rendered, so block it here. Otherwise, the load might be
  // handled by a plugin or end up as a download, so allow it to let the
  // embedder figure out what to do with it. Navigations to filesystem URLs are
  // always blocked here.
  if (frame_->IsMainFrame() && origin_document &&
      !frame_->Client()->AllowContentInitiatedDataUrlNavigations(
          request.OriginDocument()->Url()) &&
      (url.ProtocolIs("filesystem") ||
       (url.ProtocolIsData() &&
        network_utils::IsDataURLMimeTypeSupported(url)))) {
    frame_->GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Not allowed to navigate top frame to " + url.Protocol() +
            " URL: " + url.ElidedString()));
    return;
  }

  // TODO(dgozman): merge page dismissal check and FrameNavigationDisabler.
  if (!frame_->IsNavigationAllowed() ||
      frame_->GetDocument()->PageDismissalEventBeingDispatched() !=
          Document::kNoDismissal) {
    return;
  }

  frame_load_type = DetermineFrameLoadType(
      resource_request.Url(), resource_request.HttpMethod(), origin_document,
      KURL(), frame_load_type);

  bool same_document_navigation =
      request.GetNavigationPolicy() == kNavigationPolicyCurrentTab &&
      ShouldPerformFragmentNavigation(
          request.Form(), resource_request.HttpMethod(), frame_load_type, url);

  // Perform same document navigation.
  if (same_document_navigation) {
    document_loader_->CommitSameDocumentNavigation(
        url, frame_load_type, nullptr, request.ClientRedirect(),
        origin_document,
        request.GetTriggeringEventInfo() != TriggeringEventInfo::kNotFromEvent,
        nullptr /* extra_data */);
    return;
  }

  WebNavigationType navigation_type = DetermineNavigationType(
      frame_load_type, resource_request.HttpBody() || request.Form(),
      request.GetTriggeringEventInfo() != TriggeringEventInfo::kNotFromEvent);
  mojom::RequestContextType request_context_type =
      DetermineRequestContextFromNavigationType(navigation_type);

  // TODO(lyf): handle `frame` context type. https://crbug.com/1019716
  if (mojom::RequestContextType::LOCATION == request_context_type &&
      !frame_->IsMainFrame()) {
    request_context_type = mojom::RequestContextType::IFRAME;
  }
  resource_request.SetRequestContext(request_context_type);
  request.SetFrameType(frame_->IsMainFrame()
                           ? network::mojom::RequestContextFrameType::kTopLevel
                           : network::mojom::RequestContextFrameType::kNested);

  mojo::PendingRemote<mojom::blink::NavigationInitiator> navigation_initiator;
  WebContentSecurityPolicyList initiator_csp;
  if (origin_document && origin_document->GetContentSecurityPolicy()
                             ->ExperimentalFeaturesEnabled()) {
    initiator_csp = origin_document->GetContentSecurityPolicy()
                        ->ExposeForNavigationalChecks();
    origin_document->NavigationInitiator().BindReceiver(
        navigation_initiator.InitWithNewPipeAndPassReceiver());
  }

  if (origin_document && origin_document->GetContentSecurityPolicy()) {
    last_origin_document_csp_ = MakeGarbageCollected<ContentSecurityPolicy>();
    last_origin_document_csp_->CopyStateFrom(
        origin_document->GetContentSecurityPolicy());
    last_origin_document_csp_->CopyPluginTypesFrom(
        origin_document->GetContentSecurityPolicy());
  }

  // Record the latest requiredCSP value that will be used when sending this
  // request.
  RecordLatestRequiredCSP();

  // TODO(arthursonzogni): 'frame-src' check is disabled on the
  // renderer side, but is enforced on the browser side.
  // See http://crbug.com/692595 for understanding why it
  // can't be enforced on both sides instead.

  // 'form-action' check in the frame that is navigating is disabled on the
  // renderer side, but is enforced on the browser side instead.
  // N.B. check in the frame that initiates the navigation stills occurs in
  // blink and is not enforced on the browser-side.
  // TODO(arthursonzogni) The 'form-action' check should be fully disabled
  // in blink, except when the form submission doesn't trigger a navigation
  // (i.e. javascript urls). Please see https://crbug.com/701749.

  // Report-only CSP headers are checked in browser.
  const FetchClientSettingsObject* fetch_client_settings_object = nullptr;
  if (origin_document) {
    fetch_client_settings_object = &origin_document->Fetcher()
                                        ->GetProperties()
                                        .GetFetchClientSettingsObject();
  }
  ModifyRequestForCSP(resource_request, fetch_client_settings_object,
                      origin_document, request.GetFrameType());

  DCHECK(Client()->HasWebView());
  // Check for non-escaped new lines in the url.
  if (url.PotentiallyDanglingMarkup() && url.ProtocolIsInHTTPFamily()) {
    Deprecation::CountDeprecation(
        origin_document, WebFeature::kCanRequestURLHTTPContainingNewline);
    return;
  }

  if (url.ProtocolIsJavaScript()) {
    if (!origin_document ||
        origin_document->CanExecuteScripts(kAboutToExecuteScript)) {
      frame_->GetDocument()->ProcessJavaScriptUrl(
          url, request.ShouldCheckMainWorldContentSecurityPolicy());
    }
    return;
  }

  bool has_transient_activation =
      LocalFrame::HasTransientUserActivation(frame_);
  // TODO(csharrison): In M71 when UserActivation v2 should ship, we can remove
  // the check that the pages are equal, because consumption should not be
  // shared across pages. After that, we can also get rid of consumption call
  // in RenderFrameImpl::OpenURL.
  if (frame_->IsMainFrame() && origin_document &&
      frame_->GetPage() == origin_document->GetPage()) {
    LocalFrame::ConsumeTransientUserActivation(frame_);
  }

  // The main resource request gets logged here, because V8DOMActivityLogger
  // is looked up based on the current v8::Context. When the request actually
  // begins, the v8::Context may no longer be on the stack.
  if (V8DOMActivityLogger* activity_logger =
          V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld()) {
    if (!DocumentLoader::WillLoadUrlAsEmpty(url)) {
      Vector<String> argv;
      argv.push_back("Main resource");
      argv.push_back(url.GetString());
      activity_logger->LogEvent("blinkRequestResource", argv.size(),
                                argv.data());
    }
  }

  if (request.ClientRedirectReason() != ClientNavigationReason::kNone) {
    probe::FrameRequestedNavigation(frame_, frame_, url,
                                    request.ClientRedirectReason());
  }

  const network::mojom::IPAddressSpace initiator_address_space =
      origin_document ? origin_document->AddressSpace()
                      : network::mojom::IPAddressSpace::kUnknown;

  Client()->BeginNavigation(
      resource_request, request.GetFrameType(), origin_document,
      nullptr /* document_loader */, navigation_type,
      request.GetNavigationPolicy(), has_transient_activation, frame_load_type,
      request.ClientRedirect() == ClientRedirectPolicy::kClientRedirect,
      request.GetTriggeringEventInfo(), request.Form(),
      request.ShouldCheckMainWorldContentSecurityPolicy(),
      request.GetBlobURLToken(), request.GetInputStartTime(),
      request.HrefTranslate().GetString(), std::move(initiator_csp),
      initiator_address_space, std::move(navigation_initiator));
}

static void FillStaticResponseIfNeeded(WebNavigationParams* params,
                                       LocalFrame* frame) {
  if (params->is_static_data)
    return;
  const KURL& url = params->url;
  // See WebNavigationParams for special case explanations.
  if (url.IsAboutSrcdocURL()) {
    // TODO(dgozman): instead of reaching to the owner here, we could instead:
    // - grab the "srcdoc" value when starting a navigation right in the owner;
    // - pass it around through BeginNavigation to CommitNavigation as |data|;
    // - use it here instead of re-reading from the owner.
    // This way we will get rid of extra dependency between starting and
    // committing navigation.
    String srcdoc;
    HTMLFrameOwnerElement* owner_element = frame->DeprecatedLocalOwner();
    if (!IsA<HTMLIFrameElement>(owner_element) ||
        !owner_element->FastHasAttribute(html_names::kSrcdocAttr)) {
      // Cannot retrieve srcdoc content anymore (perhaps, the attribute was
      // cleared) - load empty instead.
    } else {
      srcdoc = owner_element->FastGetAttribute(html_names::kSrcdocAttr);
      DCHECK(!srcdoc.IsNull());
    }
    WebNavigationParams::FillStaticResponse(params, "text/html", "UTF-8",
                                            StringUTF8Adaptor(srcdoc));
    return;
  }

  MHTMLArchive* archive = nullptr;
  if (auto* parent = DynamicTo<LocalFrame>(frame->Tree().Parent()))
    archive = parent->Loader().GetDocumentLoader()->Archive();
  if (archive && !url.ProtocolIsData()) {
    // If we have an archive loaded in some ancestor frame, we should
    // retrieve document content from that archive. This is different from
    // loading an archive into this frame, which will be handled separately
    // once we load the body and parse it as an archive.
    params->body_loader.reset();
    ArchiveResource* archive_resource = archive->SubresourceForURL(url);
    if (archive_resource) {
      SharedBuffer* archive_data = archive_resource->Data();
      WebNavigationParams::FillStaticResponse(
          params, archive_resource->MimeType(),
          archive_resource->TextEncoding(),
          base::make_span(archive_data->Data(), archive_data->size()));
    }
  }
}

static bool ShouldNavigate(WebNavigationParams* params, LocalFrame* frame) {
  if (params->is_static_data)
    return true;
  if (DocumentLoader::WillLoadUrlAsEmpty(params->url))
    return true;

  int status_code = params->response.HttpStatusCode();
  if (status_code == 204 || status_code == 205) {
    // The server does not want us to replace the page contents.
    return false;
  }

  if (IsContentDispositionAttachment(
          params->response.HttpHeaderField(http_names::kContentDisposition))) {
    // The server wants us to download instead of replacing the page contents.
    // Downloading is handled by the embedder, but we still get the initial
    // response so that we can ignore it and clean up properly.
    return false;
  }

  const String& mime_type = params->response.MimeType();
  if (MIMETypeRegistry::IsSupportedMIMEType(mime_type))
    return true;
  PluginData* plugin_data = frame->GetPluginData();
  return !mime_type.IsEmpty() && plugin_data &&
         plugin_data->SupportsMimeType(mime_type);
}

void FrameLoader::CommitNavigation(
    std::unique_ptr<WebNavigationParams> navigation_params,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data,
    base::OnceClosure call_before_attaching_new_document,
    bool is_javascript_url) {
  DCHECK(frame_->GetDocument());
  DCHECK(Client()->HasWebView());

  if (!frame_->IsNavigationAllowed() ||
      frame_->GetDocument()->PageDismissalEventBeingDispatched() !=
          Document::kNoDismissal) {
    // Any of the checks above should not be necessary.
    // Unfortunately, in the case of sync IPCs like print() there might be
    // reentrancy and, for example, frame detach happening.
    // See fast/loader/detach-while-printing.html for a repro.
    // TODO(https://crbug.com/862088): we should probably ignore print()
    // call in this case instead.
    return;
  }

  // TODO(dgozman): figure out the better place for this check
  // to cancel lazy load both on start and commit. Perhaps
  // CancelProvisionalLoaderForNewNavigation() is a good one.
  HTMLFrameOwnerElement* frame_owner = frame_->DeprecatedLocalOwner();
  if (frame_owner)
    frame_owner->CancelPendingLazyLoad();

  navigation_params->frame_load_type = DetermineFrameLoadType(
      navigation_params->url, navigation_params->http_method,
      nullptr /* origin_document */, navigation_params->unreachable_url,
      navigation_params->frame_load_type);

  // Note: we might actually classify this navigation as same document
  // right here in the following circumstances:
  // - the loader has already committed a navigation and notified the browser
  //   process which did not receive a message about that just yet;
  // - meanwhile, the browser process sent us a command to commit this new
  //   "cross-document" navigation, while it's actually same-document
  //   with regards to the last commit.
  // In this rare case, we intentionally proceed as cross-document.

  RecordLatestRequiredCSP();

  if (!CancelProvisionalLoaderForNewNavigation())
    return;

  FillStaticResponseIfNeeded(navigation_params.get(), frame_);
  if (!ShouldNavigate(navigation_params.get(), frame_)) {
    DidFinishNavigation(FrameLoader::NavigationFinishState::kSuccess);
    return;
  }

  // TODO(dgozman): navigation type should probably be passed by the caller.
  // It seems incorrect to pass |false| for |have_event| and then use
  // determined navigation type to update resource request.
  WebNavigationType navigation_type = DetermineNavigationType(
      navigation_params->frame_load_type,
      !navigation_params->http_body.IsNull(), false /* have_event */);

  // Keep track of the current Document HistoryItem as the new DocumentLoader
  // might need to copy state from it. Note that the current DocumentLoader
  // should always exist, as the initial empty document is committed through
  // FrameLoader::Init.
  DCHECK(!StateMachine()->CreatingInitialEmptyDocument());
  HistoryItem* previous_history_item = GetDocumentLoader()->GetHistoryItem();

  base::Optional<Document::UnloadEventTiming> unload_timing;
  scoped_refptr<SecurityOrigin> security_origin =
      SecurityOrigin::Create(navigation_params->url);

  // TODO(dgozman): get rid of provisional document loader and most of the code
  // below. We should probably call DocumentLoader::CommitNavigation directly.
  DocumentLoader* provisional_document_loader = Client()->CreateDocumentLoader(
      frame_, navigation_type, std::move(navigation_params),
      std::move(extra_data));

  FrameSwapScope frame_swap_scope(frame_owner);

  {
    base::AutoReset<bool> scoped_committing(&committing_navigation_, true);
    if (is_javascript_url)
      provisional_document_loader->SetLoadingJavaScriptUrl();

    progress_tracker_->ProgressStarted();
    provisional_document_loader_ = provisional_document_loader;
    frame_->GetFrameScheduler()->DidStartProvisionalLoad(frame_->IsMainFrame());
    probe::DidStartProvisionalLoad(frame_);
    virtual_time_pauser_.PauseVirtualTime();

    provisional_document_loader_->StartLoading();
    virtual_time_pauser_.UnpauseVirtualTime();
    DCHECK(Client()->HasWebView());
    if (!DetachDocument(security_origin.get(), &unload_timing))
      return;
  }

  tls_version_warning_origins_.clear();

  // Following the call to StartLoading, the provisional DocumentLoader state
  // has taken into account all redirects that happened during navigation. Its
  // HistoryItem can be properly updated for the commit, using the HistoryItem
  // of the previous Document.
  provisional_document_loader_->SetHistoryItemStateForCommit(
      previous_history_item, provisional_document_loader_->LoadType(),
      DocumentLoader::HistoryNavigationType::kDifferentDocument);

  CommitDocumentLoader(provisional_document_loader_.Release(), unload_timing,
                       true /* dispatch_did_start */,
                       std::move(call_before_attaching_new_document),
                       !is_javascript_url /* dispatch_did_commit */);

  TakeObjectSnapshot();
}

bool FrameLoader::WillStartNavigation(const WebNavigationInfo& info,
                                      bool is_history_navigation_in_new_frame) {
  if (!CancelProvisionalLoaderForNewNavigation())
    return false;

  progress_tracker_->ProgressStarted();
  client_navigation_ = std::make_unique<ClientNavigationState>();
  client_navigation_->url = info.url_request.Url();
  client_navigation_->is_history_navigation_in_new_frame =
      is_history_navigation_in_new_frame;
  frame_->GetFrameScheduler()->DidStartProvisionalLoad(frame_->IsMainFrame());
  probe::DidStartProvisionalLoad(frame_);
  virtual_time_pauser_.PauseVirtualTime();
  TakeObjectSnapshot();
  return true;
}

void FrameLoader::StopAllLoaders() {
  if (!frame_->IsNavigationAllowed() ||
      frame_->GetDocument()->PageDismissalEventBeingDispatched() !=
          Document::kNoDismissal) {
    return;
  }

  // This method could be called from within this method, e.g. through plugin
  // detach. Avoid infinite recursion by disabling navigations.
  FrameNavigationDisabler navigation_disabler(*frame_);

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child))
      child_local_frame->Loader().StopAllLoaders();
  }

  frame_->GetDocument()->CancelParsing();
  if (document_loader_)
    document_loader_->StopLoading();
  DetachDocumentLoader(provisional_document_loader_);
  CancelClientNavigation();
  DidFinishNavigation(FrameLoader::NavigationFinishState::kSuccess);

  TakeObjectSnapshot();
}

void FrameLoader::DidAccessInitialDocument() {
  if (frame_->IsMainFrame()) {
    // Forbid script execution to prevent re-entering V8, since this is called
    // from a binding security check.
    ScriptForbiddenScope forbid_scripts;
    if (Client())
      Client()->DidAccessInitialDocument();
  }
}

bool FrameLoader::DetachDocument(
    SecurityOrigin* committing_origin,
    base::Optional<Document::UnloadEventTiming>* timing) {
  PluginScriptForbiddenScope forbid_plugin_destructor_scripting;
  DocumentLoader* pdl = provisional_document_loader_;

  // Don't allow this frame to navigate anymore. This line is needed for
  // navigation triggered from children's unload handlers. Blocking navigations
  // triggered from this frame's unload handler is already covered in
  // DispatchUnloadEvent().
  FrameNavigationDisabler navigation_disabler(*frame_);
  // Don't allow any new child frames to load in this frame: attaching a new
  // child frame during or after detaching children results in an attached frame
  // on a detached DOM tree, which is bad.
  SubframeLoadingDisabler disabler(frame_->GetDocument());
  // https://html.spec.whatwg.org/C/browsing-the-web.html#unload-a-document
  // The ignore-opens-during-unload counter of a Document must be incremented
  // both when unloading itself and when unloading its descendants.
  IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
      frame_->GetDocument());
  if (document_loader_)
    DispatchUnloadEvent(committing_origin, timing);
  frame_->DetachChildren();
  // The previous calls to dispatchUnloadEvent() and detachChildren() can
  // execute arbitrary script via things like unload events. If the executed
  // script causes the current frame to be detached, we need to abandon the
  // current load.
  if (!frame_->Client())
    return false;
  // FrameNavigationDisabler should prevent another load from starting.
  DCHECK_EQ(provisional_document_loader_, pdl);
  // detachFromFrame() will abort XHRs that haven't completed, which can trigger
  // event listeners for 'abort'. These event listeners might call
  // window.stop(), which will in turn detach the provisional document loader.
  // At this point, the provisional document loader should not detach, because
  // then the FrameLoader would not have any attached DocumentLoaders. This is
  // guaranteed by FrameNavigationDisabler above.
  if (document_loader_)
    DetachDocumentLoader(document_loader_, true);
  // 'abort' listeners can also detach the frame.
  if (!frame_->Client())
    return false;
  // FrameNavigationDisabler should prevent another load from starting.
  DCHECK_EQ(provisional_document_loader_, pdl);

  // No more events will be dispatched so detach the Document.
  // TODO(yoav): Should we also be nullifying domWindow's document (or
  // domWindow) since the doc is now detached?
  if (frame_->GetDocument())
    frame_->GetDocument()->Shutdown();
  document_loader_ = nullptr;

  return true;
}

void FrameLoader::CommitDocumentLoader(
    DocumentLoader* document_loader,
    const base::Optional<Document::UnloadEventTiming>& unload_timing,
    bool dispatch_did_start,
    base::OnceClosure call_before_attaching_new_document,
    bool dispatch_did_commit) {
  document_loader_ = document_loader;
  CHECK(document_loader_);

  // Update the DocumentLoadTiming with the timings from the previous document
  // unload event.
  if (unload_timing.has_value()) {
    document_loader_->GetTiming().SetHasSameOriginAsPreviousDocument(true);
    document_loader_->GetTiming().MarkUnloadEventStart(
        unload_timing->unload_event_start);
    document_loader_->GetTiming().MarkUnloadEventEnd(
        unload_timing->unload_event_end);
  }

  document_loader_->MarkAsCommitted();

  TakeObjectSnapshot();

  Client()->TransitionToCommittedForNewPage();

  document_loader_->CommitNavigation();

  {
    FrameNavigationDisabler navigation_disabler(*frame_);
    // TODO(https://crbug.com/855189): replace DispatchDidStartProvisionalLoad,
    // call_before_attaching_new_document and DispatchDidCommitLoad with a
    // single call.
    if (dispatch_did_start)
      Client()->DispatchDidStartProvisionalLoad(document_loader_);
    std::move(call_before_attaching_new_document).Run();
    Client()->DidCreateNewDocument();
    if (dispatch_did_commit) {
      // TODO(https://crbug.com/855189): Do not make exceptions
      // for javascript urls.
      Client()->DispatchDidCommitLoad(
          document_loader_->GetHistoryItem(),
          DocumentLoader::LoadTypeToCommitType(document_loader_->LoadType()),
          document_loader_->GetGlobalObjectReusePolicy());
    }
    // TODO(dgozman): make DidCreateScriptContext notification call currently
    // triggered by installing new document happen here, after commit.
  }
  if (document_loader_->LoadType() == WebFrameLoadType::kBackForward) {
    if (Page* page = frame_->GetPage())
      page->HistoryNavigationVirtualTimePauser().UnpauseVirtualTime();
  }

  // Load the document if needed.
  document_loader_->StartLoadingResponse();
}

void FrameLoader::RestoreScrollPositionAndViewState() {
  if (!frame_->GetPage() || !GetDocumentLoader() ||
      !GetDocumentLoader()->GetHistoryItem() ||
      !GetDocumentLoader()->GetHistoryItem()->GetViewState() ||
      in_restore_scroll_) {
    return;
  }
  base::AutoReset<bool> in_restore_scroll(&in_restore_scroll_, true);
  RestoreScrollPositionAndViewState(
      GetDocumentLoader()->LoadType(), false /* is_same_document */,
      *GetDocumentLoader()->GetHistoryItem()->GetViewState(),
      GetDocumentLoader()->GetHistoryItem()->ScrollRestorationType());
}

void FrameLoader::RestoreScrollPositionAndViewState(
    WebFrameLoadType load_type,
    bool is_same_document,
    const HistoryItem::ViewState& view_state,
    HistoryScrollRestorationType scroll_restoration_type) {
  LocalFrameView* view = frame_->View();
  if (!view || !view->LayoutViewport() ||
      !state_machine_.CommittedFirstRealDocumentLoad() ||
      !frame_->IsAttached()) {
    return;
  }
  if (!NeedsHistoryItemRestore(load_type))
    return;

  bool should_restore_scroll =
      scroll_restoration_type != kScrollRestorationManual;
  bool should_restore_scale = view_state.page_scale_factor_;

  // This tries to balance:
  // 1. restoring as soon as possible.
  // 2. not overriding user scroll (TODO(majidvp): also respect user scale).
  // 3. detecting clamping to avoid repeatedly popping the scroll position down
  //    as the page height increases.
  // 4. forcing a layout if necessary to avoid clamping.
  // 5. ignoring clamp detection if scroll state is not being restored, if load
  //    is complete, or if the navigation is same-document (as the new page may
  //    be smaller than the previous page).
  bool can_restore_without_clamping =
      view->LayoutViewport()->ClampScrollOffset(view_state.scroll_offset_) ==
      view_state.scroll_offset_;

  bool should_force_clamping = !frame_->IsLoading() || is_same_document;
  // Here |can_restore_without_clamping| is false, but layout might be necessary
  // to ensure correct content size.
  if (!can_restore_without_clamping && should_force_clamping)
    frame_->GetDocument()->UpdateStyleAndLayout();

  bool can_restore_without_annoying_user =
      !GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user &&
      (can_restore_without_clamping || should_force_clamping ||
       !should_restore_scroll);
  if (!can_restore_without_annoying_user)
    return;

  if (should_restore_scroll) {
    // TODO(pnoland): attempt to restore the anchor in more places than this.
    // Anchor-based restore should allow for earlier restoration.
    bool did_restore = view->LayoutViewport()->RestoreScrollAnchor(
        {view_state.scroll_anchor_data_.selector_,
         LayoutPoint(view_state.scroll_anchor_data_.offset_.x,
                     view_state.scroll_anchor_data_.offset_.y),
         view_state.scroll_anchor_data_.simhash_});
    if (!did_restore) {
      view->LayoutViewport()->SetScrollOffset(view_state.scroll_offset_,
                                              kProgrammaticScroll);
    }
  }

  // For main frame restore scale and visual viewport position
  if (frame_->IsMainFrame()) {
    ScrollOffset visual_viewport_offset(
        view_state.visual_viewport_scroll_offset_);

    // If the visual viewport's offset is (-1, -1) it means the history item
    // is an old version of HistoryItem so distribute the scroll between
    // the main frame and the visual viewport as best as we can.
    if (visual_viewport_offset.Width() == -1 &&
        visual_viewport_offset.Height() == -1) {
      visual_viewport_offset =
          view_state.scroll_offset_ - view->LayoutViewport()->GetScrollOffset();
    }

    VisualViewport& visual_viewport = frame_->GetPage()->GetVisualViewport();
    if (should_restore_scale && should_restore_scroll) {
      visual_viewport.SetScaleAndLocation(
          view_state.page_scale_factor_, visual_viewport.IsPinchGestureActive(),
          FloatPoint(visual_viewport_offset));
    } else if (should_restore_scale) {
      visual_viewport.SetScale(view_state.page_scale_factor_);
    } else if (should_restore_scroll) {
      visual_viewport.SetLocation(FloatPoint(visual_viewport_offset));
    }

    if (ScrollingCoordinator* scrolling_coordinator =
            frame_->GetPage()->GetScrollingCoordinator())
      scrolling_coordinator->FrameViewRootLayerDidChange(view);
  }

  GetDocumentLoader()->GetInitialScrollState().did_restore_from_history = true;
}

String FrameLoader::UserAgent() const {
  String user_agent = Client()->UserAgent();
  probe::ApplyUserAgentOverride(probe::ToCoreProbeSink(frame_->GetDocument()),
                                &user_agent);
  return user_agent;
}

blink::UserAgentMetadata FrameLoader::UserAgentMetadata() const {
  // TODO(mkwst): Support overrides.
  return Client()->UserAgentMetadata();
}

void FrameLoader::Detach() {
  frame_->GetDocument()->CancelParsing();
  DetachDocumentLoader(document_loader_);
  if (provisional_document_loader_) {
    // Suppress client notification about failed provisional
    // load - it does not bring any value when the frame is
    // being detached anyway.
    provisional_document_loader_->SetSentDidFinishLoad();
    DetachDocumentLoader(provisional_document_loader_);
  }
  ClearClientNavigation();
  committing_navigation_ = false;
  DidFinishNavigation(FrameLoader::NavigationFinishState::kSuccess);

  if (progress_tracker_) {
    progress_tracker_->Dispose();
    progress_tracker_.Clear();
  }

  TRACE_EVENT_OBJECT_DELETED_WITH_ID("loading", "FrameLoader", this);
  detached_ = true;
  virtual_time_pauser_.UnpauseVirtualTime();
}

bool FrameLoader::MaybeRenderFallbackContent() {
  DCHECK(frame_->Owner() && frame_->Owner()->CanRenderFallbackContent());
  // |client_navigation_| can be null here:
  // 1. We asked client to navigation through BeginNavigation();
  // 2. Meanwhile, another navigation has been started, e.g. to about:srcdoc.
  //    This navigation has been processed, |client_navigation_| has been
  //    reset, and browser process was informed about cancellation.
  // 3. Before the cancellation reached the browser process, it decided that
  //    first navigation has failed and asks to commit the failed navigation.
  // 4. We come here, while |client_navigation_| is null.
  // TODO(dgozman): shouldn't we abandon the commit of navigation failure
  // because we've already notified the client about cancellation? This needs
  // to be double-checked, perhaps this is dead code.
  if (!client_navigation_)
    return false;

  frame_->Owner()->RenderFallbackContent(frame_);
  ClearClientNavigation();
  DidFinishNavigation(FrameLoader::NavigationFinishState::kSuccess);
  return true;
}

void FrameLoader::DetachProvisionalDocumentLoader() {
  DetachDocumentLoader(provisional_document_loader_);
}

bool FrameLoader::ShouldPerformFragmentNavigation(bool is_form_submission,
                                                  const String& http_method,
                                                  WebFrameLoadType load_type,
                                                  const KURL& url) {
  // We don't do this if we are submitting a form with method other than "GET",
  // explicitly reloading, currently displaying a frameset, or if the URL does
  // not have a fragment.
  return DeprecatedEqualIgnoringCase(http_method, http_names::kGET) &&
         !IsReloadLoadType(load_type) &&
         load_type != WebFrameLoadType::kBackForward &&
         url.HasFragmentIdentifier() &&
         // For provisional LocalFrame, there is no real document loaded and
         // the initial empty document should not be considered, so there is
         // no way to get a same-document load in this case.
         !frame_->IsProvisional() &&
         EqualIgnoringFragmentIdentifier(frame_->GetDocument()->Url(), url)
         // We don't want to just scroll if a link from within a frameset is
         // trying to reload the frameset into _top.
         && !frame_->GetDocument()->IsFrameSet();
}

void FrameLoader::ProcessFragment(const KURL& url,
                                  WebFrameLoadType frame_load_type,
                                  LoadStartType load_start_type) {
  LocalFrameView* view = frame_->View();
  if (!view)
    return;

  // Leaking scroll position to a cross-origin ancestor would permit the
  // so-called "framesniffing" attack.
  Frame* boundary_frame =
      url.HasFragmentIdentifier()
          ? frame_->FindUnsafeParentScrollPropagationBoundary()
          : nullptr;

  // FIXME: Handle RemoteFrames
  if (auto* boundary_local_frame = DynamicTo<LocalFrame>(boundary_frame))
    boundary_local_frame->View()->SetSafeToPropagateScrollToParent(false);

  // If scroll position is restored from history fragment or scroll
  // restoration type is manual, then we should not override it unless this
  // is a same document reload.
  bool should_scroll_to_fragment =
      (load_start_type == kNavigationWithinSameDocument &&
       !IsBackForwardLoadType(frame_load_type)) ||
      (!GetDocumentLoader()->GetInitialScrollState().did_restore_from_history &&
       !(GetDocumentLoader()->GetHistoryItem() &&
         GetDocumentLoader()->GetHistoryItem()->ScrollRestorationType() ==
             kScrollRestorationManual));

  view->ProcessUrlFragment(url,
                           load_start_type == kNavigationWithinSameDocument,
                           should_scroll_to_fragment);

  if (auto* boundary_local_frame = DynamicTo<LocalFrame>(boundary_frame))
    boundary_local_frame->View()->SetSafeToPropagateScrollToParent(true);
}

bool FrameLoader::ShouldClose(bool is_reload) {
  Page* page = frame_->GetPage();
  if (!page || !page->GetChromeClient().CanOpenBeforeUnloadConfirmPanel())
    return true;

  HeapVector<Member<LocalFrame>> descendant_frames;
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().TraverseNext(frame_)) {
    // FIXME: There is not yet any way to dispatch events to out-of-process
    // frames.
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child))
      descendant_frames.push_back(child_local_frame);
  }

  {
    FrameNavigationDisabler navigation_disabler(*frame_);
    bool did_allow_navigation = false;

    // https://html.spec.whatwg.org/C/browsing-the-web.html#prompt-to-unload-a-document

    // First deal with this frame.
    IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
        frame_->GetDocument());
    if (!frame_->GetDocument()->DispatchBeforeUnloadEvent(
            &page->GetChromeClient(), is_reload, did_allow_navigation))
      return false;

    // Then deal with descendent frames.
    for (Member<LocalFrame>& descendant_frame : descendant_frames) {
      if (!descendant_frame->Tree().IsDescendantOf(frame_))
        continue;

      // There is some confusion in the spec around what counters should be
      // incremented for a descendant browsing context:
      // https://github.com/whatwg/html/issues/3899
      //
      // Here for implementation ease, we use the current spec behavior, which
      // is to increment only the counter of the Document on which this is
      // called, and that of the Document we are firing the beforeunload event
      // on -- not any intermediate Documents that may be the parent of the
      // frame being unloaded but is not root Document.
      IgnoreOpensDuringUnloadCountIncrementer
          ignore_opens_during_unload_descendant(
              descendant_frame->GetDocument());
      if (!descendant_frame->GetDocument()->DispatchBeforeUnloadEvent(
              &page->GetChromeClient(), is_reload, did_allow_navigation))
        return false;
    }
  }

  return true;
}

void FrameLoader::DidDropNavigation() {
  if (!client_navigation_)
    return;
  // TODO(dgozman): should we ClearClientNavigation instead and not
  // notify the client in response to its own call?
  CancelClientNavigation();
  DidFinishNavigation(FrameLoader::NavigationFinishState::kSuccess);

  // Forcibly instantiate WindowProxy for initial frame document.
  // This is only required when frame navigation is aborted, e.g. due to
  // mixed content.
  // TODO(lushnikov): this should be done in Init for initial empty doc, but
  // that breaks extensions abusing SetForceMainWorldInitialization setting
  // and relying on the number of created window proxies.
  Settings* settings = frame_->GetSettings();
  if (settings && settings->GetForceMainWorldInitialization()) {
    // Forcibly instantiate WindowProxy.
    frame_->GetScriptController().WindowProxy(DOMWrapperWorld::MainWorld());
  }
}

void FrameLoader::MarkAsLoading() {
  // This should only be called for initial history navigation in child frame.
  DCHECK(!provisional_document_loader_ && !client_navigation_);
  DCHECK(frame_->GetDocument()->IsLoadCompleted());
  DCHECK(frame_->GetDocument()->HasFinishedParsing());
  progress_tracker_->ProgressStarted();
}

bool FrameLoader::ShouldReuseDefaultView(
    const scoped_refptr<const SecurityOrigin>& origin,
    const ContentSecurityPolicy* csp) {
  // Secure transitions can only happen when navigating from the initial empty
  // document.
  if (!state_machine_.IsDisplayingInitialEmptyDocument())
    return false;

  // The Window object should only be re-used if it is same-origin.
  // Since sandboxing turns the origin into an opaque origin it needs to also
  // be considered when deciding whether to reuse it.
  // Spec:
  // https://html.spec.whatwg.org/C/#initialise-the-document-object
  if ((csp && (csp->GetSandboxMask() & WebSandboxFlags::kOrigin) !=
                  WebSandboxFlags::kNone) ||
      ((EffectiveSandboxFlags() & WebSandboxFlags::kOrigin) !=
       WebSandboxFlags::kNone)) {
    return false;
  }

  return frame_->GetDocument()->GetSecurityOrigin()->CanAccess(origin.get());
}

bool FrameLoader::CancelProvisionalLoaderForNewNavigation() {
  // This seems to correspond to step 9 of the specification:
  // "9. Abort the active document of browsingContext."
  // https://html.spec.whatwg.org/C/#navigate
  frame_->GetDocument()->Abort();
  // document.onreadystatechange can fire in Abort(), which can:
  // 1) Detach this frame.
  // 2) Stop the provisional DocumentLoader (i.e window.stop()).
  if (!frame_->GetPage())
    return false;

  DetachDocumentLoader(provisional_document_loader_);
  // Detaching the provisional DocumentLoader above may leave the frame without
  // any loading DocumentLoader. It can causes the 'load' event to fire, which
  // can be used to detach this frame.
  if (!frame_->GetPage())
    return false;

  // For client navigations, don't send failure callbacks when simply
  // replacing client navigation with a DocumentLoader.
  ClearClientNavigation();

  return true;
}

void FrameLoader::ClearClientNavigation() {
  if (!client_navigation_)
    return;
  client_navigation_.reset();
  probe::DidFailProvisionalLoad(frame_);
  virtual_time_pauser_.UnpauseVirtualTime();
}

void FrameLoader::CancelClientNavigation() {
  if (!client_navigation_)
    return;
  ResourceError error = ResourceError::CancelledError(client_navigation_->url);
  ClearClientNavigation();
  if (WebPluginContainerImpl* plugin = frame_->GetWebPluginContainer())
    plugin->DidFailLoading(error);
  Client()->AbortClientNavigation();
}

void FrameLoader::DispatchDocumentElementAvailable() {
  ScriptForbiddenScope forbid_scripts;
  Client()->DocumentElementAvailable();
}

void FrameLoader::RunScriptsAtDocumentElementAvailable() {
  Client()->RunScriptsAtDocumentElementAvailable();
  // The frame might be detached at this point.
}

void FrameLoader::DispatchDidClearDocumentOfWindowObject() {
  DCHECK(frame_->GetDocument());
  if (state_machine_.CreatingInitialEmptyDocument())
    return;

  Settings* settings = frame_->GetSettings();
  if (settings && settings->GetForceMainWorldInitialization()) {
    // Forcibly instantiate WindowProxy, even if script is disabled.
    frame_->GetScriptController().WindowProxy(DOMWrapperWorld::MainWorld());
  }
  probe::DidClearDocumentOfWindowObject(frame_);
  if (!frame_->GetDocument()->CanExecuteScripts(kNotAboutToExecuteScript))
    return;

  if (dispatching_did_clear_window_object_in_main_world_)
    return;
  base::AutoReset<bool> in_did_clear_window_object(
      &dispatching_did_clear_window_object_in_main_world_, true);
  // We just cleared the document, not the entire window object, but for the
  // embedder that's close enough.
  Client()->DispatchDidClearWindowObjectInMainWorld();
}

void FrameLoader::DispatchDidClearWindowObjectInMainWorld() {
  DCHECK(frame_->GetDocument());
  if (!frame_->GetDocument()->CanExecuteScripts(kNotAboutToExecuteScript))
    return;

  if (dispatching_did_clear_window_object_in_main_world_)
    return;
  base::AutoReset<bool> in_did_clear_window_object(
      &dispatching_did_clear_window_object_in_main_world_, true);
  Client()->DispatchDidClearWindowObjectInMainWorld();
}

SandboxFlags FrameLoader::EffectiveSandboxFlags() const {
  SandboxFlags flags = forced_sandbox_flags_;
  if (FrameOwner* frame_owner = frame_->Owner())
    flags |= frame_owner->GetFramePolicy().sandbox_flags;
  // Frames need to inherit the sandbox flags of their parent frame.
  if (Frame* parent_frame = frame_->Tree().Parent())
    flags |= parent_frame->GetSecurityContext()->GetSandboxFlags();
  return flags;
}

void FrameLoader::ModifyRequestForCSP(
    ResourceRequest& resource_request,
    const FetchClientSettingsObject* fetch_client_settings_object,
    Document* document_for_logging,
    network::mojom::RequestContextFrameType frame_type) const {
  if (!RequiredCSP().IsEmpty()) {
    DCHECK(
        ContentSecurityPolicy::IsValidCSPAttr(RequiredCSP().GetString(), ""));
    resource_request.SetHttpHeaderField(http_names::kSecRequiredCSP,
                                        RequiredCSP());
  }

  // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational
  // requests, as described in
  // https://w3c.github.io/webappsec-upgrade-insecure-requests/#feature-detect
  if (frame_type != network::mojom::RequestContextFrameType::kNone) {
    // Early return if the request has already been upgraded.
    if (!resource_request.HttpHeaderField(http_names::kUpgradeInsecureRequests)
             .IsNull()) {
      return;
    }

    resource_request.SetHttpHeaderField(http_names::kUpgradeInsecureRequests,
                                        "1");
  }

  MixedContentChecker::UpgradeInsecureRequest(resource_request,
                                              fetch_client_settings_object,
                                              document_for_logging, frame_type);
}

void FrameLoader::ReportLegacyTLSVersion(const KURL& url,
                                         bool is_subresource,
                                         bool is_ad_resource) {
  document_loader_->GetUseCounterHelper().Count(
      is_subresource
          ? WebFeature::kLegacyTLSVersionInSubresource
          : (frame_->Tree().Parent()
                 ? WebFeature::kLegacyTLSVersionInSubframeMainResource
                 : WebFeature::kLegacyTLSVersionInMainFrameResource),
      frame_.Get());

  // For non-main-frame loads, we have to use the main frame's document for
  // the UKM recorder and source ID.
  auto& root = frame_->LocalFrameRoot();
  ukm::builders::Net_LegacyTLSVersion(root.GetDocument()->UkmSourceID())
      .SetIsMainFrame(frame_->IsMainFrame())
      .SetIsSubresource(is_subresource)
      .SetIsAdResource(is_ad_resource)
      .Record(root.GetDocument()->UkmRecorder());

  // Web tests use an outdated server on macOS. See https://crbug.com/936515.
#if defined(OS_MACOSX)
  if (WebTestSupport::IsRunningWebTest())
    return;
#endif

  String origin = SecurityOrigin::Create(url)->ToString();
  // To prevent log spam, only log the message once per origin.
  if (tls_version_warning_origins_.Contains(origin))
    return;

  // After |kMaxSecurityWarningMessages| warnings, stop printing messages to the
  // console. At exactly |kMaxSecurityWarningMessages| warnings, print a message
  // that additional resources on the page use legacy certificates without
  // specifying which exact resources. Before |kMaxSecurityWarningMessages|
  // messages, print the exact resource URL in the message to help the developer
  // pinpoint the problematic resources.
  const size_t kMaxSecurityWarningMessages = 10;
  size_t num_warnings = tls_version_warning_origins_.size();
  if (num_warnings > kMaxSecurityWarningMessages)
    return;

  String console_message;
  if (num_warnings == kMaxSecurityWarningMessages) {
    console_message =
        "Additional resources on this page were loaded with TLS 1.0 or TLS "
        "1.1, which are deprecated and will be disabled in the future. Once "
        "disabled, users will be prevented from loading these resources. "
        "Servers should enable TLS 1.2 or later. See "
        "https://www.chromestatus.com/feature/5654791610957824 for more "
        "information.";
  } else {
    console_message =
        "The connection used to load resources from " + origin +
        " used TLS 1.0 or TLS "
        "1.1, which are deprecated and will be disabled in the future. Once "
        "disabled, users will be prevented from loading these resources. The "
        "server should enable TLS 1.2 or later. See "
        "https://www.chromestatus.com/feature/5654791610957824 for more "
        "information.";
  }
  tls_version_warning_origins_.insert(origin);
  // To avoid spamming the console, use verbose message level for subframe
  // resources, and only use the warning level for main-frame resources.
  frame_->Console().AddMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kOther,
      frame_->IsMainFrame() ? mojom::ConsoleMessageLevel::kWarning
                            : mojom::ConsoleMessageLevel::kVerbose,
      console_message));
}

void FrameLoader::RecordLatestRequiredCSP() {
  required_csp_ =
      frame_->Owner() ? frame_->Owner()->RequiredCsp() : g_null_atom;
}

std::unique_ptr<TracedValue> FrameLoader::ToTracedValue() const {
  auto traced_value = std::make_unique<TracedValue>();
  traced_value->BeginDictionary("frame");
  traced_value->SetString("id_ref", IdentifiersFactory::FrameId(frame_.Get()));
  traced_value->EndDictionary();
  traced_value->SetBoolean("isLoadingMainFrame", frame_->IsMainFrame());
  traced_value->SetString("stateMachine", state_machine_.ToString());
  traced_value->SetString("provisionalDocumentLoaderURL",
                          provisional_document_loader_
                              ? provisional_document_loader_->Url().GetString()
                              : String());
  traced_value->SetString(
      "documentLoaderURL",
      document_loader_ ? document_loader_->Url().GetString() : String());
  return traced_value;
}

inline void FrameLoader::TakeObjectSnapshot() const {
  if (detached_) {
    // We already logged TRACE_EVENT_OBJECT_DELETED_WITH_ID in detach().
    return;
  }
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID("loading", "FrameLoader", this,
                                      ToTracedValue());
}

bool FrameLoader::IsClientNavigationInitialHistoryLoad() {
  return client_navigation_ &&
         client_navigation_->is_history_navigation_in_new_frame;
}

STATIC_ASSERT_ENUM(kWebHistoryScrollRestorationManual,
                   kScrollRestorationManual);
STATIC_ASSERT_ENUM(kWebHistoryScrollRestorationAuto, kScrollRestorationAuto);

}  // namespace blink
