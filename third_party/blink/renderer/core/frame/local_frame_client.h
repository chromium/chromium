/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_CLIENT_H_

#include <memory>

#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink-forward.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink-forward.h"
#include "third_party/blink/public/common/feature_policy/document_policy_features.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/common/navigation/triggering_event_info.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom-blink-forward.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_manifest_manager.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/icon_url.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/html/link_resource.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {
namespace mojom {
enum class WebFeature : int32_t;
}  // namespace mojom

class AssociatedInterfaceProvider;
class ContentSecurityPolicy;
class DocumentLoader;
class HTMLFormElement;
class HTMLFrameOwnerElement;
class HTMLMediaElement;
class HTMLPortalElement;
class HTMLPlugInElement;
class HistoryItem;
class KURL;
class LocalDOMWindow;
class LocalFrame;
class WebPluginContainerImpl;
class RemoteFrame;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class WebContentCaptureClient;
class WebDedicatedWorkerHostFactoryClient;
class WebLocalFrame;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebMediaPlayerSource;
class WebRemotePlaybackClient;
class WebServiceWorkerProvider;
class WebSpellCheckPanelHostClient;
class WebTextCheckClient;
class ResourceLoadInfoNotifierWrapper;

class CORE_EXPORT LocalFrameClient : public FrameClient {
 public:
  ~LocalFrameClient() override = default;

  virtual WebContentCaptureClient* GetWebContentCaptureClient() const {
    return nullptr;
  }

  virtual WebLocalFrame* GetWebFrame() const { return nullptr; }

  virtual bool HasWebView() const = 0;  // mainly for assertions

  virtual void WillBeDetached() = 0;
  virtual void DispatchWillSendRequest(ResourceRequest&) = 0;
  virtual void DispatchDidLoadResourceFromMemoryCache(
      const ResourceRequest&,
      const ResourceResponse&) = 0;

  virtual void DispatchDidHandleOnloadEvents() = 0;
  virtual void DidFinishSameDocumentNavigation(HistoryItem*,
                                               WebHistoryCommitType,
                                               bool content_initiated) {}
  virtual void DispatchDidReceiveTitle(const String&) = 0;
  virtual void DispatchDidCommitLoad(
      HistoryItem* item,
      WebHistoryCommitType commit_type,
      bool should_reset_browser_interface_broker,
      network::mojom::WebSandboxFlags sandbox_flags,
      const blink::ParsedFeaturePolicy& feature_policy_header,
      const blink::DocumentPolicyFeatureState& document_policy_header) = 0;
  virtual void DispatchDidFailLoad(const ResourceError&,
                                   WebHistoryCommitType) = 0;
  virtual void DispatchDidFinishDocumentLoad() = 0;
  virtual void DispatchDidFinishLoad() = 0;

  virtual void BeginNavigation(
      const ResourceRequest&,
      mojom::RequestContextFrameType,
      LocalDOMWindow* origin_window,
      DocumentLoader*,
      WebNavigationType,
      NavigationPolicy,
      WebFrameLoadType,
      bool is_client_redirect,
      TriggeringEventInfo,
      HTMLFormElement*,
      network::mojom::CSPDisposition
          should_check_main_world_content_security_policy,
      mojo::PendingRemote<mojom::blink::BlobURLToken>,
      base::TimeTicks input_start_time,
      const String& href_translate,
      const base::Optional<WebImpression>& impression,
      WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr>
          initiator_csp,
      network::mojom::blink::CSPSourcePtr initiator_self_source,
      network::mojom::IPAddressSpace,
      mojo::PendingRemote<mojom::blink::NavigationInitiator>) = 0;

  virtual void DispatchWillSendSubmitEvent(HTMLFormElement*) = 0;

  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;

  virtual bool NavigateBackForward(int offset) const = 0;

  virtual void DidDispatchPingLoader(const KURL&) = 0;

  // Will be called when |PerformanceTiming| events are updated
  virtual void DidChangePerformanceTiming() {}
  // Will be called when an |InputEvent| is observed.
  virtual void DidObserveInputDelay(base::TimeDelta input_delay) {}

  // Will be called when |CpuTiming| events are updated
  virtual void DidChangeCpuTiming(base::TimeDelta time) {}

  // Will be called when a particular loading code path has been used. This
  // propogates renderer loading behavior to the browser process for histograms.
  virtual void DidObserveLoadingBehavior(LoadingBehaviorFlag) {}

