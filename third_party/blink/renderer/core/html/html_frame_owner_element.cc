/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"

#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/lazy_load_frame_observer.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

using PluginSet = HeapHashSet<Member<WebPluginContainerImpl>>;
PluginSet& PluginsPendingDispose() {
  DEFINE_STATIC_LOCAL(Persistent<PluginSet>, set,
                      (MakeGarbageCollected<PluginSet>()));
  return *set;
}

bool DoesParentAllowLazyLoadingChildren(Document& document) {
  LocalFrame* containing_frame = document.GetFrame();
  if (!containing_frame)
    return true;

  // If the embedding document has no owner, then by default allow lazy loading
  // children.
  FrameOwner* containing_frame_owner = containing_frame->Owner();
  if (!containing_frame_owner)
    return true;

  return containing_frame_owner->ShouldLazyLoadChildren();
}

bool IsFrameLazyLoadable(const ExecutionContext* context,
                         const KURL& url,
                         bool is_loading_attr_lazy,
                         bool should_lazy_load_children) {
  if (!RuntimeEnabledFeatures::LazyFrameLoadingEnabled() &&
      !RuntimeEnabledFeatures::LazyFrameVisibleLoadTimeMetricsEnabled()) {
    return false;
  }

  // Only http:// or https:// URLs are eligible for lazy loading, excluding
  // URLs like invalid or empty URLs, "about:blank", local file URLs, etc.
  // that it doesn't make sense to lazily load.
  if (!url.ProtocolIsInHTTPFamily())
    return false;

  if (is_loading_attr_lazy)
    return true;

  if (!should_lazy_load_children ||
      // Disallow lazy loading by default if javascript in the embedding
      // document would be able to access the contents of the frame, since in
      // those cases deferring the frame could break the page. Note that this
      // check does not take any possible redirects of |url| into account.
      context->GetSecurityOrigin()->CanAccess(
          SecurityOrigin::Create(url).get())) {
    return false;
  }

  return true;
}

bool ShouldLazilyLoadFrame(const Document& document,
                           bool is_loading_attr_lazy) {
  DCHECK(document.GetSettings());
  if (!RuntimeEnabledFeatures::LazyFrameLoadingEnabled() ||
      !document.GetSettings()->GetLazyLoadEnabled()) {
    return false;
  }

  // Disable explicit and automatic lazyload for backgrounded pages.
  if (!document.IsPageVisible())
    return false;

  if (is_loading_attr_lazy)
    return true;
  if (!RuntimeEnabledFeatures::AutomaticLazyFrameLoadingEnabled())
    return false;

  // If lazy loading is restricted to only Data Saver users, then avoid
  // lazy loading unless Data Saver is enabled, taking the Data Saver
  // holdback into consideration.
  if (RuntimeEnabledFeatures::
          RestrictAutomaticLazyFrameLoadingToDataSaverEnabled() &&
      !GetNetworkStateNotifier().SaveDataEnabled()) {
    return false;
  }

  // Skip automatic lazyload when reloading a page.
  if (!RuntimeEnabledFeatures::AutoLazyLoadOnReloadsEnabled() &&
      document.Loader() && IsReloadLoadType(document.Loader()->LoadType())) {
    return false;
  }
  return true;
}

}  // namespace

SubframeLoadingDisabler::SubtreeRootSet&
SubframeLoadingDisabler::DisabledSubtreeRoots() {
  DEFINE_STATIC_LOCAL(SubtreeRootSet, nodes, ());
  return nodes;
}

// static
int HTMLFrameOwnerElement::PluginDisposeSuspendScope::suspend_count_ = 0;

void HTMLFrameOwnerElement::PluginDisposeSuspendScope::
    PerformDeferredPluginDispose() {
  DCHECK_EQ(suspend_count_, 1);
  suspend_count_ = 0;

  PluginSet dispose_set;
  PluginsPendingDispose().swap(dispose_set);
  for (const auto& plugin : dispose_set) {
    plugin->Dispose();
  }
}

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tag_name,
                                             Document& document)
    : HTMLElement(tag_name, document),
      content_frame_(nullptr),
      embedded_content_view_(nullptr),
      should_lazy_load_children_(DoesParentAllowLazyLoadingChildren(document)),
      is_swapping_frames_(false) {}

