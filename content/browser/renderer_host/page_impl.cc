// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_impl.h"

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/i18n/character_encoding.h"
#include "base/trace_event/optional_trace_event.h"
#include "cc/base/features.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "content/browser/manifest/manifest_manager_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/page_delegate.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/peak_gpu_memory_tracker_factory.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_client.h"
#include "services/viz/public/mojom/compositing/offset_tag.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace content {

PageImpl::PageImpl(RenderFrameHostImpl& rfh, PageDelegate& delegate)
    : main_document_(rfh),
      delegate_(delegate),
      text_autosizer_page_info_({0, 0, 1.f}) {
  if (base::FeatureList::IsEnabled(
          blink::features::kSharedStorageSelectURLLimit)) {
    select_url_overall_budget_ = static_cast<double>(
        blink::features::kSharedStorageSelectURLBitBudgetPerPageLoad.Get());
    select_url_max_bits_per_site_ = static_cast<double>(
        blink::features::kSharedStorageSelectURLBitBudgetPerSitePerPageLoad
            .Get());
  }
}

PageImpl::~PageImpl() {
  // As SupportsUserData is a base class of PageImpl, Page members will be
  // destroyed before running ~SupportsUserData, which would delete the
  // associated PageUserData objects. Avoid this by calling ClearAllUserData
  // explicitly here to ensure that the PageUserData destructors can access
  // associated Page object.
  ClearAllUserData();

  // If we still have a PeakGpuMemoryTracker, then the loading it was observing
  // never completed. Cancel its callback so that we don't report partial
  // loads to UMA.
  CancelLoadingMemoryTracker();
}

const std::optional<GURL>& PageImpl::GetManifestUrl() const {
  return manifest_url_;
}

void PageImpl::GetManifest(GetManifestCallback callback) {
  ManifestManagerHost* manifest_manager_host =
      ManifestManagerHost::GetOrCreateForPage(*this);
  manifest_manager_host->GetManifest(std::move(callback));
}

bool PageImpl::IsPrimary() const {
  return main_document_->IsInPrimaryMainFrame();
}

void PageImpl::UpdateManifestUrl(const GURL& manifest_url) {
  manifest_url_ = manifest_url;

  // If |main_document_| is not active, the notification is sent on the page
  // activation.
  if (!main_document_->IsActive())
    return;

  main_document_->delegate()->OnManifestUrlChanged(*this);
}

void PageImpl::WriteIntoTrace(perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("main_document", *main_document_);
}

base::WeakPtr<Page> PageImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<PageImpl> PageImpl::GetWeakPtrImpl() {
  return weak_factory_.GetWeakPtr();
}

bool PageImpl::IsPageScaleFactorOne() {
  return GetPageScaleFactor() == 1.f;
}

const std::string& PageImpl::GetContentsMimeType() const {
  return contents_mime_type_;
}

void PageImpl::SetResizableForTesting(std::optional<bool> resizable) {
  SetResizable(resizable);
}

void PageImpl::SetResizable(std::optional<bool> resizable) {
  resizable_ = resizable;
  delegate_->OnCanResizeFromWebAPIChanged();
}

std::optional<bool> PageImpl::GetResizable() {
  return resizable_;
}

void PageImpl::OnFirstVisuallyNonEmptyPaint() {
  did_first_visually_non_empty_paint_ = true;
  delegate_->OnFirstVisuallyNonEmptyPaint(*this);
}

void PageImpl::OnThemeColorChanged(const std::optional<SkColor>& theme_color) {
  main_document_theme_color_ = theme_color;
  delegate_->OnThemeColorChanged(*this);
}

void PageImpl::DidChangeBackgroundColor(SkColor4f background_color,
                                        bool color_adjust) {
  // TODO(aaronhk): This should remain an SkColor4f
  main_document_background_color_ = background_color.toSkColor();
  delegate_->OnBackgroundColorChanged(*this);
  if (color_adjust) {
    // <meta name="color-scheme" content="dark"> may pass the dark canvas
    // background before the first paint in order to avoid flashing the white
    // background in between loading documents. If we perform a navigation
    // within the same renderer process, we keep the content background from the
    // previous page while rendering is blocked in the new page, but for cross
    // process navigations we would paint the default background (typically
    // white) while the rendering is blocked.
    main_document_->GetRenderWidgetHost()->GetView()->SetContentBackgroundColor(
        background_color.toSkColor());
  }
}