  // Will be called when a new UseCounter feature has been observed in a frame.
  // This propogates feature usage to the browser process for histograms.
  virtual void DidObserveNewFeatureUsage(mojom::WebFeature) {}
  // Will be called when a new UseCounter CSS property or animated CSS property
  // has been observed in a frame. This propogates feature usage to the browser
  // process for histograms.
  virtual void DidObserveNewCssPropertyUsage(
      mojom::CSSSampleId /*css_property*/,
      bool /*is_animated*/) {}

  // Reports that visible elements in the frame shifted (bit.ly/lsm-explainer).
  virtual void DidObserveLayoutShift(double score, bool after_input_or_scroll) {
  }

  // Reports the number of LayoutBlock creation, and LayoutObject::UpdateLayout
  // calls. All values are deltas since the last calls of this function.
  virtual void DidObserveLayoutNg(uint32_t all_block_count,
                                  uint32_t ng_block_count,
                                  uint32_t all_call_count,
                                  uint32_t ng_call_count) {}

  // Reports lazy loaded behavior when the frame or image is fully deferred or
  // if the frame or image is loaded after being deferred. Called every time the
  // behavior occurs. This does not apply to images that were loaded as
  // placeholders.
  virtual void DidObserveLazyLoadBehavior(
      WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior) {}

  // Transmits the change in the set of watched CSS selectors property that
  // match any element on the frame.
  virtual void SelectorMatchChanged(
      const Vector<String>& added_selectors,
      const Vector<String>& removed_selectors) = 0;

  virtual DocumentLoader* CreateDocumentLoader(
      LocalFrame*,
      WebNavigationType,
      ContentSecurityPolicy*,
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) = 0;

  virtual void UpdateDocumentLoader(
      DocumentLoader* document_loader,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) = 0;

  virtual String UserAgent() = 0;
  virtual base::Optional<blink::UserAgentMetadata> UserAgentMetadata() = 0;

  virtual String DoNotTrackValue() = 0;

  virtual void TransitionToCommittedForNewPage() = 0;

  virtual LocalFrame* CreateFrame(const AtomicString& name,
                                  HTMLFrameOwnerElement*) = 0;

  // Creates a portal for the |HTMLPortalElement| and binds the other end of the
  // |mojo::PendingAssociatedReceiver<mojom::blink::Portal>|. Returns a pair of
  // a RemoteFrame and a token that identifies the portal. If the returned
  // RemoteFrame is nullptr, then the PortalToken is meaningless.
  virtual std::pair<RemoteFrame*, PortalToken> CreatePortal(
      HTMLPortalElement*,
      mojo::PendingAssociatedReceiver<mojom::blink::Portal>,
      mojo::PendingAssociatedRemote<mojom::blink::PortalClient>) = 0;

  // Adopts the predecessor |portal|. The HTMLPortalElement must have been
  // created by adopting the predecessor in the PortalActivateEvent, and have a
  // valid portal token. Returns a RemoteFrame for the portal.
  // Adopting the predecessor allows a page to keep it alive and embed it as a
  // portal, allowing instantaneous back and forward activations.
  virtual RemoteFrame* AdoptPortal(HTMLPortalElement* portal) = 0;

  // Whether or not plugin creation should fail if the HTMLPlugInElement isn't
  // in the DOM after plugin initialization.
  enum DetachedPluginPolicy {
    kFailOnDetachedPlugin,
    kAllowDetachedPlugin,
  };
  virtual WebPluginContainerImpl* CreatePlugin(HTMLPlugInElement&,
                                               const KURL&,
                                               const Vector<String>&,
                                               const Vector<String>&,
                                               const String&,
                                               bool load_manually) = 0;