LayoutEmbeddedContent* HTMLFrameOwnerElement::GetLayoutEmbeddedContent() const {
  // HTMLObjectElement and HTMLEmbedElement may return arbitrary layoutObjects
  // when using fallback content.
  if (!GetLayoutObject() || !GetLayoutObject()->IsLayoutEmbeddedContent())
    return nullptr;
  return ToLayoutEmbeddedContent(GetLayoutObject());
}

void HTMLFrameOwnerElement::SetContentFrame(Frame& frame) {
  // Make sure we will not end up with two frames referencing the same owner
  // element.
  DCHECK(!content_frame_ || content_frame_->Owner() != this);
  // Disconnected frames should not be allowed to load.
  DCHECK(isConnected());

  // There should be no lazy load in progress since before SetContentFrame,
  // |this| frame element should have been disconnected.
  DCHECK(!lazy_load_frame_observer_ ||
         !lazy_load_frame_observer_->IsLazyLoadPending());

  content_frame_ = &frame;

  // Invalidate compositing inputs, because a remote frame child can cause the
  // owner to become composited.
  if (auto* box = GetLayoutBox()) {
    if (auto* layer = box->Layer())
      layer->SetNeedsCompositingInputsUpdate();
  }
  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kFrame));

  for (ContainerNode* node = this; node; node = node->ParentOrShadowHostNode())
    node->IncrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::ClearContentFrame() {
  if (!content_frame_)
    return;

  // It's possible for there to be a lazy load in progress right now if
  // Frame::Detach() was called without
  // HTMLFrameOwnerElement::DisconnectContentFrame() being called first, so
  // cancel any pending lazy load here.
  // TODO(dcheng): Change this back to a DCHECK asserting that no lazy load is
  // in progress once https://crbug.com/773683 is fixed.
  CancelPendingLazyLoad();

  DCHECK_EQ(content_frame_->Owner(), this);
  content_frame_ = nullptr;

  for (ContainerNode* node = this; node; node = node->ParentOrShadowHostNode())
    node->DecrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::DisconnectContentFrame() {
  if (!ContentFrame())
    return;

  CancelPendingLazyLoad();

  // Removing a subframe that was still loading can impact the result of
  // AllDescendantsAreComplete that is consulted by Document::ShouldComplete.
  // Therefore we might need to re-check this after removing the subframe. The
  // re-check is not needed for local frames (which will handle re-checking from
  // FrameLoader::DidFinishNavigation that responds to LocalFrame::Detach).
  // OTOH, re-checking is required for OOPIFs - see https://crbug.com/779433.
  Document& parent_doc = GetDocument();
  bool have_to_check_if_parent_is_completed =
      ContentFrame()->IsRemoteFrame() && ContentFrame()->IsLoading();

  // FIXME: Currently we don't do this in removedFrom because this causes an
  // unload event in the subframe which could execute script that could then
  // reach up into this document and then attempt to look back down. We should
  // see if this behavior is really needed as Gecko does not allow this.
  ContentFrame()->Detach(FrameDetachType::kRemove);

  // Check if removing the subframe caused |parent_doc| to finish loading.
  if (have_to_check_if_parent_is_completed)
    parent_doc.CheckCompleted();
}

HTMLFrameOwnerElement::~HTMLFrameOwnerElement() {
  // An owner must by now have been informed of detachment
  // when the frame was closed.
  DCHECK(!content_frame_);
}

Document* HTMLFrameOwnerElement::contentDocument() const {
  auto* content_local_frame = DynamicTo<LocalFrame>(content_frame_.Get());
  return content_local_frame ? content_local_frame->GetDocument() : nullptr;
}

DOMWindow* HTMLFrameOwnerElement::contentWindow() const {
  return content_frame_ ? content_frame_->DomWindow() : nullptr;
}

void HTMLFrameOwnerElement::SetSandboxFlags(
    network::mojom::blink::WebSandboxFlags flags) {
  frame_policy_.sandbox_flags = flags;
  // Recalculate the container policy in case the allow-same-origin flag has
  // changed.
  frame_policy_.container_policy = ConstructContainerPolicy();

  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (ContentFrame()) {
    GetDocument().GetFrame()->GetLocalFrameHostRemote().DidChangeFramePolicy(
        ContentFrame()->GetFrameToken(), frame_policy_);
  }
}

void HTMLFrameOwnerElement::SetDisallowDocumentAccesss(bool disallowed) {
  frame_policy_.disallow_document_access = disallowed;
  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (ContentFrame()) {
    GetDocument().GetFrame()->GetLocalFrameHostRemote().DidChangeFramePolicy(
        ContentFrame()->GetFrameToken(), frame_policy_);
  }
}

bool HTMLFrameOwnerElement::IsKeyboardFocusable() const {
  return content_frame_ && HTMLElement::IsKeyboardFocusable();
}

void HTMLFrameOwnerElement::DisposePluginSoon(WebPluginContainerImpl* plugin) {
  if (PluginDisposeSuspendScope::suspend_count_) {
    PluginsPendingDispose().insert(plugin);
    PluginDisposeSuspendScope::suspend_count_ |= 1;
  } else
    plugin->Dispose();
}

void HTMLFrameOwnerElement::UpdateContainerPolicy() {
  frame_policy_.container_policy = ConstructContainerPolicy();
  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet.
  if (ContentFrame()) {
    GetDocument().GetFrame()->GetLocalFrameHostRemote().DidChangeFramePolicy(
        ContentFrame()->GetFrameToken(), frame_policy_);
  }
}

void HTMLFrameOwnerElement::UpdateRequiredPolicy() {
  if (!RuntimeEnabledFeatures::DocumentPolicyNegotiationEnabled(
          GetExecutionContext()))
    return;

  auto* frame = GetDocument().GetFrame();
  DocumentPolicyFeatureState new_required_policy =
      frame
          ? DocumentPolicy::MergeFeatureState(
                ConstructRequiredPolicy(), /* self_required_policy */
                frame->GetRequiredDocumentPolicy() /* parent_required_policy */)
          : ConstructRequiredPolicy();

  // Filter out policies that are disabled by origin trials.
  frame_policy_.required_document_policy.clear();
  for (auto i = new_required_policy.begin(), last = new_required_policy.end();
       i != last;) {
    if (!DisabledByOriginTrial(i->first, GetExecutionContext()))
      frame_policy_.required_document_policy.insert(*i);
    ++i;
  }

  if (ContentFrame()) {
    frame->GetLocalFrameHostRemote().DidChangeFramePolicy(
        ContentFrame()->GetFrameToken(), frame_policy_);
  }
}

network::mojom::blink::TrustTokenParamsPtr
HTMLFrameOwnerElement::ConstructTrustTokenParams() const {
  return nullptr;
}

void HTMLFrameOwnerElement::FrameOwnerPropertiesChanged() {
  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet; or if we are in the middle of
  // swapping one frame for another, in which case the final state of
  // properties will be propagated at the end of the swapping operation.
  if (is_swapping_frames_ || !ContentFrame())
    return;

  mojom::blink::FrameOwnerPropertiesPtr properties =
      mojom::blink::FrameOwnerProperties::New();
  properties->name = BrowsingContextContainerName().IsNull()
                         ? WTF::g_empty_string
                         : BrowsingContextContainerName(),
  properties->scrollbar_mode = ScrollbarMode();
  properties->margin_width = MarginWidth();
  properties->margin_height = MarginHeight();
  properties->allow_fullscreen = AllowFullscreen();
  properties->allow_payment_request = AllowPaymentRequest();
  properties->is_display_none = IsDisplayNone();
  properties->color_scheme = GetColorScheme();
  properties->required_csp =
      RequiredCsp().IsNull() ? WTF::g_empty_string : RequiredCsp();

  GetDocument()
      .GetFrame()
      ->GetLocalFrameHostRemote()
      .DidChangeFrameOwnerProperties(ContentFrame()->GetFrameToken(),
                                     std::move(properties));
}

void HTMLFrameOwnerElement::CSPAttributeChanged() {
  if (!base::FeatureList::IsEnabled(network::features::kOutOfBlinkCSPEE))
    return;

  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet; or if we are in the middle of
  // swapping one frame for another, in which case the final state
  // will be propagated at the end of the swapping operation.
  if (is_swapping_frames_ || !ContentFrame())
    return;

  String fake_header =
      "HTTP/1.1 200 OK\nContent-Security-Policy: " + RequiredCsp();
  network::mojom::blink::ParsedHeadersPtr parsed_headers =
      ParseHeaders(fake_header, GetDocument().Url());

  DCHECK_LE(parsed_headers->content_security_policy.size(), 1u);

  network::mojom::blink::ContentSecurityPolicyPtr csp =
      parsed_headers->content_security_policy.IsEmpty()
          ? nullptr
          : std::move(parsed_headers->content_security_policy[0]);

  GetDocument().GetFrame()->GetLocalFrameHostRemote().DidChangeCSPAttribute(
      ContentFrame()->GetFrameToken(), std::move(csp));
}

void HTMLFrameOwnerElement::AddResourceTiming(const ResourceTimingInfo& info) {
  // Resource timing info should only be reported if the subframe is attached.
  DCHECK(ContentFrame() && ContentFrame()->IsLocalFrame());
  DOMWindowPerformance::performance(*GetDocument().domWindow())
      ->GenerateAndAddResourceTiming(info, localName());
}

void HTMLFrameOwnerElement::DispatchLoad() {
  if (lazy_load_frame_observer_)
    lazy_load_frame_observer_->RecordMetricsOnLoadFinished();

  DispatchScopedEvent(*Event::Create(event_type_names::kLoad));
}

Document* HTMLFrameOwnerElement::getSVGDocument(
    ExceptionState& exception_state) const {
  Document* doc = contentDocument();
  if (doc && doc->IsSVGDocument())
    return doc;
  return nullptr;
}

void HTMLFrameOwnerElement::SetEmbeddedContentView(
    EmbeddedContentView* embedded_content_view) {
  if (embedded_content_view == embedded_content_view_)
    return;

  Document* doc = contentDocument();
  if (doc && doc->GetFrame()) {
    bool will_be_display_none = !embedded_content_view;
    if (IsDisplayNone() != will_be_display_none) {
      doc->WillChangeFrameOwnerProperties(MarginWidth(), MarginHeight(),
                                          ScrollbarMode(), will_be_display_none,
                                          GetColorScheme());
    }
  }

  EmbeddedContentView* old_view = embedded_content_view_.Get();
  embedded_content_view_ = embedded_content_view;
  if (old_view) {
    if (old_view->IsAttached()) {
      old_view->DetachFromLayout();
      if (old_view->IsPluginView())
        DisposePluginSoon(To<WebPluginContainerImpl>(old_view));
      else
        old_view->Dispose();
    }
  }

  FrameOwnerPropertiesChanged();

  GetDocument().GetRootScrollerController().DidUpdateIFrameFrameView(*this);

  LayoutEmbeddedContent* layout_embedded_content = GetLayoutEmbeddedContent();
  if (!layout_embedded_content)
    return;

  layout_embedded_content->UpdateOnEmbeddedContentViewChange();

  if (embedded_content_view_) {
    if (doc) {
      DCHECK_NE(doc->Lifecycle().GetState(), DocumentLifecycle::kStopping);
    }

    DCHECK_EQ(GetDocument().View(), layout_embedded_content->GetFrameView());
    DCHECK(layout_embedded_content->GetFrameView());
    embedded_content_view_->AttachToLayout();
  }

  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(layout_embedded_content);
}

EmbeddedContentView* HTMLFrameOwnerElement::ReleaseEmbeddedContentView() {
  if (!embedded_content_view_)
    return nullptr;
  if (embedded_content_view_->IsAttached())
    embedded_content_view_->DetachFromLayout();
  LayoutEmbeddedContent* layout_embedded_content = GetLayoutEmbeddedContent();
  if (layout_embedded_content) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
      cache->ChildrenChanged(layout_embedded_content);
  }
  return embedded_content_view_.Release();
}