void PageImpl::DidInferColorScheme(
    blink::mojom::PreferredColorScheme color_scheme) {
  main_document_inferred_color_scheme_ = color_scheme;
  delegate_->DidInferColorScheme(*this);
}

void PageImpl::NotifyPageBecameCurrent() {
  if (!IsPrimary())
    return;
  delegate_->NotifyPageBecamePrimary(*this);
}

void PageImpl::SetContentsMimeType(std::string mime_type) {
  contents_mime_type_ = std::move(mime_type);
}

void PageImpl::OnTextAutosizerPageInfoChanged(
    blink::mojom::TextAutosizerPageInfoPtr page_info) {
  OPTIONAL_TRACE_EVENT0("content", "PageImpl::OnTextAutosizerPageInfoChanged");

  // Keep a copy of `page_info` in case we create a new `blink::WebView` before
  // the next update, so that the PageImpl can tell the newly created
  // `blink::WebView` about the autosizer info.
  text_autosizer_page_info_.main_frame_width = page_info->main_frame_width;
  text_autosizer_page_info_.main_frame_layout_width =
      page_info->main_frame_layout_width;
  text_autosizer_page_info_.device_scale_adjustment =
      page_info->device_scale_adjustment;

  auto remote_frames_broadcast_callback = base::BindRepeating(
      [](const blink::mojom::TextAutosizerPageInfo& page_info,
         RenderFrameProxyHost* proxy_host) {
        DCHECK(proxy_host);
        proxy_host->GetAssociatedRemoteMainFrame()->UpdateTextAutosizerPageInfo(
            page_info.Clone());
      },
      text_autosizer_page_info_);

  main_document_->frame_tree()
      ->root()
      ->render_manager()
      ->ExecuteRemoteFramesBroadcastMethod(
          std::move(remote_frames_broadcast_callback),
          main_document_->GetSiteInstance()->group());
}

void PageImpl::SetActivationStartTime(base::TimeTicks activation_start) {
  CHECK(!activation_start_time_);
  activation_start_time_ = activation_start;
}

void PageImpl::Activate(
    ActivationType type,
    StoredPage::RenderViewHostImplSafeRefSet& render_view_hosts,
    std::optional<blink::ViewTransitionState> view_transition_state,
    base::OnceCallback<void(base::TimeTicks)> completion_callback) {
  TRACE_EVENT1("navigation", "PageImpl::Activate", "activation_type", type);

  // SetActivationStartTime() should be called first as the value is used in
  // the callback below.
  CHECK(activation_start_time_.has_value());

  base::OnceClosure did_activate_render_views = base::BindOnce(
      &PageImpl::DidActivateAllRenderViewsForPrerenderingOrPreview,
      weak_factory_.GetWeakPtr(), std::move(completion_callback));

  base::RepeatingClosure barrier = base::BarrierClosure(
      render_view_hosts.size(), std::move(did_activate_render_views));
  bool view_transition_state_consumed = false;
  for (const auto& rvh : render_view_hosts) {
    auto params = blink::mojom::PrerenderPageActivationParams::New();

    if (main_document_->GetRenderViewHost() == &*rvh) {
      // For prerendering activation, send activation_start only to the
      // RenderViewHost for the main frame to avoid sending the info
      // cross-origin. Only this RenderViewHost needs the info, as we expect the
      // other RenderViewHosts are made for cross-origin iframes which have not
      // yet loaded their document. To the renderer, it just looks like an
      // ongoing navigation is happening in the frame and has not yet committed.
      // These RenderViews still need to know about activation so their
      // documents are created in the non-prerendered state once their
      // navigation is committed.
      params->activation_start = *activation_start_time_;
      // Note that there cannot be a use-after-move since the if condition
      // should be true at most once.
      CHECK(!view_transition_state_consumed);
      params->view_transition_state = std::move(view_transition_state);
      view_transition_state_consumed = true;
    } else if (type == ActivationType::kPreview) {
      // For preview activation, send activation_start to all RenderViewHosts
      // as preview loads cross-origin subframes under the capability control,
      // and activation_start time is meaningful there.
      params->activation_start = *activation_start_time_;
    }

    // For preview activation, there is no way to activate the previewed page
    // other than with a user action, or testing only methods.
    params->was_user_activated =
        (main_document_->frame_tree_node()
             ->has_received_user_gesture_before_nav() ||
         type == ActivationType::kPreview)
            ? blink::mojom::WasActivatedOption::kYes
            : blink::mojom::WasActivatedOption::kNo;
    rvh->ActivatePrerenderedPage(std::move(params), barrier);
  }

  // Prepare each RenderFrameHostImpl in this Page for activation.
  main_document_->ForEachRenderFrameHostIncludingSpeculative(
      [](RenderFrameHostImpl* rfh) {
        rfh->RendererWillActivateForPrerenderingOrPreview();
      });
}

