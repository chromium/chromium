/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/local_frame_client_impl.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "components/viz/common/frame_timing_details.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "net/storage_access_api/status.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/loader/fetch_later.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_client.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_background_resource_fetch_assets.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_manifest_manager.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Convenience helper for frame tree helpers in FrameClient to reduce the amount
// of null-checking boilerplate code. Since the frame tree is maintained in the
// web/ layer, the frame tree helpers often have to deal with null WebFrames:
// for example, a frame with no parent will return null for WebFrame::parent().
// TODO(dcheng): Remove duplication between LocalFrameClientImpl and
// RemoteFrameClientImpl somehow...
Frame* ToCoreFrame(WebFrame* frame) {
  return frame ? WebFrame::ToCoreFrame(*frame) : nullptr;
}

// Return the parent of |frame| as a LocalFrame, nullptr when there is no
// parent or when the parent is a remote frame.
LocalFrame* GetLocalParentFrame(WebLocalFrameImpl* frame) {
  WebFrame* parent = frame->Parent();
  auto* parent_web_local_frame = DynamicTo<WebLocalFrameImpl>(parent);
  if (!parent_web_local_frame)
    return nullptr;

  return parent_web_local_frame->GetFrame();
}

// Returns whether the |local_frame| has been loaded using an MHTMLArchive. When
// it is the case, each subframe must use it for loading.
bool IsLoadedAsMHTMLArchive(LocalFrame* local_frame) {
  return local_frame && local_frame->GetDocument()->Fetcher()->Archive();
}

// Returns whether the |local_frame| is in a middle of a back/forward
// navigation.
bool IsBackForwardNavigationInProgress(LocalFrame* local_frame) {
  return local_frame &&
         IsBackForwardOrRestore(
             local_frame->Loader().GetDocumentLoader()->LoadType()) &&
         !local_frame->GetDocument()->LoadEventFinished();
}

// Called after committing provisional load to reset the EventHandlerProperties.
// Only called on local frame roots.
void ResetWheelAndTouchEventHandlerProperties(LocalFrame& frame) {
  // If we are loading a local root, it is important to explicitly set the event
  // listener properties to Nothing as this triggers notifications to the
  // client. Clients may assume the presence of handlers for touch and wheel
  // events, so these notifications tell it there are (presently) no handlers.
  auto& chrome_client = frame.GetPage()->GetChromeClient();
  chrome_client.SetEventListenerProperties(
      &frame, cc::EventListenerClass::kTouchStartOrMove,
      cc::EventListenerProperties::kNone);
  chrome_client.SetEventListenerProperties(&frame,
                                           cc::EventListenerClass::kMouseWheel,
                                           cc::EventListenerProperties::kNone);
  chrome_client.SetEventListenerProperties(
      &frame, cc::EventListenerClass::kTouchEndOrCancel,
      cc::EventListenerProperties::kNone);
}

bool IsCompositedOutermostMainFrame(WebLocalFrameImpl* web_frame) {
  return web_frame->GetFrame()->IsMainFrame() &&
         !web_frame->IsInFencedFrameTree() &&
         web_frame->ViewImpl()->does_composite();
}

}  // namespace

LocalFrameClientImpl::LocalFrameClientImpl(WebLocalFrameImpl* frame)
    : web_frame_(frame) {}

LocalFrameClientImpl::~LocalFrameClientImpl() = default;

void LocalFrameClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(web_frame_);
  LocalFrameClient::Trace(visitor);
}

WebLocalFrameImpl* LocalFrameClientImpl::GetWebFrame() const {
  return web_frame_.Get();
}

WebContentCaptureClient* LocalFrameClientImpl::GetWebContentCaptureClient()
    const {
  return web_frame_->ContentCaptureClient();
}

void LocalFrameClientImpl::DidCommitDocumentReplacementNavigation(
    DocumentLoader* loader) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidCommitDocumentReplacementNavigation(loader);
  }
}

void LocalFrameClientImpl::DispatchDidClearWindowObjectInMainWorld(
    v8::Isolate* isolate,
    v8::MicrotaskQueue* microtask_queue) {
  if (web_frame_->Client()) {
    // Do not run microtasks while invoking the callback.
    {
      v8::MicrotasksScope microtasks(isolate, microtask_queue,
                                     v8::MicrotasksScope::kDoNotRunMicrotasks);
      web_frame_->Client()->DidClearWindowObject();
    }
    Document* document = web_frame_->GetFrame()->GetDocument();
    if (document) {
      const Settings* const settings = web_frame_->GetFrame()->GetSettings();
      CoreInitializer::GetInstance().OnClearWindowObjectInMainWorld(*document,
                                                                    *settings);
    }
  }
}

void LocalFrameClientImpl::DocumentElementAvailable() {
  if (web_frame_->Client())
    web_frame_->Client()->DidCreateDocumentElement();
}

void LocalFrameClientImpl::RunScriptsAtDocumentElementAvailable() {
  if (web_frame_->Client())
    web_frame_->Client()->RunScriptsAtDocumentElementAvailable();
  // The callback might have deleted the frame, do not use |this|!
}