  virtual std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) = 0;
  virtual WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement&) = 0;

  virtual void DidCreateInitialEmptyDocument() = 0;
  virtual void DidCommitDocumentReplacementNavigation(DocumentLoader*) = 0;
  virtual void DispatchDidClearWindowObjectInMainWorld() = 0;
  virtual void DocumentElementAvailable() = 0;
  virtual void RunScriptsAtDocumentElementAvailable() = 0;
  virtual void RunScriptsAtDocumentReady(bool document_is_empty) = 0;
  virtual void RunScriptsAtDocumentIdle() = 0;

  virtual void DidCreateScriptContext(v8::Local<v8::Context>,
                                      int32_t world_id) = 0;
  virtual void WillReleaseScriptContext(v8::Local<v8::Context>,
                                        int32_t world_id) = 0;
  virtual bool AllowScriptExtensions() = 0;

  virtual void DidChangeScrollOffset() {}
  virtual void DidUpdateCurrentHistoryItem() {}

  // Called when a content-initiated, main frame navigation to a data URL is
  // about to occur.
  virtual bool AllowContentInitiatedDataUrlNavigations(const KURL&) {
    return false;
  }

  virtual void DidChangeName(const String&) {}

  virtual std::unique_ptr<WebServiceWorkerProvider>
  CreateServiceWorkerProvider() = 0;

  virtual WebContentSettingsClient* GetContentSettingsClient() = 0;

  virtual void DispatchDidChangeManifest() {}

  unsigned BackForwardLength() override { return 0; }

  virtual bool IsLocalFrameClientImpl() const { return false; }

  // Overwrites the given URL to use an HTML5 embed if possible. An empty URL is
  // returned if the URL is not overriden.
  virtual KURL OverrideFlashEmbedWithHTML(const KURL&) { return KURL(); }

  virtual BlameContext* GetFrameBlameContext() { return nullptr; }

  virtual BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() = 0;

  virtual AssociatedInterfaceProvider*
  GetRemoteNavigationAssociatedInterfaces() = 0;

  virtual void NotifyUserActivation() {}

  virtual void AbortClientNavigation() {}

  virtual WebSpellCheckPanelHostClient* SpellCheckPanelHostClient() const = 0;

  virtual WebTextCheckClient* GetTextCheckerClient() const = 0;

  virtual std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() = 0;

  virtual void AnnotatedRegionsChanged() = 0;

  virtual void SetVirtualTimePauser(
      WebScopedVirtualTimePauser virtual_time_pauser) {}

  virtual String evaluateInInspectorOverlayForTesting(const String& script) = 0;

  virtual bool HandleCurrentKeyboardEvent() { return false; }

  virtual void DidChangeSelection(bool is_selection_empty) {}

  virtual void DidChangeContents() {}

  virtual Frame* FindFrame(const AtomicString& name) const = 0;

  virtual void FrameRectsChanged(const IntRect&) {}

  virtual void OnOverlayPopupAdDetected() {}

  virtual void OnLargeStickyAdDetected() {}

  virtual void FocusedElementChanged(Element* element) {}

  // Returns true when the contents of plugin are handled externally. This means
  // the plugin element will own a content frame but the frame is than used
  // externally to load the required handelrs.
  virtual bool IsPluginHandledExternally(HTMLPlugInElement&,
                                         const KURL&,
                                         const String&) {
    return false;
  }

  // When a plugin element is handled externally, this method is used to obtain
  // a scriptable object which exposes custom API such as postMessage.
  virtual v8::Local<v8::Object> GetScriptableObject(HTMLPlugInElement&,
                                                    v8::Isolate*) {
    return v8::Local<v8::Object>();
  }

  // Returns a new WebWorkerFetchContext for a dedicated worker (in the
  // non-PlzDedicatedWorker case) or worklet.
  virtual scoped_refptr<WebWorkerFetchContext> CreateWorkerFetchContext() {
    return nullptr;
  }

  // Returns a new WebWorkerFetchContext for PlzDedicatedWorker.
  // (https://crbug.com/906991)
  virtual scoped_refptr<WebWorkerFetchContext>
  CreateWorkerFetchContextForPlzDedicatedWorker(
      WebDedicatedWorkerHostFactoryClient*) {
    return nullptr;
  }

  virtual std::unique_ptr<WebContentSettingsClient>
  CreateWorkerContentSettingsClient() {
    return nullptr;
  }

  virtual std::unique_ptr<media::SpeechRecognitionClient>
  CreateSpeechRecognitionClient(
      media::SpeechRecognitionClient::OnReadyCallback callback) {
    return nullptr;
  }

  virtual void SetMouseCapture(bool) {}

  // Returns whether we are associated with a print context who suggests to use
  // printing layout.
  virtual bool UsePrintingLayout() const { return false; }

  virtual std::unique_ptr<ResourceLoadInfoNotifierWrapper>
  CreateResourceLoadInfoNotifierWrapper() {
    return nullptr;
  }

  // AppCache ------------------------------------------------------------
  virtual void UpdateSubresourceFactory(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> pending_factory) {}

  virtual void DidChangeMobileFriendliness(const MobileFriendliness&) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_CLIENT_H_