void PageImpl::MaybeDispatchLoadEventsOnPrerenderActivation() {
  DCHECK(IsPrimary());

  // Dispatch LoadProgressChanged notification on activation with the
  // prerender last load progress value if the value is not equal to
  // blink::kFinalLoadProgress, whose notification is dispatched during call
  // to DidStopLoading.
  if (load_progress() != blink::kFinalLoadProgress)
    main_document_->DidChangeLoadProgress(load_progress());

  // Dispatch PrimaryMainDocumentElementAvailable before dispatching following
  // load complete events.
  if (is_main_document_element_available())
    main_document_->MainDocumentElementAvailable(uses_temporary_zoom_level());

  main_document_->ForEachRenderFrameHost(
      &RenderFrameHostImpl::MaybeDispatchDOMContentLoadedOnPrerenderActivation);

  if (is_on_load_completed_in_main_document())
    main_document_->DocumentOnLoadCompleted();

  main_document_->ForEachRenderFrameHost(
      &RenderFrameHostImpl::MaybeDispatchDidFinishLoadOnPrerenderActivation);
}

void PageImpl::DidActivateAllRenderViewsForPrerenderingOrPreview(
    base::OnceCallback<void(base::TimeTicks)> completion_callback) {
  TRACE_EVENT0("navigation",
               "PageImpl::DidActivateAllRenderViewsForPrerendering");

  // Tell each RenderFrameHostImpl in this Page that activation finished.
  main_document_->ForEachRenderFrameHostIncludingSpeculative(
      [this](RenderFrameHostImpl* rfh) {
        if (&rfh->GetPage() != this) {
          return;
        }
        rfh->RendererDidActivateForPrerendering();
      });
  CHECK(activation_start_time_.has_value());
  std::move(completion_callback).Run(*activation_start_time_);
}

RenderFrameHost& PageImpl::GetMainDocumentHelper() {
  return *main_document_;
}

RenderFrameHostImpl& PageImpl::GetMainDocument() const {
  return *main_document_;
}

void PageImpl::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate,
    const std::optional<cc::BrowserControlsOffsetTagsInfo>& offset_tags_info) {
  // TODO(crbug.com/40159655): Asking for the LocalMainFrame interface
  // before the RenderFrame is created is racy.
  if (!GetMainDocument().IsRenderFrameLive())
    return;

  if (base::FeatureList::IsEnabled(
          features::kUpdateBrowserControlsWithoutProxy)) {
    GetMainDocument().GetRenderWidgetHost()->UpdateBrowserControlsState(
        constraints, current, animate, offset_tags_info);
  } else {
    GetMainDocument().GetAssociatedLocalMainFrame()->UpdateBrowserControlsState(
        constraints, current, animate, offset_tags_info);
  }
}