void LocalFrameClientImpl::RunScriptsAtDocumentReady(bool document_is_empty) {
  if (!document_is_empty && IsLoadedAsMHTMLArchive(web_frame_->GetFrame())) {
    // For MHTML pages, recreate the shadow DOM contents from the templates that
    // are captured from the shadow DOM trees at serialization.
    // Note that the MHTML page is loaded in sandboxing mode with script
    // execution disabled and thus only the following script will be executed.
    // Any other scripts and event handlers outside the scope of the following
    // script, including those that may be inserted in shadow DOM templates,
    // will NOT be run.
    String script = R"(
function createShadowRootWithin(node) {
  var nodes = node.querySelectorAll('template[shadowmode]');
  for (var i = 0; i < nodes.length; ++i) {
    var template = nodes[i];
    var mode = template.getAttribute('shadowmode');
    var parent = template.parentNode;
    if (!parent)
      continue;
    parent.removeChild(template);
    var shadowRoot;
    if (mode == 'open' || mode == 'closed') {
      var delegatesFocus = template.hasAttribute('shadowdelegatesfocus');
      shadowRoot = parent.attachShadow({'mode': mode,
                                        'delegatesFocus': delegatesFocus});
    }
    if (!shadowRoot)
      continue;
    var clone = document.importNode(template.content, true);
    shadowRoot.appendChild(clone);
    createShadowRootWithin(shadowRoot);
  }
}
createShadowRootWithin(document.body);
)";
    ClassicScript::CreateUnspecifiedScript(script,
                                           ScriptSourceLocationType::kInternal)
        ->RunScript(web_frame_->GetFrame()->DomWindow(),
                    ExecuteScriptPolicy::kExecuteScriptWhenScriptsDisabled);
  }

  if (web_frame_->Client()) {
    web_frame_->Client()->RunScriptsAtDocumentReady();
  }
  // The callback might have deleted the frame, do not use |this|!
}

void LocalFrameClientImpl::RunScriptsAtDocumentIdle() {
  if (web_frame_->Client())
    web_frame_->Client()->RunScriptsAtDocumentIdle();
  // The callback might have deleted the frame, do not use |this|!
}

void LocalFrameClientImpl::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  if (web_frame_->Client())
    web_frame_->Client()->DidCreateScriptContext(context, world_id);
}

void LocalFrameClientImpl::WillReleaseScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  if (web_frame_->Client()) {
    web_frame_->Client()->WillReleaseScriptContext(context, world_id);
  }
}

bool LocalFrameClientImpl::AllowScriptExtensions() {
  return true;
}

void LocalFrameClientImpl::DidChangeScrollOffset() {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeScrollOffset();
}

void LocalFrameClientImpl::NotifyCurrentHistoryItemChanged() {
  if (web_frame_->Client())
    web_frame_->Client()->NotifyCurrentHistoryItemChanged();
}

void LocalFrameClientImpl::DidUpdateCurrentHistoryItem() {
  web_frame_->Client()->DidUpdateCurrentHistoryItem();
}

bool LocalFrameClientImpl::AllowContentInitiatedDataUrlNavigations(
    const KURL& url) {
  if (RuntimeEnabledFeatures::AllowContentInitiatedDataUrlNavigationsEnabled())
    return true;
  if (web_frame_->Client())
    return web_frame_->Client()->AllowContentInitiatedDataUrlNavigations(url);
  return false;
}

bool LocalFrameClientImpl::HasWebView() const {
  return web_frame_->ViewImpl();
}

bool LocalFrameClientImpl::InShadowTree() const {
  return web_frame_->GetTreeScopeType() == mojom::blink::TreeScopeType::kShadow;
}

void LocalFrameClientImpl::WillBeDetached() {
  web_frame_->WillBeDetached();
}

void LocalFrameClientImpl::Detached(FrameDetachType type) {
  // Alert the client that the frame is being detached. This is the last
  // chance we have to communicate with the client.
  WebLocalFrameClient* client = web_frame_->Client();
  if (!client)
    return;

  web_frame_->WillDetachParent();

  // Signal that no further communication with WebLocalFrameClient should take
  // place at this point since we are no longer associated with the Page.
  web_frame_->SetClient(nullptr);

  client->WillDetach((type == FrameDetachType::kSwap)
                         ? DetachReason::kNavigation
                         : DetachReason::kFrameDeletion);

  // We only notify the browser process when the frame is being detached for
  // removal, not after a swap.
  if (type == FrameDetachType::kRemove)
    web_frame_->GetFrame()->GetLocalFrameHostRemote().Detach();

  client->FrameDetached();

  if (type == FrameDetachType::kRemove)
    ToCoreFrame(web_frame_)->DetachFromParent();

  // Clear our reference to LocalFrame at the very end, in case the client
  // refers to it.
  web_frame_->SetCoreFrame(nullptr);
}

void LocalFrameClientImpl::DispatchFinalizeRequest(ResourceRequest& request) {
  // Give the WebLocalFrameClient a crack at the request.
  if (web_frame_->Client()) {
    WrappedResourceRequest webreq(request);
    web_frame_->Client()->FinalizeRequest(webreq);
  }
}

std::optional<KURL> LocalFrameClientImpl::DispatchWillSendRequest(
    const KURL& requested_url,
    const scoped_refptr<const SecurityOrigin>& requestor_origin,
    const net::SiteForCookies& site_for_cookies,
    bool has_redirect_info,
    const KURL& upstream_url) {
  if (!web_frame_->Client()) {
    return std::nullopt;
  }
  return web_frame_->Client()->WillSendRequest(
      requested_url, requestor_origin, site_for_cookies,
      WebLocalFrameClient::ForRedirect(has_redirect_info), upstream_url);
}