bool HTMLFrameOwnerElement::LoadOrRedirectSubframe(
    const KURL& url,
    const AtomicString& frame_name,
    bool replace_current_item) {
  TRACE_EVENT0("navigation", "HTMLFrameOwnerElement::LoadOrRedirectSubframe");
  // Update the |should_lazy_load_children_| value according to the "loading"
  // attribute immediately, so that it still gets respected even if the "src"
  // attribute gets parsed in ParseAttribute() before the "loading" attribute
  // does.
  if (should_lazy_load_children_ &&
      EqualIgnoringASCIICase(FastGetAttribute(html_names::kLoadingAttr),
                             "eager")) {
    should_lazy_load_children_ = false;
  }

  UpdateContainerPolicy();
  UpdateRequiredPolicy();

  KURL url_to_request = url.IsNull() ? BlankURL() : url;
  ResourceRequestHead request(url_to_request);
  request.SetReferrerPolicy(ReferrerPolicyAttribute());

  network::mojom::blink::TrustTokenParamsPtr trust_token_params =
      ConstructTrustTokenParams();
  if (trust_token_params)
    request.SetTrustTokenParams(*trust_token_params);

  const auto& loading_attr = FastGetAttribute(html_names::kLoadingAttr);
  bool loading_lazy_set = EqualIgnoringASCIICase(loading_attr, "lazy");

  if (ContentFrame()) {
    FrameLoadRequest frame_load_request(GetDocument().domWindow(), request);
    frame_load_request.SetClientRedirectReason(
        ClientNavigationReason::kFrameNavigation);
    WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;
    if (replace_current_item)
      frame_load_type = WebFrameLoadType::kReplaceCurrentItem;

    if (IsFrameLazyLoadable(GetExecutionContext(), url, loading_lazy_set,
                            should_lazy_load_children_)) {
      // Avoid automatically deferring subresources inside a lazily loaded
      // frame. This will make it possible for subresources in hidden frames to
      // load that will never be visible, as well as make it so that deferred
      // frames that have multiple layers of iframes inside them can load faster
      // once they're near the viewport or visible.
      should_lazy_load_children_ = false;

      lazy_load_frame_observer_ = MakeGarbageCollected<LazyLoadFrameObserver>(
          *this, LazyLoadFrameObserver::LoadType::kSubsequent);

      if (RuntimeEnabledFeatures::LazyFrameVisibleLoadTimeMetricsEnabled())
        lazy_load_frame_observer_->StartTrackingVisibilityMetrics();

      if (ShouldLazilyLoadFrame(GetDocument(), loading_lazy_set)) {
        lazy_load_frame_observer_->DeferLoadUntilNearViewport(request,
                                                              frame_load_type);
        return true;
      }
    }

    ContentFrame()->Navigate(frame_load_request, frame_load_type);
    return true;
  }

  if (!SubframeLoadingDisabler::CanLoadFrame(*this))
    return false;

  if (GetDocument().GetFrame()->GetPage()->SubframeCount() >=
      Page::MaxNumberOfFrames()) {
    return false;
  }

  LocalFrame* child_frame =
      GetDocument().GetFrame()->Client()->CreateFrame(frame_name, this);
  DCHECK_EQ(ContentFrame(), child_frame);
  if (!child_frame)
    return false;

  // Send 'csp' attribute to the browser.
  CSPAttributeChanged();

  WebFrameLoadType child_load_type = WebFrameLoadType::kReplaceCurrentItem;
  if (!GetDocument().LoadEventFinished() &&
      GetDocument().Loader()->LoadType() ==
          WebFrameLoadType::kReloadBypassingCache) {
    child_load_type = WebFrameLoadType::kReloadBypassingCache;
    request.SetCacheMode(mojom::FetchCacheMode::kBypassCache);
  }

  // Plug-ins should not load via service workers as plug-ins may have their
  // own origin checking logic that may get confused if service workers respond
  // with resources from another origin.
  // https://w3c.github.io/ServiceWorker/#implementer-concerns
  if (IsPlugin())
    request.SetSkipServiceWorker(true);

  if (!lazy_load_frame_observer_ &&
      IsFrameLazyLoadable(GetExecutionContext(), url, loading_lazy_set,
                          should_lazy_load_children_)) {
    // Avoid automatically deferring subresources inside a lazily loaded frame.
    // This will make it possible for subresources in hidden frames to load that
    // will never be visible, as well as make it so that deferred frames that
    // have multiple layers of iframes inside them can load faster once they're
    // near the viewport or visible.
    should_lazy_load_children_ = false;

    lazy_load_frame_observer_ = MakeGarbageCollected<LazyLoadFrameObserver>(
        *this, LazyLoadFrameObserver::LoadType::kFirst);

    if (RuntimeEnabledFeatures::LazyFrameVisibleLoadTimeMetricsEnabled())
      lazy_load_frame_observer_->StartTrackingVisibilityMetrics();

    if (ShouldLazilyLoadFrame(GetDocument(), loading_lazy_set)) {
      lazy_load_frame_observer_->DeferLoadUntilNearViewport(request,
                                                            child_load_type);
      return true;
    }
  }

  FrameLoadRequest frame_load_request(GetDocument().domWindow(), request);
  child_frame->Loader().StartNavigation(frame_load_request, child_load_type);

  return true;
}