float PageImpl::GetPageScaleFactor() const {
  return GetMainDocument().GetPageScaleFactor();
}

void PageImpl::UpdateEncoding(const std::string& encoding_name) {
  if (encoding_name == last_reported_encoding_)
    return;
  last_reported_encoding_ = encoding_name;

  canonical_encoding_ =
      base::GetCanonicalEncodingNameByAliasName(encoding_name);
}

void PageImpl::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  // TODO(crbug.com/40222405): send notification to outer frames if
  // needed.
  DCHECK_EQ(virtual_keyboard_mode(),
            ui::mojom::VirtualKeyboardMode::kOverlaysContent);
  GetMainDocument().GetAssociatedLocalFrame()->NotifyVirtualKeyboardOverlayRect(
      keyboard_rect);
}

void PageImpl::SetVirtualKeyboardMode(ui::mojom::VirtualKeyboardMode mode) {
  if (virtual_keyboard_mode_ == mode)
    return;

  virtual_keyboard_mode_ = mode;

  delegate_->OnVirtualKeyboardModeChanged(*this);
}

base::flat_map<std::string, std::string> PageImpl::GetKeyboardLayoutMap() {
  return GetMainDocument().GetRenderWidgetHost()->GetKeyboardLayoutMap();
}

blink::SharedStorageSelectUrlBudgetStatus
PageImpl::CheckAndMaybeDebitSelectURLBudgets(const net::SchemefulSite& site,
                                             double bits_to_charge) {
  if (!select_url_overall_budget_) {
    // The limits are not enabled.
    return blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget;
  }

  // Return insufficient if there is insufficient overall budget.
  if (bits_to_charge > select_url_overall_budget_.value()) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &GetMainDocument(),
        blink::mojom::WebFeature::
            kSharedStorageAPI_SelectURLOverallPageloadBudgetInsufficient);
    return blink::SharedStorageSelectUrlBudgetStatus::
        kInsufficientOverallPageloadBudget;
  }

  DCHECK(select_url_max_bits_per_site_);

  // Return false if the max bits per site is set to a value smaller than the
  // current bits to charge.
  if (bits_to_charge > select_url_max_bits_per_site_.value()) {
    return blink::SharedStorageSelectUrlBudgetStatus::
        kInsufficientSitePageloadBudget;
  }

  // Charge the per-site budget or return insufficient if there is not enough.
  auto it = select_url_per_site_budget_.find(site);
  if (it == select_url_per_site_budget_.end()) {
    select_url_per_site_budget_[site] =
        select_url_max_bits_per_site_.value() - bits_to_charge;
  } else if (bits_to_charge > it->second) {
    // There is insufficient per-site budget remaining.
    return blink::SharedStorageSelectUrlBudgetStatus::
        kInsufficientSitePageloadBudget;
  } else {
    it->second -= bits_to_charge;
  }

  // Charge the overall budget.
  select_url_overall_budget_.value() -= bits_to_charge;
  return blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget;
}

void PageImpl::TakeLoadingMemoryTracker(NavigationRequest* request) {
  CHECK(IsPrimary());
  loading_memory_tracker_ = request->TakePeakGpuMemoryTracker();
}

void PageImpl::ResetLoadingMemoryTracker() {
  CHECK(IsPrimary());
  if (loading_memory_tracker_) {
    loading_memory_tracker_.reset();
  }
}

void PageImpl::CancelLoadingMemoryTracker() {
  if (loading_memory_tracker_) {
    loading_memory_tracker_->Cancel();
    loading_memory_tracker_.reset();
  }
}

void PageImpl::SetLastCommitParams(
    mojom::DidCommitProvisionalLoadParamsPtr commit_params) {
  CHECK(GetMainDocument().IsOutermostMainFrame());
  last_commit_params_ = std::move(commit_params);
}

mojom::DidCommitProvisionalLoadParamsPtr PageImpl::TakeLastCommitParams() {
  CHECK(GetMainDocument().IsOutermostMainFrame());
  return std::move(last_commit_params_);
}

}  // namespace content