void LocalFrameClientImpl::DispatchDidDispatchDOMContentLoadedEvent() {
  if (web_frame_->Client())
    web_frame_->Client()->DidDispatchDOMContentLoadedEvent();

  web_frame_->DidDispatchDOMContentLoadedEvent();
}

void LocalFrameClientImpl::DispatchDidLoadResourceFromMemoryCache(
    const ResourceRequest& request,
    const ResourceResponse& response) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidLoadResourceFromMemoryCache(
        WrappedResourceRequest(request), WrappedResourceResponse(response));
  }
}

void LocalFrameClientImpl::DispatchDidHandleOnloadEvents() {
  if (web_frame_->Client())
    web_frame_->Client()->DidHandleOnloadEvents();
}

void LocalFrameClientImpl::DidFinishSameDocumentNavigation(
    WebHistoryCommitType commit_type,
    bool is_synchronously_committed,
    mojom::blink::SameDocumentNavigationType same_document_navigation_type,
    bool is_client_redirect,
    bool is_browser_initiated) {
  bool should_create_history_entry = commit_type == kWebStandardCommit;
  // TODO(dglazkov): Does this need to be called for subframes?
  web_frame_->ViewImpl()->DidCommitLoad(should_create_history_entry, true);
  if (web_frame_->Client()) {
    // This unique token is used to associate the session history entry, and its
    // viewport screenshot before the navigation finishes in the renderer.
    base::UnguessableToken screenshot_destination;

    // Exclude `kWebHistoryInertCommit` because these types of navigations does
    // not originate from nor add entries to the session history (i.e., they are
    // not history-traversable).
    // Exclude the WebView not being composited because we won't present any
    // frame if it is not being actively drawn.
    // Exclude cases with prefers-reduced-motion. Back forward transitions are
    // disabled in this case so no screenshots are necessary.
    // We however always propagate the history sequence number for correctness
    // in CompositedOuterMainFrame cases.
    bool navigation_with_screenshot = false;
    if (IsCompositedOutermostMainFrame(web_frame_)) {
      WebFrameWidgetImpl* frame_widget = web_frame_->FrameWidgetImpl();
      // The outermost mainframe must have a frame widget.
      CHECK(frame_widget);
      frame_widget->PropagateHistorySequenceNumberToCompositor();

      if (commit_type != kWebHistoryInertCommit &&
          !web_frame_->GetFrame()->GetSettings()->GetPrefersReducedMotion()) {
        navigation_with_screenshot = true;
        if (RuntimeEnabledFeatures::
                IncrementLocalSurfaceIdForMainframeSameDocNavigationEnabled()) {
          frame_widget->RequestNewLocalSurfaceId();
          if (RuntimeEnabledFeatures::BackForwardTransitionsEnabled()) {
            screenshot_destination = base::UnguessableToken::Create();
            frame_widget->RequestViewportScreenshot(screenshot_destination);
          }
        }

        frame_widget->NotifyPresentationTime(WTF::BindOnce(
            [](base::TimeTicks start,
               const viz::FrameTimingDetails& frame_timing_details) {
              base::TimeDelta duration =
                  frame_timing_details.presentation_feedback.timestamp - start;
              base::UmaHistogramTimes(
                  "Navigation."
                  "MainframeSameDocumentNavigationCommitToPresentFirstFrame",
                  duration);
            },
            base::TimeTicks::Now()));
      }
    }
    base::UmaHistogramBoolean("Navigation.SameDocumentNavigationWithScreenshot",
                              navigation_with_screenshot);

    std::optional<blink::SameDocNavigationScreenshotDestinationToken> token =
        std::nullopt;
    if (!screenshot_destination.is_empty()) {
      token = blink::SameDocNavigationScreenshotDestinationToken(
          screenshot_destination);
    }
    web_frame_->Client()->DidFinishSameDocumentNavigation(
        commit_type, is_synchronously_committed, same_document_navigation_type,
        is_client_redirect, token);
  }

  // Set the layout shift exclusion window for the browser initiated same
  // document navigation.
  if (is_browser_initiated) {
    LocalFrame* frame = web_frame_->GetFrame();
    if (frame) {
      frame->View()
          ->GetLayoutShiftTracker()
          .NotifyBrowserInitiatedSameDocumentNavigation();
    }
  }
}
void LocalFrameClientImpl::DidFailAsyncSameDocumentCommit() {
  web_frame_->Client()->DidFailAsyncSameDocumentCommit();
}

void LocalFrameClientImpl::DispatchDidOpenDocumentInputStream(const KURL& url) {
  web_frame_->Client()->DidOpenDocumentInputStream(url);
}

void LocalFrameClientImpl::DispatchDidReceiveTitle(const String& title) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidReceiveTitle(title);
  }
}

