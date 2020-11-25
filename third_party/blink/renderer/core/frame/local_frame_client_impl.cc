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

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom-blink-forward.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_client.h"
#include "third_party/blink/public/platform/platform.h"
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
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_document_loader_impl.h"
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
         IsBackForwardLoadType(
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

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebContentSecurityPolicySourceExpression ConvertToPublic(
    network::mojom::blink::CSPSourcePtr source) {
  return {source->scheme,
          source->host,
          source->is_host_wildcard ? kWebWildcardDispositionHasWildcard
                                   : kWebWildcardDispositionNoWildcard,
          source->port,
          source->is_port_wildcard ? kWebWildcardDispositionHasWildcard
                                   : kWebWildcardDispositionNoWildcard,
          source->path};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebContentSecurityPolicySourceList ConvertToPublic(
    network::mojom::blink::CSPSourceListPtr source_list) {
  WebVector<WebContentSecurityPolicySourceExpression> sources(
      source_list->sources.size());
  for (size_t i = 0; i < sources.size(); ++i)
    sources[i] = ConvertToPublic(std::move(source_list->sources[i]));
  return {source_list->allow_self, source_list->allow_star,
          source_list->allow_response_redirects, std::move(sources)};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebString ConvertToPublic(
    network::mojom::blink::CSPDirectiveName directive_name) {
  using CSPDirectiveName = network::mojom::blink::CSPDirectiveName;
  switch (directive_name) {
    case CSPDirectiveName::BaseURI:
      return "base-uri";
    case CSPDirectiveName::ChildSrc:
      return "child-src";
    case CSPDirectiveName::ConnectSrc:
      return "connect-src";
    case CSPDirectiveName::DefaultSrc:
      return "default-src";
    case CSPDirectiveName::FrameAncestors:
      return "frame-ancestors";
    case CSPDirectiveName::FrameSrc:
      return "frame-src";
    case CSPDirectiveName::FontSrc:
      return "font-src";
    case CSPDirectiveName::FormAction:
      return "form-action";
    case CSPDirectiveName::ImgSrc:
      return "img-src";
    case CSPDirectiveName::ManifestSrc:
      return "manifest-src";
    case CSPDirectiveName::MediaSrc:
      return "media-src";
    case CSPDirectiveName::ObjectSrc:
      return "object-src";
    case CSPDirectiveName::PrefetchSrc:
      return "prefetch-src";
    case CSPDirectiveName::ReportURI:
      return "report-uri";
    case CSPDirectiveName::Sandbox:
      return "sandbox";
    case CSPDirectiveName::ScriptSrc:
      return "script-src";
    case CSPDirectiveName::ScriptSrcAttr:
      return "script-src-attr";
    case CSPDirectiveName::ScriptSrcElem:
      return "script-src-elem";
    case CSPDirectiveName::StyleSrc:
      return "style-src";
    case CSPDirectiveName::StyleSrcAttr:
      return "style-src-attr";
    case CSPDirectiveName::StyleSrcElem:
      return "style-src-elem";
    case CSPDirectiveName::UpgradeInsecureRequests:
      return "upgrade-insecure-requests";
    case CSPDirectiveName::TreatAsPublicAddress:
      return "treat-as-public-address";
    case CSPDirectiveName::WorkerSrc:
      return "worker-src";
    case CSPDirectiveName::ReportTo:
      return "report-to";
    case CSPDirectiveName::NavigateTo:
      return "navigate-to";
    case CSPDirectiveName::Unknown:
      NOTREACHED();
      return "";
    default:
      NOTREACHED();
      return "";
  };
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
base::Optional<WebCSPTrustedTypes> ConvertToPublic(
    network::mojom::blink::CSPTrustedTypesPtr trusted_types) {
  if (!trusted_types)
    return base::nullopt;
  return WebCSPTrustedTypes{std::move(trusted_types->list),
                            trusted_types->allow_any,
                            trusted_types->allow_duplicates};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebContentSecurityPolicy ConvertToPublic(
    network::mojom::blink::ContentSecurityPolicyPtr policy) {
  WebVector<WebContentSecurityPolicyDirective> directives(
      policy->directives.size());
  size_t i = 0;
  for (auto& directive : policy->directives) {
    directives[i++] = {ConvertToPublic(directive.key),
                       ConvertToPublic(std::move(directive.value))};
  }

  return {policy->header->type,
          policy->header->source,
          std::move(directives),
          policy->upgrade_insecure_requests,
          policy->block_all_mixed_content,
          std::move(policy->report_endpoints),
          policy->header->header_value,
          policy->use_reporting_api,
          policy->require_trusted_types_for,
          ConvertToPublic(std::move(policy->trusted_types))};
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

void LocalFrameClientImpl::DidCreateInitialEmptyDocument() {
  if (web_frame_->Client())
    web_frame_->Client()->DidCreateInitialEmptyDocument();
}

void LocalFrameClientImpl::DidCommitDocumentReplacementNavigation(
    DocumentLoader* loader) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidCommitDocumentReplacementNavigation(
        WebDocumentLoaderImpl::FromDocumentLoader(loader));
  }
}

void LocalFrameClientImpl::DispatchDidClearWindowObjectInMainWorld() {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidClearWindowObject();
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
    if (mode == 'v0') {
      shadowRoot = parent.createShadowRoot();
    } else if (mode == 'open' || mode == 'closed') {
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
    ClassicScript::CreateUnspecifiedScript(
        ScriptSourceCode(script, ScriptSourceLocationType::kInternal))
        ->RunScript(web_frame_->GetFrame()->DomWindow(),
                    ScriptController::kExecuteScriptWhenScriptsDisabled);
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

void LocalFrameClientImpl::DidUpdateCurrentHistoryItem() {
  if (web_frame_->Client())
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
  return web_frame_->InShadowTree();
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

  client->WillDetach();

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

void LocalFrameClientImpl::DispatchWillSendRequest(ResourceRequest& request) {
  // Give the WebLocalFrameClient a crack at the request.
  if (web_frame_->Client()) {
    WrappedResourceRequest webreq(request);
    web_frame_->Client()->WillSendRequest(
        webreq, WebLocalFrameClient::ForRedirect(
                    request.GetRedirectInfo().has_value()));
  }
}

void LocalFrameClientImpl::DispatchDidFinishDocumentLoad() {
  // TODO(dglazkov): Sadly, workers are WebLocalFrameClients, and they can
  // totally destroy themselves when didFinishDocumentLoad is invoked, and in
  // turn destroy the fake WebLocalFrame that they create, which means that you
  // should not put any code touching `this` after the two lines below.
  if (web_frame_->Client())
    web_frame_->Client()->DidFinishDocumentLoad();
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
    HistoryItem* item,
    WebHistoryCommitType commit_type,
    bool content_initiated) {
  bool should_create_history_entry = commit_type == kWebStandardCommit;
  // TODO(dglazkov): Does this need to be called for subframes?
  web_frame_->ViewImpl()->DidCommitLoad(should_create_history_entry, true);
  if (web_frame_->Client()) {
    web_frame_->Client()->DidFinishSameDocumentNavigation(commit_type,
                                                          content_initiated);
  }
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
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::ParsedFeaturePolicy& feature_policy_header,
    const blink::DocumentPolicyFeatureState& document_policy_header) {
  if (!web_frame_->Parent()) {
    web_frame_->ViewImpl()->DidCommitLoad(commit_type == kWebStandardCommit,
                                          false);
  }

  if (web_frame_->Client()) {
    web_frame_->Client()->DidCommitNavigation(
        commit_type, should_reset_browser_interface_broker, sandbox_flags,
        feature_policy_header, document_policy_header);
    if (web_frame_->GetFrame()->IsLocalRoot()) {
      // This update should be sent as soon as loading the new document begins
      // so that the browser and compositor could reset their states. However,
      // up to this point |web_frame_| is still provisional and the updates will
      // not get sent. Revise this when https://crbug.com/578349 is fixed.
      ResetWheelAndTouchEventHandlerProperties(*web_frame_->GetFrame());

      web_frame_->FrameWidgetImpl()->DidNavigate();

      // UKM metrics are only collected for the main frame. Ensure after
      // a navigation on the main frame we setup the appropriate structures.
      if (web_frame_->GetFrame()->IsMainFrame() &&
          web_frame_->ViewImpl()->does_composite()) {
        cc::LayerTreeHost* layer_tree_host =
            web_frame_->FrameWidgetImpl()->LayerTreeHost();

        // Update the URL and the document source id used to key UKM metrics in
        // the compositor. Note that the metrics for all frames are keyed to the
        // main frame's URL.
        layer_tree_host->SetSourceURL(
            web_frame_->GetDocument().GetUkmSourceId(),
            KURL(web_frame_->Client()->LastCommittedUrlForUKM()));

        auto shmem = layer_tree_host->CreateSharedMemoryForSmoothnessUkm();
        if (shmem.IsValid()) {
          web_frame_->Client()->SetUpSharedMemoryForSmoothness(
              std::move(shmem));
        }
      }
    }
  }
  if (WebDevToolsAgentImpl* dev_tools = DevToolsAgent())
    dev_tools->DidCommitLoadForLocalFrame(web_frame_->GetFrame());
}

void LocalFrameClientImpl::DispatchDidFailLoad(
    const ResourceError& error,
    WebHistoryCommitType commit_type) {
  web_frame_->DidFailLoad(error, commit_type);
}

void LocalFrameClientImpl::DispatchDidFinishLoad() {
  web_frame_->DidFinish();
}

void LocalFrameClientImpl::BeginNavigation(
    const ResourceRequest& request,
    mojom::RequestContextFrameType frame_type,
    LocalDOMWindow* origin_window,
    DocumentLoader* document_loader,
    WebNavigationType type,
    NavigationPolicy policy,
    WebFrameLoadType frame_load_type,
    bool is_client_redirect,
    TriggeringEventInfo triggering_event_info,
    HTMLFormElement* form,
    network::mojom::CSPDisposition
        should_check_main_world_content_security_policy,
    mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token,
    base::TimeTicks input_start_time,
    const String& href_translate,
    const base::Optional<WebImpression>& impression,
    WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr> initiator_csp,
    network::mojom::blink::CSPSourcePtr initiator_self_source,
    network::mojom::IPAddressSpace initiator_address_space,
    mojo::PendingRemote<mojom::blink::NavigationInitiator>
        navigation_initiator) {
  if (!web_frame_->Client())
    return;

  auto navigation_info = std::make_unique<WebNavigationInfo>();
  navigation_info->url_request.CopyFrom(WrappedResourceRequest(request));
  navigation_info->frame_type = frame_type;
  navigation_info->navigation_type = type;
  navigation_info->navigation_policy = static_cast<WebNavigationPolicy>(policy);
  navigation_info->has_transient_user_activation = request.HasUserGesture();
  navigation_info->frame_load_type = frame_load_type;
  navigation_info->is_client_redirect = is_client_redirect;
  navigation_info->triggering_event_info = triggering_event_info;
  navigation_info->should_check_main_world_content_security_policy =
      should_check_main_world_content_security_policy;
  navigation_info->blob_url_token = std::move(blob_url_token);
  navigation_info->input_start = input_start_time;
  if (origin_window && origin_window->GetFrame()) {
    navigation_info->initiator_frame =
        origin_window->GetFrame()->Client()->GetWebFrame();
  } else {
    navigation_info->initiator_frame = nullptr;
  }
  for (auto& csp_policy : initiator_csp) {
    navigation_info->initiator_csp.emplace_back(
        ConvertToPublic(std::move(csp_policy)));
  }
  if (initiator_self_source) {
    navigation_info->initiator_self_source =
        ConvertToPublic(std::move(initiator_self_source));
  }
  navigation_info->initiator_address_space = initiator_address_space;
  navigation_info->navigation_initiator_remote =
      std::move(navigation_initiator);

  navigation_info->impression = impression;

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

  LocalFrame* frame = origin_window ? origin_window->GetFrame() : nullptr;
  if (frame) {
    navigation_info->is_opener_navigation =
        frame->Opener() == ToCoreFrame(web_frame_);
    navigation_info->initiator_frame_has_download_sandbox_flag =
        origin_window->IsSandboxed(
            network::mojom::blink::WebSandboxFlags::kDownloads);
    navigation_info->initiator_frame_is_ad = frame->IsAdSubframe();
  }

  navigation_info->blocking_downloads_in_sandbox_enabled =
      RuntimeEnabledFeatures::BlockingDownloadsInSandboxEnabled();

  // The frame has navigated either by itself or by the action of the
  // |origin_window| when it is defined. |source_location| represents the
  // line of code that has initiated the navigation. It is used to let web
  // developpers locate the root cause of blocked navigations.
  // TODO(crbug.com/804504): This is likely wrong -- this is often invoked
  // asynchronously as a result of ScheduledURLNavigation::Fire(), so JS
  // stack is not available here.
  std::unique_ptr<SourceLocation> source_location =
      origin_window
          ? SourceLocation::Capture(origin_window)
          : SourceLocation::Capture(web_frame_->GetFrame()->DomWindow());
  if (source_location && !source_location->IsUnknown()) {
    navigation_info->source_location.url = source_location->Url();
    navigation_info->source_location.line_number =
        source_location->LineNumber();
    navigation_info->source_location.column_number =
        source_location->ColumnNumber();
  }

  std::unique_ptr<Vector<OriginTrialFeature>> initiator_origin_trial_features =
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

  if (WebDevToolsAgentImpl* devtools = DevToolsAgent()) {
    navigation_info->devtools_initiator_info =
        devtools->NavigationInitiatorInfo(web_frame_->GetFrame());
  }

  auto* owner = ToCoreFrame(web_frame_)->Owner();
  navigation_info->frame_policy =
      owner ? owner->GetFramePolicy() : FramePolicy();

  navigation_info->href_translate = href_translate;

  web_frame_->Client()->BeginNavigation(std::move(navigation_info));
}

void LocalFrameClientImpl::DispatchWillSendSubmitEvent(HTMLFormElement* form) {
  if (web_frame_->Client())
    web_frame_->Client()->WillSendSubmitEvent(WebFormElement(form));
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

bool LocalFrameClientImpl::NavigateBackForward(int offset) const {
  WebViewImpl* webview = web_frame_->ViewImpl();
  DCHECK(webview->Client());
  DCHECK(web_frame_->Client());

  DCHECK(offset);
  if (offset > webview->Client()->HistoryForwardListCount())
    return false;
  if (offset < -webview->Client()->HistoryBackListCount())
    return false;

  bool has_user_gesture =
      LocalFrame::HasTransientUserActivation(web_frame_->GetFrame());
  web_frame_->GetFrame()->GetLocalFrameHostRemote().GoToEntryAtOffset(
      offset, has_user_gesture);
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

void LocalFrameClientImpl::DidObserveInputDelay(base::TimeDelta input_delay) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidObserveInputDelay(input_delay);
  }
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

void LocalFrameClientImpl::DidObserveNewFeatureUsage(
    mojom::WebFeature feature) {
  if (web_frame_->Client())
    web_frame_->Client()->DidObserveNewFeatureUsage(feature);
}

void LocalFrameClientImpl::DidObserveNewCssPropertyUsage(
    mojom::CSSSampleId css_property,
    bool is_animated) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidObserveNewCssPropertyUsage(css_property,
                                                        is_animated);
  }
}

void LocalFrameClientImpl::DidObserveLayoutShift(double score,
                                                 bool after_input_or_scroll) {
  if (WebLocalFrameClient* client = web_frame_->Client())
    client->DidObserveLayoutShift(score, after_input_or_scroll);
}

void LocalFrameClientImpl::DidObserveLayoutNg(uint32_t all_block_count,
                                              uint32_t ng_block_count,
                                              uint32_t all_call_count,
                                              uint32_t ng_call_count) {
  if (WebLocalFrameClient* client = web_frame_->Client()) {
    client->DidObserveLayoutNg(all_block_count, ng_block_count, all_call_count,
                               ng_call_count);
  }
}

void LocalFrameClientImpl::DidObserveLazyLoadBehavior(
    WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior) {
  if (WebLocalFrameClient* client = web_frame_->Client())
    client->DidObserveLazyLoadBehavior(lazy_load_behavior);
}

void LocalFrameClientImpl::SelectorMatchChanged(
    const Vector<String>& added_selectors,
    const Vector<String>& removed_selectors) {
  if (WebLocalFrameClient* client = web_frame_->Client()) {
    client->DidMatchCSS(WebVector<WebString>(added_selectors),
                        WebVector<WebString>(removed_selectors));
  }
}

DocumentLoader* LocalFrameClientImpl::CreateDocumentLoader(
    LocalFrame* frame,
    WebNavigationType navigation_type,
    ContentSecurityPolicy* content_security_policy,
    std::unique_ptr<WebNavigationParams> navigation_params,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  DCHECK(frame);
  WebDocumentLoaderImpl* document_loader =
      MakeGarbageCollected<WebDocumentLoaderImpl>(frame, navigation_type,
                                                  content_security_policy,
                                                  std::move(navigation_params));
  document_loader->SetExtraData(std::move(extra_data));
  if (web_frame_->Client())
    web_frame_->Client()->DidCreateDocumentLoader(document_loader);
  return document_loader;
}

void LocalFrameClientImpl::UpdateDocumentLoader(
    DocumentLoader* document_loader,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  static_cast<WebDocumentLoaderImpl*>(document_loader)
      ->SetExtraData(std::move(extra_data));
}

String LocalFrameClientImpl::UserAgent() {
  WebString override =
      web_frame_->Client() ? web_frame_->Client()->UserAgentOverride() : "";
  if (!override.IsEmpty())
    return override;

  if (user_agent_.IsEmpty())
    user_agent_ = Platform::Current()->UserAgent();
  return user_agent_;
}

base::Optional<UserAgentMetadata> LocalFrameClientImpl::UserAgentMetadata() {
  bool ua_override_on = web_frame_->Client() &&
                        !web_frame_->Client()->UserAgentOverride().IsEmpty();
  base::Optional<blink::UserAgentMetadata> user_agent_metadata =
      ua_override_on ? web_frame_->Client()->UserAgentMetadataOverride()
                     : Platform::Current()->UserAgentMetadata();

  Document* document = web_frame_->GetDocument();
  probe::ApplyUserAgentMetadataOverride(probe::ToCoreProbeSink(document),
                                        &user_agent_metadata);

  return user_agent_metadata;
}

String LocalFrameClientImpl::DoNotTrackValue() {
  WebString do_not_track = web_frame_->Client()->DoNotTrackValue();
  if (!do_not_track.IsEmpty())
    return do_not_track;
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

std::pair<RemoteFrame*, PortalToken> LocalFrameClientImpl::CreatePortal(
    HTMLPortalElement* portal,
    mojo::PendingAssociatedReceiver<mojom::blink::Portal> portal_receiver,
    mojo::PendingAssociatedRemote<mojom::blink::PortalClient> portal_client) {
  return web_frame_->CreatePortal(portal, std::move(portal_receiver),
                                  std::move(portal_client));
}

RemoteFrame* LocalFrameClientImpl::AdoptPortal(HTMLPortalElement* portal) {
  return web_frame_->AdoptPortal(portal);
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
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(html_media_element.GetDocument().GetFrame());

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
  if (!webview || !webview->Client())
    return 0;
  return webview->Client()->HistoryBackListCount() + 1 +
         webview->Client()->HistoryForwardListCount();
}

BlameContext* LocalFrameClientImpl::GetFrameBlameContext() {
  if (WebLocalFrameClient* client = web_frame_->Client())
    return client->GetFrameBlameContext();
  return nullptr;
}

WebDevToolsAgentImpl* LocalFrameClientImpl::DevToolsAgent() {
  return WebLocalFrameImpl::FromFrame(web_frame_->GetFrame()->LocalFrameRoot())
      ->DevToolsAgentImpl();
}

KURL LocalFrameClientImpl::OverrideFlashEmbedWithHTML(const KURL& url) {
  return web_frame_->Client()->OverrideFlashEmbedWithHTML(WebURL(url));
}

void LocalFrameClientImpl::NotifyUserActivation() {
  if (WebAutofillClient* autofill_client = web_frame_->AutofillClient())
    autofill_client->UserGestureObserved();
}

void LocalFrameClientImpl::AbortClientNavigation() {
  if (web_frame_->Client())
    web_frame_->Client()->AbortClientNavigation();
}

WebSpellCheckPanelHostClient* LocalFrameClientImpl::SpellCheckPanelHostClient()
    const {
  return web_frame_->SpellCheckPanelHostClient();
}

WebTextCheckClient* LocalFrameClientImpl::GetTextCheckerClient() const {
  return web_frame_->GetTextCheckerClient();
}

std::unique_ptr<blink::WebURLLoaderFactory>
LocalFrameClientImpl::CreateURLLoaderFactory() {
  return web_frame_->Client()->CreateURLLoaderFactory();
}

blink::BrowserInterfaceBrokerProxy&
LocalFrameClientImpl::GetBrowserInterfaceBroker() {
  return *web_frame_->Client()->GetBrowserInterfaceBroker();
}

AssociatedInterfaceProvider*
LocalFrameClientImpl::GetRemoteNavigationAssociatedInterfaces() {
  return web_frame_->Client()->GetRemoteNavigationAssociatedInterfaces();
}

void LocalFrameClientImpl::AnnotatedRegionsChanged() {
  web_frame_->Client()->DraggableRegionsChanged();
}

base::UnguessableToken LocalFrameClientImpl::GetDevToolsFrameToken() const {
  return web_frame_->Client()->GetDevToolsFrameToken();
}

String LocalFrameClientImpl::evaluateInInspectorOverlayForTesting(
    const String& script) {
  if (WebDevToolsAgentImpl* devtools = DevToolsAgent())
    return devtools->EvaluateInOverlayForTesting(script);
  return g_empty_string;
}

bool LocalFrameClientImpl::HandleCurrentKeyboardEvent() {
  return web_frame_->LocalRoot()
      ->FrameWidgetImpl()
      ->HandleCurrentKeyboardEvent();
}

void LocalFrameClientImpl::DidChangeSelection(bool is_selection_empty) {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeSelection(is_selection_empty);
}

void LocalFrameClientImpl::DidChangeContents() {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeContents();
}

Frame* LocalFrameClientImpl::FindFrame(const AtomicString& name) const {
  DCHECK(web_frame_->Client());
  return ToCoreFrame(web_frame_->Client()->FindFrame(name));
}

void LocalFrameClientImpl::FrameRectsChanged(const IntRect& frame_rect) {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->FrameRectsChanged(frame_rect);
}

void LocalFrameClientImpl::FocusedElementChanged(Element* element) {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->FocusedElementChanged(element);
}

void LocalFrameClientImpl::OnMainFrameIntersectionChanged(
    const IntRect& intersection_rect) {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->OnMainFrameIntersectionChanged(intersection_rect);
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

std::unique_ptr<media::SpeechRecognitionClient>
LocalFrameClientImpl::CreateSpeechRecognitionClient(
    media::SpeechRecognitionClient::OnReadyCallback callback) {
  DCHECK(web_frame_->Client());
  return web_frame_->Client()->CreateSpeechRecognitionClient(
      std::move(callback));
}

void LocalFrameClientImpl::SetMouseCapture(bool capture) {
  web_frame_->LocalRoot()->FrameWidgetImpl()->SetMouseCapture(capture);
}

bool LocalFrameClientImpl::UsePrintingLayout() const {
  return web_frame_->UsePrintingLayout();
}

std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
LocalFrameClientImpl::CreateResourceLoadInfoNotifierWrapper() {
  DCHECK(web_frame_->Client());
  return web_frame_->Client()->CreateResourceLoadInfoNotifierWrapper();
}

void LocalFrameClientImpl::UpdateSubresourceFactory(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle> pending_factory) {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->UpdateSubresourceFactory(std::move(pending_factory));
}

void LocalFrameClientImpl::DidChangeMobileFriendliness(
    const MobileFriendliness& mf) {
  DCHECK(web_frame_->Client());
  web_frame_->Client()->DidChangeMobileFriendliness(mf);
}

}  // namespace blink