void HTMLFrameOwnerElement::CancelPendingLazyLoad() {
  if (!lazy_load_frame_observer_)
    return;
  lazy_load_frame_observer_->CancelPendingLazyLoad();
}

bool HTMLFrameOwnerElement::ShouldLazyLoadChildren() const {
  return should_lazy_load_children_;
}

void HTMLFrameOwnerElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kLoadingAttr) {
    LoadingAttributeValue loading = GetLoadingAttributeValue(params.new_value);
    if (loading == LoadingAttributeValue::kEager) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kLazyLoadFrameLoadingAttributeEager);
    } else if (loading == LoadingAttributeValue::kLazy) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kLazyLoadFrameLoadingAttributeLazy);
    }

    // Setting the loading attribute to eager should eagerly load any pending
    // requests, just as unsetting the loading attribute does if automatic lazy
    // loading is disabled.
    if (loading == LoadingAttributeValue::kEager ||
        (GetDocument().GetSettings() &&
         !ShouldLazilyLoadFrame(GetDocument(),
                                loading == LoadingAttributeValue::kLazy))) {
      should_lazy_load_children_ = false;
      if (lazy_load_frame_observer_ &&
          lazy_load_frame_observer_->IsLazyLoadPending()) {
        lazy_load_frame_observer_->LoadImmediately();
      }
    }
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

bool HTMLFrameOwnerElement::IsAdRelated() const {
  if (!content_frame_)
    return false;

  return content_frame_->IsAdSubframe();
}

ColorScheme HTMLFrameOwnerElement::GetColorScheme() const {
  if (const auto* style = GetComputedStyle())
    return style->UsedColorSchemeForInitialColors();
  return ColorScheme::kLight;
}

void HTMLFrameOwnerElement::SetColorScheme(ColorScheme color_scheme) {
  Document* doc = contentDocument();
  if (doc && doc->GetFrame()) {
    doc->WillChangeFrameOwnerProperties(MarginWidth(), MarginHeight(),
                                        ScrollbarMode(), IsDisplayNone(),
                                        color_scheme);
  }
  FrameOwnerPropertiesChanged();
}

void HTMLFrameOwnerElement::Trace(Visitor* visitor) const {
  visitor->Trace(content_frame_);
  visitor->Trace(embedded_content_view_);
  visitor->Trace(lazy_load_frame_observer_);
  HTMLElement::Trace(visitor);
  FrameOwner::Trace(visitor);
}

}  // namespace blink