void LocalFrameClientImpl::DispatchDidCommitLoad(
    HistoryItem* item,
    WebHistoryCommitType commit_type,
    bool should_reset_browser_interface_broker,
    const blink::ParsedPermissionsPolicy& permissions_policy_header,
    const blink::DocumentPolicyFeatureState& document_policy_header) {
  if (!web_frame_->Parent()) {
    web_frame_->ViewImpl()->DidCommitLoad(commit_type == kWebStandardCommit,
                                          false);
  }

  if (web_frame_->Client()) {
    web_frame_->Client()->DidCommitNavigation(
        commit_type, should_reset_browser_interface_broker,
        permissions_policy_header, document_policy_header);

    // With local to local swap it's possible for the frame to be deleted as a
    // side effect of JS event handlers called in DidCommitNavigation
    // (e.g. unload).
    if (!web_frame_->Client())
      return;
    if (web_frame_->GetFrame()->IsLocalRoot()) {
      // This update should be sent as soon as loading the new document begins
      // so that the browser and compositor could reset their states. However,
      // up to this point |web_frame_| is still provisional and the updates will
      // not get sent. Revise this when https://crbug.com/578349 is fixed.
      ResetWheelAndTouchEventHandlerProperties(*web_frame_->GetFrame());

      web_frame_->FrameWidgetImpl()->DidNavigate();

      // The navigation state pushed to the compositor is limited to outermost
      // main frames. This is particularly important for UKM metrics, since we
      // only record URL keyed data if the URL is being displayed in the main
      // frame.
      if (IsCompositedOutermostMainFrame(web_frame_)) {
        WebFrameWidgetImpl* frame_widget = web_frame_->FrameWidgetImpl();

        // Update the navigation states (URL, the document source id used to key
        // UKM metrics in the compositor. Note that the metrics for all frames
        // are keyed to the main frame's URL.
        frame_widget->UpdateNavigationStateForCompositor(
            web_frame_->GetDocument().GetUkmSourceId(),
            KURL(web_frame_->Client()->LastCommittedUrlForUKM()));

        auto shmem = frame_widget->CreateSharedMemoryForSmoothnessUkm();
        if (shmem.IsValid()) {
          web_frame_->Client()->SetUpSharedMemoryForSmoothness(
              std::move(shmem));
        }
      }
    }
  }
  if (WebDevToolsAgentImpl* dev_tools =
          DevToolsAgent(/*create_if_necessary=*/false)) {
    dev_tools->DidCommitLoadForLocalFrame(web_frame_->GetFrame());
  }

  web_frame_->DidCommitLoad();
}

void LocalFrameClientImpl::DispatchDidFailLoad(
    const ResourceError& error,
    WebHistoryCommitType commit_type) {
  web_frame_->DidFailLoad(error, commit_type);
}

void LocalFrameClientImpl::DispatchDidFinishLoad() {
  web_frame_->DidFinish();
}

void LocalFrameClientImpl::DispatchDidFinishLoadForPrinting() {
  web_frame_->DidFinishLoadForPrinting();
}

