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

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/lazy_load_frame_observer.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/url_matcher.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
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

bool IsFrameLazyLoadable(ExecutionContext* context,
                         const KURL& url,
                         bool is_loading_attr_lazy,
                         bool should_lazy_load_children) {
  if (!RuntimeEnabledFeatures::LazyFrameLoadingEnabled()) {
    return false;
  }

  // Only http:// or https:// URLs are eligible for lazy loading, excluding
  // URLs like invalid or empty URLs, "about:blank", local file URLs, etc.
  // that it doesn't make sense to lazily load.
  if (!url.ProtocolIsInHTTPFamily())
    return false;

  // Do not lazyload frames when JavaScript is disabled, regardless of the
  // `loading` attribute.
  if (!context->CanExecuteScripts(kNotAboutToExecuteScript))
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutomaticLazyLoadFrame {
  kFeatureNotEnabled = 0,
  kTargetFramesNotFound = 1,
  kTargetFramesFound = 2,
  kMaxValue = kTargetFramesFound,
};

int GetLazyAdsSkipFrameCount() {
  DCHECK(
      base::FeatureList::IsEnabled(features::kAutomaticLazyFrameLoadingToAds));
  static const int skip_frame_count = features::kSkipFrameCountForLazyAds.Get();
  return skip_frame_count;
}

int GetLazyEmbedsSkipFrameCount() {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutomaticLazyFrameLoadingToEmbeds));
  static const int skip_frame_count =
      features::kSkipFrameCountForLazyEmbeds.Get();
  return skip_frame_count;
}

bool CheckAndRecordIfShouldLazilyLoadFrame(const Document& document,
                                           bool is_loading_attr_lazy,
                                           bool is_eligible_for_lazy_embeds,
                                           bool is_eligible_for_lazy_ads,
                                           bool record_uma) {
  DCHECK(document.GetSettings());
  if (!RuntimeEnabledFeatures::LazyFrameLoadingEnabled() ||
      !document.GetSettings()->GetLazyLoadEnabled()) {
    return false;
  }

  // Disable explicit lazyload for backgrounded pages.
  if (!document.IsPageVisible())
    return false;

  if (is_loading_attr_lazy)
    return true;

  Document& top_document = document.TopDocument();
  if (top_document.Loader() &&
      top_document.Loader()->IsReloadedOrFormSubmitted()) {
    return false;
  }

  if (record_uma) {
    base::UmaHistogramEnumeration(
        "Blink.AutomaticLazyLoadFrame",
        !base::FeatureList::IsEnabled(
            features::kAutomaticLazyFrameLoadingToEmbeds)
            ? AutomaticLazyLoadFrame::kFeatureNotEnabled
            : is_eligible_for_lazy_embeds
                  ? AutomaticLazyLoadFrame::kTargetFramesFound
                  : AutomaticLazyLoadFrame::kTargetFramesNotFound);
  }

  if (is_eligible_for_lazy_embeds) {
    top_document.IncrementLazyEmbedsFrameCount();
    UseCounter::Count(top_document, WebFeature::kAutomaticLazyEmbeds);
  }

  if (is_eligible_for_lazy_ads) {
    top_document.IncrementLazyAdsFrameCount();
    UseCounter::Count(top_document, WebFeature::kAutomaticLazyAds);
  }

  if (is_eligible_for_lazy_embeds &&
      base::FeatureList::IsEnabled(
          features::kAutomaticLazyFrameLoadingToEmbeds) &&
      document.GetImmediateChildFrameCreationCount() >
          GetLazyEmbedsSkipFrameCount()) {
    return true;
  }

  if (is_eligible_for_lazy_ads &&
      base::FeatureList::IsEnabled(features::kAutomaticLazyFrameLoadingToAds) &&
      document.GetImmediateChildFrameCreationCount() >
          GetLazyAdsSkipFrameCount()) {
    return true;
  }

  return false;
}

// Checks if the passed url is the same origin with the document.
// This is called in order to limit LazyEmbeds/Ads to apply only cross-origin
// frames.
// We're not sure if this is 100% needed, and we should move the check closer
// to context->GetSecurityOrigin()->CanAccess check.
bool AreSameOrigin(const Document& document, const KURL& url) {
  return SecurityOrigin::AreSameOrigin(url, document.Url());
}

const base::TimeDelta GetLazyEmbedsTimeoutMs() {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutomaticLazyFrameLoadingToEmbeds));
  static const base::TimeDelta timeout_ms =
      base::Milliseconds(features::kTimeoutMillisForLazyEmbeds.Get());
  return timeout_ms;
}