void LocalFrameClientImpl::BeginNavigation(
    const ResourceRequest& request,
    const KURL& requestor_base_url,
    mojom::RequestContextFrameType frame_type,
    LocalDOMWindow* origin_window,
    DocumentLoader* document_loader,
    WebNavigationType type,
    NavigationPolicy policy,
    WebFrameLoadType frame_load_type,
    mojom::blink::ForceHistoryPush force_history_push,
    bool is_client_redirect,
    bool is_unfenced_top_navigation,
    mojom::blink::TriggeringEventInfo triggering_event_info,
    HTMLFormElement* form,
    network::mojom::CSPDisposition
        should_check_main_world_content_security_policy,
    mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token,
    base::TimeTicks input_start_time,
    const String& href_translate,
    const std::optional<Impression>& impression,
    const LocalFrameToken* initiator_frame_token,
    std::unique_ptr<SourceLocation> source_location,
    mojo::PendingRemote<mojom::blink::NavigationStateKeepAliveHandle>
        initiator_navigation_state_keep_alive_handle,
    bool is_container_initiated,
    bool has_rel_opener) {
  if (!web_frame_->Client())
    return;

  // |initiator_frame_token| and |initiator_navigation_state_keep_alive_handle|
  // should either be both specified or both null.
  DCHECK(!initiator_frame_token ==
         !initiator_navigation_state_keep_alive_handle);

  auto navigation_info = std::make_unique<WebNavigationInfo>();
  navigation_info->url_request.CopyFrom(WrappedResourceRequest(request));
  navigation_info->requestor_base_url = requestor_base_url;
  navigation_info->frame_type = frame_type;
  navigation_info->force_history_push = force_history_push;
  navigation_info->navigation_type = type;
  navigation_info->navigation_policy = static_cast<WebNavigationPolicy>(policy);
  navigation_info->has_transient_user_activation = request.HasUserGesture();
  navigation_info->is_unfenced_top_navigation = is_unfenced_top_navigation;
  navigation_info->frame_load_type = frame_load_type;
  navigation_info->is_client_redirect = is_client_redirect;
  navigation_info->triggering_event_info = triggering_event_info;
  navigation_info->should_check_main_world_content_security_policy =
      should_check_main_world_content_security_policy;
  navigation_info->blob_url_token = std::move(blob_url_token);
  navigation_info->input_start = input_start_time;
  navigation_info->initiator_frame_token =
      base::OptionalFromPtr(initiator_frame_token);
  navigation_info->initiator_navigation_state_keep_alive_handle =
      std::move(initiator_navigation_state_keep_alive_handle);
  LocalFrame* origin_frame =
      origin_window ? origin_window->GetFrame() : nullptr;
  if (origin_frame) {
    // Many navigation paths do not pass an |initiator_frame_token|, so we need
    // to compute it here.
    if (!navigation_info->initiator_frame_token) {
      navigation_info->initiator_frame_token =
          origin_frame->GetLocalFrameToken();
    }
    // Similarly, many navigation paths do not pass an
    // |initiator_navigation_state_keep_alive_handle|.
    if (!navigation_info->initiator_navigation_state_keep_alive_handle) {
      navigation_info->initiator_navigation_state_keep_alive_handle =
          origin_frame->IssueKeepAliveHandle();
    }
  } else {
    // TODO(https://crbug.com/1173409 and https://crbug.com/1059959): Check that
    // we always pass an |initiator_frame_token| and an
    // |initiator_navigation_state_keep_alive_handle| if |origin_window| is not
    // set.
  }

  navigation_info->impression = impression;

  // Allow cookie access via Storage Access API during the navigation, if the
  // initiator has obtained storage access. Note that the network service still
  // applies cookie semantics and user settings, and that this value is not
  // trusted by the browser process. (The Storage Access API is only relevant
  // when third-party cookies are blocked.)
  navigation_info->storage_access_api_status =
      origin_window ? origin_window->GetStorageAccessApiStatus()
                    : net::StorageAccessApiStatus::kNone;

  // Can be null.
  LocalFrame* local_parent_frame = GetLocalParentFrame(web_frame_);

  // Newly created child frames may need to be navigated to a history item
  // during a back/forward navigation. This will only happen when the parent
  // is a LocalFrame doing a back/forward navigation that has not completed.
  // (If the load has completed and the parent later adds a frame with script,
  // we do not want to use a history item for it.)
  navigation_info->is_history_navigation_in_new_child_frame =
      IsBackForwardNavigationInProgress(local_parent_frame);

  // TODO(nasko): How should this work with OOPIF?
  // The MHTMLArchive is parsed as a whole, but can be constructed from frames
  // in multiple processes. In that case, which process should parse it and how
  // should the output be spread back across multiple processes?
  navigation_info->archive_status =
      IsLoadedAsMHTMLArchive(local_parent_frame)
          ? WebNavigationInfo::ArchiveStatus::Present
          : WebNavigationInfo::ArchiveStatus::Absent;

  if (form)
    navigation_info->form = WebFormElement(form);

  if (origin_frame) {
    navigation_info->is_opener_navigation =
        origin_frame->Opener() == ToCoreFrame(web_frame_);
    navigation_info->initiator_frame_has_download_sandbox_flag =
        origin_window->IsSandboxed(
            network::mojom::blink::WebSandboxFlags::kDownloads);
    navigation_info->initiator_frame_is_ad = origin_frame->IsAdFrame();
    navigation_info->is_ad_script_in_stack = origin_frame->IsAdScriptInStack();
  }

  navigation_info->has_rel_opener = has_rel_opener;

  // The frame has navigated either by itself or by the action of the
  // |origin_window| when it is defined. |source_location| represents the
  // line of code that has initiated the navigation. It is used to let web
  // developers locate the root cause of blocked navigations.
  // If `origin_window` is defined, then `source_location` must be, too, since
  // it should have been captured when creating the `FrameLoadRequest`.
  // Otherwise, try to capture the `source_location` from the current frame.
  if (!source_location) {
    DCHECK(!origin_window);
    source_location =
        CaptureSourceLocation(web_frame_->GetFrame()->DomWindow());
  }
  if (!source_location->IsUnknown()) {
    navigation_info->source_location.url = source_location->Url();
    navigation_info->source_location.line_number =
        source_location->LineNumber();
    navigation_info->source_location.column_number =
        source_location->ColumnNumber();
  }

  std::unique_ptr<Vector<mojom::blink::OriginTrialFeature>>
      initiator_origin_trial_features =
          OriginTrialContext::GetEnabledNavigationFeatures(
              web_frame_->GetFrame()->DomWindow());
  if (initiator_origin_trial_features) {
    navigation_info->initiator_origin_trial_features.reserve(
        initiator_origin_trial_features->size());
    for (auto feature : *initiator_origin_trial_features) {
      // Convert from OriginTrialFeature to int. We convert to int here since
      // OriginTrialFeature is not visible (and is not needed) outside of
      // blink. These values are only passed outside of blink so they can be
      // forwarded to the next blink navigation, but aren't used outside of
      // blink other than to forward the values between navigations.
      navigation_info->initiator_origin_trial_features.emplace_back(
          static_cast<int>(feature));
    }
  }

  if (WebDevToolsAgentImpl* devtools =
          DevToolsAgent(/*create_if_necessary=*/false)) {
    navigation_info->devtools_initiator_info =
        devtools->NavigationInitiatorInfo(web_frame_->GetFrame());
  }

  auto* owner = ToCoreFrame(web_frame_)->Owner();
  navigation_info->frame_policy =
      owner ? owner->GetFramePolicy() : FramePolicy();

  // navigation_info->frame_policy is only used for the synchronous
  // re-navigation to about:blank. See:
  // - |RenderFrameImpl::SynchronouslyCommitAboutBlankForBug778318| and
  // - |WebNavigationParams::CreateFromInfo|
  //
  // |owner->GetFramePolicy()| above only contains the sandbox flags defined by
  // the <iframe> element. It doesn't take into account inheritance from the
  // parent or the opener. The synchronous re-navigation to about:blank and the
  // initial empty document must both have the same sandbox flags. Make a copy:
  navigation_info->frame_policy.sandbox_flags = web_frame_->GetFrame()
                                                    ->DomWindow()
                                                    ->GetSecurityContext()
                                                    .GetSandboxFlags();

  navigation_info->href_translate = href_translate;
  navigation_info->is_container_initiated = is_container_initiated;

  web_frame_->Client()->BeginNavigation(std::move(navigation_info));
}