const base::TimeDelta GetLazyAdsTimeoutMs() {
  DCHECK(
      base::FeatureList::IsEnabled(features::kAutomaticLazyFrameLoadingToAds));
  static const base::TimeDelta timeout_ms =
      base::Milliseconds(features::kTimeoutMillisForLazyAds.Get());
  return timeout_ms;
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
      should_lazy_load_children_(DoesParentAllowLazyLoadingChildren(document)) {
  document.IncrementImmediateChildFrameCreationCount();
}

LayoutEmbeddedContent* HTMLFrameOwnerElement::GetLayoutEmbeddedContent() const {
  // HTMLObjectElement and HTMLEmbedElement may return arbitrary layoutObjects
  // when using fallback content.
  return DynamicTo<LayoutEmbeddedContent>(GetLayoutObject());
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

  DCHECK_NE(content_frame_, &frame);
  auto* resource_coordinator = RendererResourceCoordinator::Get();
  if (content_frame_)
    resource_coordinator->OnBeforeContentFrameDetached(*content_frame_, *this);
  resource_coordinator->OnBeforeContentFrameAttached(frame, *this);

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
  RendererResourceCoordinator::Get()->OnBeforeContentFrameDetached(
      *content_frame_, *this);

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
  DidChangeContainerPolicy();
}

void HTMLFrameOwnerElement::DidChangeContainerPolicy() {
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

  GetDocument()
      .GetFrame()
      ->GetLocalFrameHostRemote()
      .DidChangeFrameOwnerProperties(ContentFrame()->GetFrameToken(),
                                     std::move(properties));
}

void HTMLFrameOwnerElement::AddResourceTiming(
    mojom::blink::ResourceTimingInfoPtr info) {
  // Resource timing info should only be reported if the subframe is attached.
  DCHECK(ContentFrame() && ContentFrame()->IsLocalFrame());

  // Make sure we don't double-report, e.g. in the case of restored iframes.
  if (!HasPendingFallbackTimingInfo()) {
    return;
  }

  // This would only happen in rare cases, where the frame is navigated from the
  // outside, e.g. by a web extension or window.open() with target, and that
  // navigation would cancel the container-initiated navigation. This safeguard
  // would make this type of race harmless.
  // TODO(crbug.com/1410705): fix this properly by moving IFrame reporting to
  // the browser side.
  if (fallback_timing_info_->name != info->name) {
    return;
  }

  DOMWindowPerformance::performance(*GetDocument().domWindow())
      ->AddResourceTiming(std::move(info), localName());
  DidReportResourceTiming();
}

bool HTMLFrameOwnerElement::HasPendingFallbackTimingInfo() const {
  return !!fallback_timing_info_;
}

void HTMLFrameOwnerElement::DidReportResourceTiming() {
  fallback_timing_info_.reset();
}

void HTMLFrameOwnerElement::WillPerformContainerInitiatedNavigation(
    const KURL& url) {
  if (!url.ProtocolIsInHTTPFamily() &&
      !url.ProtocolIs(url::kUuidInPackageScheme)) {
    return;
  }

  fallback_timing_info_ = CreateResourceTimingInfo(base::TimeTicks::Now(), url,
                                                   /*response=*/nullptr);
}

// This will report fallback timing only if the "real" resource timing had not
// been previously reported: e.g. a cross-origin iframe without TAO.
void HTMLFrameOwnerElement::ReportFallbackResourceTimingIfNeeded() {
  if (!fallback_timing_info_) {
    return;
  }

  mojom::blink::ResourceTimingInfoPtr resource_timing_info;
  resource_timing_info.Swap(&fallback_timing_info_);
  resource_timing_info->response_end = base::TimeTicks::Now();

  DOMWindowPerformance::performance(*GetDocument().domWindow())
      ->AddResourceTiming(std::move(resource_timing_info), localName());
}

void HTMLFrameOwnerElement::DispatchLoad() {
  ReportFallbackResourceTimingIfNeeded();
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

bool HTMLFrameOwnerElement::LoadImmediatelyIfLazy() {
  if (!lazy_load_frame_observer_)
    return false;

  bool lazy_load_pending = lazy_load_frame_observer_->IsLazyLoadPending();
  if (lazy_load_pending)
    lazy_load_frame_observer_->LoadImmediately();
  return lazy_load_pending;
}

bool HTMLFrameOwnerElement::LazyLoadIfPossible(
    const KURL& url,
    const ResourceRequestHead& request,
    WebFrameLoadType frame_load_type) {
  const auto& loading_attr = FastGetAttribute(html_names::kLoadingAttr);
  bool loading_lazy_set = EqualIgnoringASCIICase(loading_attr, "lazy");

  if (!IsFrameLazyLoadable(GetExecutionContext(), url, loading_lazy_set,
                           should_lazy_load_children_)) {
    return false;
  }

  // Avoid automatically deferring subresources inside
  // a lazily loaded frame. This will make it possible
  // for subresources in hidden frames to load that
  // will never be visible, as well as make it so that
  // deferred frames that have multiple layers of
  // iframes inside them can load faster once they're
  // near the viewport or visible.
  should_lazy_load_children_ = false;

  if (lazy_load_frame_observer_)
    lazy_load_frame_observer_->CancelPendingLazyLoad();

  lazy_load_frame_observer_ = MakeGarbageCollected<LazyLoadFrameObserver>(
      *this, LazyLoadFrameObserver::LoadType::kSubsequent);

  // TODO(crbug.com/1341892) Remove having multiple booleans here. We eventually
  // select one reason to decide the timeout, so essentially we don't have to
  // keep them. But currently we need these two booleans separately to record
  // UKM in CheckAndRecordIfShouldLazilyLoadFrame. Once we confirm that we can
  // ignore AutomaticLazyLoadReason::kBothEmbedsAndAds case due to the small
  // amount of the data size, we remove these booleans and
  // AutomaticLazyLoadReason::kBothEmbedsAndAds.
  const bool is_eligible_for_lazy_embeds = IsEligibleForLazyEmbeds(url);
  const bool is_eligible_for_lazy_ads = IsEligibleForLazyAds(url);
  AutomaticLazyLoadReason auto_lazy_load_reason;
  if (is_eligible_for_lazy_embeds && is_eligible_for_lazy_ads) {
    auto_lazy_load_reason = AutomaticLazyLoadReason::kBothEmbedsAndAds;
  } else if (is_eligible_for_lazy_embeds) {
    auto_lazy_load_reason = AutomaticLazyLoadReason::kEmbeds;
  } else if (is_eligible_for_lazy_ads) {
    auto_lazy_load_reason = AutomaticLazyLoadReason::kAds;
  } else {
    auto_lazy_load_reason = AutomaticLazyLoadReason::kNotEligible;
  }
  base::UmaHistogramEnumeration("Blink.AutomaticLazyFrameLoad.Reason",
                                auto_lazy_load_reason);

  if (CheckAndRecordIfShouldLazilyLoadFrame(GetDocument(), loading_lazy_set,
                                            is_eligible_for_lazy_embeds,
                                            is_eligible_for_lazy_ads,
                                            /*record_uma=*/true)) {
    lazy_load_frame_observer_->DeferLoadUntilNearViewport(request,
                                                          frame_load_type);
    MaybeSetTimeoutToStartFrameLoading(url, loading_lazy_set,
                                       auto_lazy_load_reason);

    return true;
  }
  return false;
}

bool HTMLFrameOwnerElement::IsCurrentlyWithinFrameLimit() const {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return false;
  Page* page = frame->GetPage();
  if (!page)
    return false;
  return page->SubframeCount() < Page::MaxNumberOfFrames();
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
  request.SetHasUserGesture(
      LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));

  network::mojom::blink::TrustTokenParamsPtr trust_token_params =
      ConstructTrustTokenParams();
  if (trust_token_params)
    request.SetTrustTokenParams(*trust_token_params);

  if (ContentFrame()) {
    FrameLoadRequest frame_load_request(GetDocument().domWindow(), request);
    frame_load_request.SetClientRedirectReason(
        ClientNavigationReason::kFrameNavigation);
    WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;
    if (replace_current_item)
      frame_load_type = WebFrameLoadType::kReplaceCurrentItem;

    // Check if the existing frame is eligible to be lazy-loaded. This method
    // should be called before starting the navigation.
    if (LazyLoadIfPossible(url, request, frame_load_type)) {
      return true;
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

  // Propagate attributes like 'csp' or 'anonymous' to the browser process.
  DidChangeAttributes();

  WebFrameLoadType child_load_type = WebFrameLoadType::kReplaceCurrentItem;
  // If the frame URL is not about:blank, see if it should do a
  // kReloadBypassingCache navigation, following the parent frame. If the frame
  // URL is about:blank, it should be committed synchronously as a
  // kReplaceCurrentItem navigation (see https://crbug.com/778318).
  if (url != BlankURL() && !GetDocument().LoadEventFinished() &&
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

  // Check if the newly created child frame is eligible to be lazy-loaded.
  // This method should be called before starting the navigation.
  if (!lazy_load_frame_observer_ &&
      LazyLoadIfPossible(url, request, child_load_type)) {
    return true;
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
         !CheckAndRecordIfShouldLazilyLoadFrame(
             GetDocument(), loading == LoadingAttributeValue::kLazy,
             /*is_eligible_for_lazy_embeds=*/false,
             /*is_eligible_for_lazy_ads=*/false,
             /*record_uma=*/false))) {
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

bool HTMLFrameOwnerElement::IsEligibleForLazyEmbeds(const KURL& url) const {
#if DCHECK_IS_ON()
  if (base::FeatureList::IsEnabled(
          features::kAutomaticLazyFrameLoadingToEmbeds)) {
    DCHECK(base::FeatureList::IsEnabled(
        features::kAutomaticLazyFrameLoadingToEmbedUrls))
        << "kAutomaticLazyFrameLoadingToEmbedUrls should be enabled when "
           "kAutomaticLazyFrameLoadingToEmbeds is enabled.";
  }
#endif  // DCHECK_IS_ON()

  // LazyEmbeds targets are third-party frames.
  // Not eligible if the frame url is a same-origin as the parent url.
  if (AreSameOrigin(GetDocument(), url)) {
    return false;
  }

  DEFINE_STATIC_LOCAL(
      features::AutomaticLazyFrameLoadingToEmbedLoadingStrategy,
      loading_strategy,
      (features::kAutomaticLazyFrameLoadingToEmbedLoadingStrategyParam.Get()));

  switch (loading_strategy) {
    case features::AutomaticLazyFrameLoadingToEmbedLoadingStrategy::
        kAllowList: {
      DEFINE_STATIC_LOCAL(UrlMatcher, url_matcher,
                          (UrlMatcher(base::GetFieldTrialParamValueByFeature(
                              features::kAutomaticLazyFrameLoadingToEmbedUrls,
                              "allowed_websites"))));
      return url_matcher.Match(url);
    }
    case features::AutomaticLazyFrameLoadingToEmbedLoadingStrategy::kNonAds:
      return !IsAdRelated();
  }
}

bool HTMLFrameOwnerElement::IsAdRelated() const {
  if (!content_frame_)
    return false;

  return content_frame_->IsAdFrame();
}

bool HTMLFrameOwnerElement::IsEligibleForLazyAds(const KURL& url) {
  // LazyAds targets are third-party frames.
  // Not eligible if the frame url is a same-origin as the parent url.
  return IsAdRelated() && !AreSameOrigin(GetDocument(), url);
}

void HTMLFrameOwnerElement::MaybeSetTimeoutToStartFrameLoading(
    const KURL& url,
    bool is_loading_attr_lazy,
    AutomaticLazyLoadReason auto_lazy_load_reason) {
  // Even if the frame is ad related, respect the explicit loading="lazy"
  // attribute and won't set a timeout if the attribute exists.
  if (is_loading_attr_lazy) {
    return;
  }

  base::TimeDelta timeout_ms;
  switch (auto_lazy_load_reason) {
    case AutomaticLazyLoadReason::kAds:
      timeout_ms = GetLazyAdsTimeoutMs();
      break;
    // We prioritize LazyEmbeds if the frame is eligible for both reasons, at
    // least until the LazyEmbeds experiment finishes to secure the chunk of
    // group size.
    case AutomaticLazyLoadReason::kBothEmbedsAndAds:
    case AutomaticLazyLoadReason::kEmbeds:
      timeout_ms = GetLazyEmbedsTimeoutMs();
      break;
    case AutomaticLazyLoadReason::kNotEligible:
      // If the auto lazy-load is not elibible, do nothing and return.
      return;
  }

  GetDocument()
      .GetTaskRunner(TaskType::kInternalLoading)
      ->PostDelayedTask(
          FROM_HERE,
          WTF::BindOnce(
              base::IgnoreResult(&HTMLFrameOwnerElement::LoadImmediatelyIfLazy),
              WrapWeakPersistent(this)),
          timeout_ms);
}

mojom::blink::ColorScheme HTMLFrameOwnerElement::GetColorScheme() const {
  if (const auto* style = GetComputedStyle())
    return style->UsedColorScheme();
  return mojom::blink::ColorScheme::kLight;
}

void HTMLFrameOwnerElement::SetColorScheme(
    mojom::blink::ColorScheme color_scheme) {
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