void LocalFrameClientImpl::DispatchWillSendSubmitEvent(HTMLFormElement* form) {
  web_frame_->WillSendSubmitEvent(WebFormElement(form));
}

void LocalFrameClientImpl::DidStartLoading() {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidStartLoading();
  }
}

void LocalFrameClientImpl::DidStopLoading() {
  if (web_frame_->Client())
    web_frame_->Client()->DidStopLoading();
}

bool LocalFrameClientImpl::NavigateBackForward(
    int offset,
    std::optional<scheduler::TaskAttributionId>
        soft_navigation_heuristics_task_id) const {
  WebViewImpl* webview = web_frame_->ViewImpl();
  DCHECK(webview->Client());
  DCHECK(web_frame_->Client());

  DCHECK(offset);
  if (offset > webview->HistoryForwardListCount())
    return false;
  if (offset < -webview->HistoryBackListCount())
    return false;

  bool has_user_gesture =
      LocalFrame::HasTransientUserActivation(web_frame_->GetFrame());
  web_frame_->GetFrame()->GetLocalFrameHostRemote().GoToEntryAtOffset(
      offset, has_user_gesture, soft_navigation_heuristics_task_id);
  return true;
}

void LocalFrameClientImpl::DidDispatchPingLoader(const KURL& url) {
  if (web_frame_->Client())
    web_frame_->Client()->DidDispatchPingLoader(url);
}

void LocalFrameClientImpl::DidChangePerformanceTiming() {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangePerformanceTiming();
}

void LocalFrameClientImpl::DidObserveUserInteraction(
    base::TimeTicks max_event_start,
    base::TimeTicks max_event_queued_main_thread,
    base::TimeTicks max_event_commit_finish,
    base::TimeTicks max_event_end,
    UserInteractionType interaction_type,
    uint64_t interaction_offset) {
  web_frame_->Client()->DidObserveUserInteraction(
      max_event_start, max_event_queued_main_thread, max_event_commit_finish,
      max_event_end, interaction_type, interaction_offset);
}

void LocalFrameClientImpl::DidChangeCpuTiming(base::TimeDelta time) {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeCpuTiming(time);
}

void LocalFrameClientImpl::DidObserveLoadingBehavior(
    LoadingBehaviorFlag behavior) {
  if (web_frame_->Client())
    web_frame_->Client()->DidObserveLoadingBehavior(behavior);
}

void LocalFrameClientImpl::DidObserveJavaScriptFrameworks(
    const JavaScriptFrameworkDetectionResult& result) {
  web_frame_->Client()->DidObserveJavaScriptFrameworks(result);
}

void LocalFrameClientImpl::DidObserveSubresourceLoad(
    const SubresourceLoadMetrics& subresource_load_metrics) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidObserveSubresourceLoad(subresource_load_metrics);
  }
}

void LocalFrameClientImpl::DidObserveNewFeatureUsage(
    const UseCounterFeature& feature) {
  if (web_frame_->Client())
    web_frame_->Client()->DidObserveNewFeatureUsage(feature);
}

// A new soft navigation was observed.
void LocalFrameClientImpl::DidObserveSoftNavigation(
    SoftNavigationMetrics metrics) {
  if (WebLocalFrameClient* client = web_frame_->Client()) {
    client->DidObserveSoftNavigation(metrics);
  }
}

void LocalFrameClientImpl::DidObserveLayoutShift(double score,
                                                 bool after_input_or_scroll) {
  if (WebLocalFrameClient* client = web_frame_->Client())
    client->DidObserveLayoutShift(score, after_input_or_scroll);
}

void LocalFrameClientImpl::SelectorMatchChanged(
    const Vector<String>& added_selectors,
    const Vector<String>& removed_selectors) {
  if (WebLocalFrameClient* client = web_frame_->Client()) {
    client->DidMatchCSS(WebVector<WebString>(added_selectors),
                        WebVector<WebString>(removed_selectors));
  }
}

void LocalFrameClientImpl::DidCreateDocumentLoader(
    DocumentLoader* document_loader) {
  web_frame_->Client()->DidCreateDocumentLoader(document_loader);
}

String LocalFrameClientImpl::UserAgentOverride() {
  return web_frame_->Client()
             ? String(web_frame_->Client()->UserAgentOverride())
             : g_empty_string;
}

String LocalFrameClientImpl::UserAgent() {
  String override = UserAgentOverride();
  if (!override.empty()) {
    return override;
  }

  if (user_agent_.empty())
    user_agent_ = Platform::Current()->UserAgent();
  return user_agent_;
}

std::optional<UserAgentMetadata> LocalFrameClientImpl::UserAgentMetadata() {
  bool ua_override_on = web_frame_->Client() &&
                        !web_frame_->Client()->UserAgentOverride().IsEmpty();
  std::optional<blink::UserAgentMetadata> user_agent_metadata =
      ua_override_on ? web_frame_->Client()->UserAgentMetadataOverride()
                     : Platform::Current()->UserAgentMetadata();

  Document* document = web_frame_->GetDocument();
  probe::ApplyUserAgentMetadataOverride(probe::ToCoreProbeSink(document),
                                        &user_agent_metadata);

  return user_agent_metadata;
}

String LocalFrameClientImpl::DoNotTrackValue() {
  if (web_frame_->View()->GetRendererPreferences().enable_do_not_track)
    return "1";
  return String();
}

// Called when the FrameLoader goes into a state in which a new page load
// will occur.
void LocalFrameClientImpl::TransitionToCommittedForNewPage() {
  web_frame_->CreateFrameView();
}

LocalFrame* LocalFrameClientImpl::CreateFrame(
    const AtomicString& name,
    HTMLFrameOwnerElement* owner_element) {
  return web_frame_->CreateChildFrame(name, owner_element);
}

RemoteFrame* LocalFrameClientImpl::CreateFencedFrame(
    HTMLFencedFrameElement* fenced_frame,
    mojo::PendingAssociatedReceiver<mojom::blink::FencedFrameOwnerHost>
        receiver) {
  return web_frame_->CreateFencedFrame(fenced_frame, std::move(receiver));
}

WebPluginContainerImpl* LocalFrameClientImpl::CreatePlugin(
    HTMLPlugInElement& element,
    const KURL& url,
    const Vector<String>& param_names,
    const Vector<String>& param_values,
    const String& mime_type,
    bool load_manually) {
  if (!web_frame_->Client())
    return nullptr;

  WebPluginParams params;
  params.url = url;
  params.mime_type = mime_type;
  params.attribute_names = param_names;
  params.attribute_values = param_values;
  params.load_manually = load_manually;

  WebPlugin* web_plugin = web_frame_->Client()->CreatePlugin(params);
  if (!web_plugin)
    return nullptr;

  // The container takes ownership of the WebPlugin.
  auto* container =
      MakeGarbageCollected<WebPluginContainerImpl>(element, web_plugin);

  if (!web_plugin->Initialize(container))
    return nullptr;

  if (!element.GetLayoutObject())
    return nullptr;

  return container;
}

std::unique_ptr<WebMediaPlayer> LocalFrameClientImpl::CreateWebMediaPlayer(
    HTMLMediaElement& html_media_element,
    const WebMediaPlayerSource& source,
    WebMediaPlayerClient* client) {
  LocalFrame* local_frame = html_media_element.LocalFrameForPlayer();
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(local_frame);

  if (!web_frame || !web_frame->Client())
    return nullptr;

  return CoreInitializer::GetInstance().CreateWebMediaPlayer(
      web_frame->Client(), html_media_element, source, client);
}

WebRemotePlaybackClient* LocalFrameClientImpl::CreateWebRemotePlaybackClient(
    HTMLMediaElement& html_media_element) {
  return CoreInitializer::GetInstance().CreateWebRemotePlaybackClient(
      html_media_element);
}

void LocalFrameClientImpl::DidChangeName(const String& name) {
  if (!web_frame_->Client())
    return;
  web_frame_->Client()->DidChangeName(name);
}

std::unique_ptr<WebServiceWorkerProvider>
LocalFrameClientImpl::CreateServiceWorkerProvider() {
  if (!web_frame_->Client())
    return nullptr;
  return web_frame_->Client()->CreateServiceWorkerProvider();
}

WebContentSettingsClient* LocalFrameClientImpl::GetContentSettingsClient() {
  return web_frame_->GetContentSettingsClient();
}

void LocalFrameClientImpl::DispatchDidChangeManifest() {
  CoreInitializer::GetInstance().DidChangeManifest(*web_frame_->GetFrame());
}

unsigned LocalFrameClientImpl::BackForwardLength() {
  WebViewImpl* webview = web_frame_->ViewImpl();
  return webview ? webview->HistoryListLength() : 0;
}

WebDevToolsAgentImpl* LocalFrameClientImpl::DevToolsAgent(
    bool create_if_necessary) {
  return WebLocalFrameImpl::FromFrame(web_frame_->GetFrame()->LocalFrameRoot())
      ->DevToolsAgentImpl(create_if_necessary);
}

KURL LocalFrameClientImpl::OverrideFlashEmbedWithHTML(const KURL& url) {
  return web_frame_->Client()->OverrideFlashEmbedWithHTML(WebURL(url));
}

void LocalFrameClientImpl::NotifyUserActivation() {
  if (WebAutofillClient* autofill_client = web_frame_->AutofillClient())
    autofill_client->UserGestureObserved();
}

void LocalFrameClientImpl::AbortClientNavigation(bool for_new_navigation) {
  if (web_frame_->Client()) {
    web_frame_->Client()->AbortClientNavigation(for_new_navigation);
  }
}

WebSpellCheckPanelHostClient* LocalFrameClientImpl::SpellCheckPanelHostClient()
    const {
  return web_frame_->SpellCheckPanelHostClient();
}

WebTextCheckClient* LocalFrameClientImpl::GetTextCheckerClient() const {
  return web_frame_->GetTextCheckerClient();
}

scoped_refptr<network::SharedURLLoaderFactory>
LocalFrameClientImpl::GetURLLoaderFactory() {
  return web_frame_->Client()->GetURLLoaderFactory();
}

std::unique_ptr<URLLoader> LocalFrameClientImpl::CreateURLLoaderForTesting() {
  return web_frame_->Client()->CreateURLLoaderForTesting();
}

blink::ChildURLLoaderFactoryBundle*
LocalFrameClientImpl::GetLoaderFactoryBundle() {
  return web_frame_->Client()->GetLoaderFactoryBundle();
}

scoped_refptr<WebBackgroundResourceFetchAssets>
LocalFrameClientImpl::MaybeGetBackgroundResourceFetchAssets() {
  return web_frame_->Client()->MaybeGetBackgroundResourceFetchAssets();
}

AssociatedInterfaceProvider*
LocalFrameClientImpl::GetRemoteNavigationAssociatedInterfaces() {
  return web_frame_->Client()->GetRemoteNavigationAssociatedInterfaces();
}

base::UnguessableToken LocalFrameClientImpl::GetDevToolsFrameToken() const {
  return web_frame_->Client()->GetDevToolsFrameToken();
}

String LocalFrameClientImpl::evaluateInInspectorOverlayForTesting(
    const String& script) {
  if (WebDevToolsAgentImpl* devtools =
          DevToolsAgent(/*create_if_necessary=*/true)) {
    return devtools->EvaluateInOverlayForTesting(script);
  }
  return g_empty_string;
}

bool LocalFrameClientImpl::HandleCurrentKeyboardEvent() {
  return web_frame_->LocalRoot()
      ->FrameWidgetImpl()
      ->HandleCurrentKeyboardEvent();
}

void LocalFrameClientImpl::DidChangeSelection(bool is_selection_empty,
                                              blink::SyncCondition force_sync) {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeSelection(is_selection_empty, force_sync);
}

void LocalFrameClientImpl::DidChangeContents() {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeContents();
}

Frame* LocalFrameClientImpl::FindFrame(const AtomicString& name) const {
  DCHECK(web_frame_->Client());
  return ToCoreFrame(web_frame_->Client()->FindFrame(name));
}

void LocalFrameClientImpl::FocusedElementChanged(Element* element) {
  DCHECK(web_frame_->Client());
  web_frame_->ResetHasScrolledFocusedEditableIntoView();
  web_frame_->Client()->FocusedElementChanged(element);
}

void LocalFrameClientImpl::OnMainFrameIntersectionChanged(
    const gfx::Rect& main_frame_intersection_rect) {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->OnMainFrameIntersectionChanged(
      main_frame_intersection_rect);
}

void LocalFrameClientImpl::OnMainFrameViewportRectangleChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->OnMainFrameViewportRectangleChanged(
      main_frame_viewport_rect);
}

void LocalFrameClientImpl::OnMainFrameImageAdRectangleChanged(
    DOMNodeId element_id,
    const gfx::Rect& image_ad_rect) {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->OnMainFrameImageAdRectangleChanged(element_id,
                                                           image_ad_rect);
}

void LocalFrameClientImpl::OnOverlayPopupAdDetected() {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->OnOverlayPopupAdDetected();
}

void LocalFrameClientImpl::OnLargeStickyAdDetected() {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->OnLargeStickyAdDetected();
}

bool LocalFrameClientImpl::IsPluginHandledExternally(
    HTMLPlugInElement& plugin_element,
    const KURL& resource_url,
    const String& suggesed_mime_type) {
  return web_frame_->Client()->IsPluginHandledExternally(
      &plugin_element, resource_url, suggesed_mime_type);
}

v8::Local<v8::Object> LocalFrameClientImpl::GetScriptableObject(
    HTMLPlugInElement& plugin_element,
    v8::Isolate* isolate) {
  return web_frame_->Client()->GetScriptableObject(&plugin_element, isolate);
}

scoped_refptr<WebWorkerFetchContext>
LocalFrameClientImpl::CreateWorkerFetchContext() {
  DCHECK(web_frame_->Client());
  return web_frame_->Client()->CreateWorkerFetchContext();
}

scoped_refptr<WebWorkerFetchContext>
LocalFrameClientImpl::CreateWorkerFetchContextForPlzDedicatedWorker(
    WebDedicatedWorkerHostFactoryClient* factory_client) {
  DCHECK(web_frame_->Client());
  return web_frame_->Client()->CreateWorkerFetchContextForPlzDedicatedWorker(
      factory_client);
}

std::unique_ptr<WebContentSettingsClient>
LocalFrameClientImpl::CreateWorkerContentSettingsClient() {
  DCHECK(web_frame_->Client());
  return web_frame_->Client()->CreateWorkerContentSettingsClient();
}

void LocalFrameClientImpl::SetMouseCapture(bool capture) {
  web_frame_->LocalRoot()->FrameWidgetImpl()->SetMouseCapture(capture);
}

void LocalFrameClientImpl::NotifyAutoscrollForSelectionInMainFrame(
    bool autoscroll_selection) {
  web_frame_->LocalRoot()
      ->FrameWidgetImpl()
      ->NotifyAutoscrollForSelectionInMainFrame(autoscroll_selection);
}

std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
LocalFrameClientImpl::CreateResourceLoadInfoNotifierWrapper() {
  DCHECK(web_frame_->Client());
  return web_frame_->Client()->CreateResourceLoadInfoNotifierWrapper();
}

void LocalFrameClientImpl::BindDevToolsAgent(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsAgentHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsAgent> receiver) {
  if (WebDevToolsAgentImpl* devtools =
          DevToolsAgent(/*create_if_necessary=*/true)) {
    devtools->BindReceiver(std::move(host), std::move(receiver));
  }
}

bool LocalFrameClientImpl::IsDomStorageDisabled() const {
  return web_frame_->Client()->IsDomStorageDisabled();
}

}  // namespace blink
