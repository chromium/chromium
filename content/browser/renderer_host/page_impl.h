// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_state.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/renderer_host/stored_page.h"
#include "content/common/content_export.h"
#include "content/common/navigation_client.mojom.h"
#include "content/public/browser/page.h"
#include "net/base/schemeful_site.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/frame/text_autosizer_page_info.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom.h"
#include "url/gurl.h"

namespace cc {
struct BrowserControlsOffsetTagsInfo;
}  // namespace cc

namespace input {
class PeakGpuMemoryTracker;
}  // namespace input

namespace content {

class NavigationRequest;
class PageDelegate;
class RenderFrameHostImpl;

// This implements the Page interface that is exposed to embedders of content,
// and adds things only visible to content.

// Please refer to content/public/browser/page.h for more details.
class CONTENT_EXPORT PageImpl : public Page {
 public:
  enum class ActivationType {
    kPrerendering,
    kPreview,
  };
  PageImpl(RenderFrameHostImpl& rfh, PageDelegate& delegate);

  ~PageImpl() override;

  using base::SupportsUserData::ClearAllUserData;

  // Page implementation.
  const std::optional<GURL>& GetManifestUrl() const override;
  void GetManifest(GetManifestCallback callback) override;
  bool IsPrimary() const override;
  void WriteIntoTrace(perfetto::TracedValue context) override;
  base::WeakPtr<Page> GetWeakPtr() override;
  bool IsPageScaleFactorOne() override;
  const std::string& GetContentsMimeType() const override;
  void SetResizableForTesting(std::optional<bool> resizable) override;
  std::optional<bool> GetResizable() override;

  // Setter for the `window.setResizable(bool)` API's value defining whether the
  // window can be resized or not. `std::nullopt` means the value is not set.
  void SetResizable(std::optional<bool> resizable);

  base::WeakPtr<PageImpl> GetWeakPtrImpl();

  virtual void UpdateManifestUrl(const GURL& manifest_url);

  RenderFrameHostImpl& GetMainDocument() const;

  bool is_on_load_completed_in_main_document() const {
    return is_on_load_completed_in_main_document_;
  }
  void set_is_on_load_completed_in_main_document(bool completed) {
    is_on_load_completed_in_main_document_ = completed;
  }

  bool is_main_document_element_available() const {
    return is_main_document_element_available_;
  }
  void set_is_main_document_element_available(bool completed) {
    is_main_document_element_available_ = completed;
  }

  bool uses_temporary_zoom_level() const { return uses_temporary_zoom_level_; }
  void set_uses_temporary_zoom_level(bool level) {
    uses_temporary_zoom_level_ = level;
  }

  void OnFirstVisuallyNonEmptyPaint();
  bool did_first_visually_non_empty_paint() const {
    return did_first_visually_non_empty_paint_;
  }

  const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls() const {
    return favicon_urls_;
  }
  void set_favicon_urls(std::vector<blink::mojom::FaviconURLPtr> favicon_urls) {
    favicon_urls_ = std::move(favicon_urls);
  }

  void OnThemeColorChanged(const std::optional<SkColor>& theme_color);

  void DidChangeBackgroundColor(SkColor4f background_color, bool color_adjust);

  // Notifies the page's color scheme was inferred.
  void DidInferColorScheme(blink::mojom::PreferredColorScheme color_scheme);

  void NotifyPageBecameCurrent();

  std::optional<SkColor> theme_color() const {
    return main_document_theme_color_;
  }

  std::optional<SkColor> background_color() const {
    return main_document_background_color_;
  }

  std::optional<blink::mojom::PreferredColorScheme> inferred_color_scheme()
      const {
    return main_document_inferred_color_scheme_;
  }

  void SetContentsMimeType(std::string mime_type);

  void OnTextAutosizerPageInfoChanged(
      blink::mojom::TextAutosizerPageInfoPtr page_info);

  blink::mojom::TextAutosizerPageInfo text_autosizer_page_info() const {
    return text_autosizer_page_info_;
  }

  FencedFrameURLMapping& fenced_frame_urls_map() {
    return fenced_frame_urls_map_;
  }

  void set_last_main_document_source_id(ukm::SourceId id) {
    last_main_document_source_id_ = id;
  }
  ukm::SourceId last_main_document_source_id() const {
    return last_main_document_source_id_;
  }

  // Sets the start time of the prerender activation navigation for this page.
  // TODO(falken): Plumb NavigationRequest to
  // RenderFrameHostManager::CommitPending and remove this.
  void SetActivationStartTime(base::TimeTicks activation_start);

  // Called during the activation navigation. Sends an IPC to the RenderViews in
  // the renderers, instructing them to transition their documents from
  // prerendered to activated. Tells the corresponding RenderFrameHostImpls that
  // the renderer will be activating their documents.
  void Activate(
      ActivationType type,
      StoredPage::RenderViewHostImplSafeRefSet& render_view_hosts_to_activate,
      std::optional<blink::ViewTransitionState> view_transition_state,
      base::OnceCallback<void(base::TimeTicks)> completion_callback);

  // Prerender2:
  // Dispatches load events that were deferred to be dispatched after
  // activation. Please note that this should only be called on prerender
  // activation.
  void MaybeDispatchLoadEventsOnPrerenderActivation();

  // Hide or show the browser controls for the given Page, based on allowed
  // states, desired state and whether the transition should be animated or
  // not.
  void UpdateBrowserControlsState(
      cc::BrowserControlsState constraints,
      cc::BrowserControlsState current,
      bool animate,
      const std::optional<cc::BrowserControlsOffsetTagsInfo>& offset_tags_info);

  float GetPageScaleFactor() const;

  void set_load_progress(double load_progress) {
    load_progress_ = load_progress;
  }
  double load_progress() const { return load_progress_; }

  void NotifyVirtualKeyboardOverlayRect(const gfx::Rect& keyboard_rect);

  void SetVirtualKeyboardMode(ui::mojom::VirtualKeyboardMode mode);
  ui::mojom::VirtualKeyboardMode virtual_keyboard_mode() const {
    return virtual_keyboard_mode_;
  }

  const std::string& GetEncoding() { return canonical_encoding_; }
  void UpdateEncoding(const std::string& encoding_name);

  // Returns the keyboard layout mapping.
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap();

  // Returns whether a pending call to `sharedStorage.selectURL()` has
  // sufficient budget for `site`, debiting `select_url_overall_budget_` and
  // `select_url_per_site_budget_[site]` if so and if
  // `blink::features::kSharedStorageSelectURLLimit` is enabled. If
  // `blink::features::kSharedStorageSelectURLLimit` is disabled, always returns
  // `blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget`. If there is
  // insufficient budget, the returned enum value specifies which budget was
  // insufficient.
  blink::SharedStorageSelectUrlBudgetStatus CheckAndMaybeDebitSelectURLBudgets(
      const net::SchemefulSite& site,
      double bits_to_charge);

  // See documentation for |credentialless_iframes_nonce_|.
  const base::UnguessableToken& credentialless_iframes_nonce() const {
    return credentialless_iframes_nonce_;
  }

  // Take ownership of the loading memory tracker from the NavigationRequest
  // that navigated to this page.
  void TakeLoadingMemoryTracker(NavigationRequest* request);
  // If we have a loading memory tracker, close it as loading has stopped. It
  // will asynchronously receive the statistics from the GPU process, and update
  // UMA stats.
  void ResetLoadingMemoryTracker();
  // If we have a loading memory tracker, cancel it as loading hasn't stopped
  // and the page is being navigated away from. UMA stats will not be recorded.
  void CancelLoadingMemoryTracker();

  bool is_overriding_user_agent() { return is_overriding_user_agent_; }
  void set_is_overriding_user_agent(bool is_overriding_user_agent) {
    is_overriding_user_agent_ = is_overriding_user_agent;
  }

  // Use to set and release |last_commit_params_|, see documentation of the
  // member for more details. This is only called for outermost pages.
  void SetLastCommitParams(
      mojom::DidCommitProvisionalLoadParamsPtr commit_params);
  mojom::DidCommitProvisionalLoadParamsPtr TakeLastCommitParams();

 private:
  void DidActivateAllRenderViewsForPrerenderingOrPreview(
      base::OnceCallback<void(base::TimeTicks)> completion_callback);

  // This method is needed to ensure that PageImpl can both implement a Page's
  // method and define a new GetMainDocument(). Please refer to page.h for more
  // details.
  RenderFrameHost& GetMainDocumentHelper() override;

  // True if we've received a notification that the onload() handler has
  // run for the main document.
  bool is_on_load_completed_in_main_document_ = false;

  // True if we've received a notification that the window.document element
  // became available for the main document.
  bool is_main_document_element_available_ = false;

  // True if plugin zoom level is set for the main document.
  bool uses_temporary_zoom_level_ = false;

  // Overall load progress of this Page. Initial load progress value is 0.0
  // before the load has begun.
  double load_progress_ = 0.0;

  // Web application manifest URL for this page.
  // See https://w3c.github.io/manifest/#web-application-manifest.
  //
  // This is non-nullopt when the page gets an update of the manifest URL. It
  // can be the empty URL when the manifest url is removed and a non-empty
  // URL when it has a valid URL for the manifest. If this is non-nullopt,
  // WebContentsObserver::DidUpdateWebManifestURL() will be called
  // (either immediately on document load, or on activation in the case
  // of a prerendered page).
  //
  // nullopt indicates that the page did not get an update of the
  // manifest URL, and DidUpdateWebManifestURL() will not be called.
  std::optional<GURL> manifest_url_;

  // Candidate favicon URLs. Each page may have a collection and will be
  // displayed when active (i.e., upon activation for prerendering).
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls_;

  // Whether the first visually non-empty paint has occurred.
  bool did_first_visually_non_empty_paint_ = false;

  // Stores the value set by `window.setResizable(bool)` API for whether the
  // window can be resized or not. `std::nullopt` means the value is not set.
  std::optional<bool> resizable_ = std::nullopt;

  // The theme color for the underlying document as specified
  // by theme-color meta tag.
  std::optional<SkColor> main_document_theme_color_;

  // The background color for the underlying document as computed by CSS.
  std::optional<SkColor> main_document_background_color_;

  // The inferred color scheme of the document.
  std::optional<blink::mojom::PreferredColorScheme>
      main_document_inferred_color_scheme_;

  // Contents MIME type for the main document. It can be used to check whether
  // we can do something for special contents.
  std::string contents_mime_type_;

  // Fenced frames:
  // Any fenced frames created within this page will access this map.
  FencedFrameURLMapping fenced_frame_urls_map_;

  // If `blink::features::kSharedStorageSelectURLLimit` is enabled, the number
  // of bits of entropy remaining in this pageload's overall budget for calls to
  // `sharedStorage.selectURL()`. Calls from all sites on this page are
  // charged to this budget. `select_url_overall_budget_` is not renewed until
  // `this` is destroyed, and it does not rely on any assumptions about when
  // specifically `this` is destroyed (e.g. during navigation or not).
  std::optional<double> select_url_overall_budget_;

  // If `blink::features::kSharedStorageSelectURLLimit` is enabled, the maximum
  // number of bits of entropy in a single site's budget.
  std::optional<double> select_url_max_bits_per_site_;

  // A map of sites to the number bits of entropy remaining in the site's
  // budget for calls to `sharedStorage.selectURL()` during this pageload.
  // `select_url_per_site_budget_` is not cleared until `this` is destroyed,
  // and it does not rely on any assumptions about when specifically `this` is
  // destroyed (e.g. during navigation or not). Used only if
  // `blink::features::kSharedStorageSelectURLLimit` is enabled.
  base::flat_map<net::SchemefulSite, double> select_url_per_site_budget_;

  // This class is owned by the main RenderFrameHostImpl and it's safe to keep a
  // reference to it.
  const raw_ref<RenderFrameHostImpl> main_document_;

  // SourceId of the navigation in this page's main frame. Note that a same
  // document navigation is the only case where this source id can change, since
  // all other navigations create a new PageImpl instance.
  ukm::SourceId last_main_document_source_id_ = ukm::kInvalidSourceId;

  // This page is owned by the RenderFrameHostImpl, which in turn does not
  // outlive the delegate (the contents).
  const raw_ref<PageDelegate> delegate_;

  // Stores information from the main frame's renderer that needs to be shared
  // with OOPIF renderers.
  blink::mojom::TextAutosizerPageInfo text_autosizer_page_info_;

  // Prerender2: The start time of the activation navigation for prerendering,
  // which is passed to the renderer process, and will be accessible in the
  // prerendered page as PerformanceNavigationTiming.activationStart. Set after
  // navigation commit.
  // TODO(b:291867362): Plumb NavigationRequest to
  // RenderFrameHostManager::CommitPending and remove this.
  std::optional<base::TimeTicks> activation_start_time_;

  // The resizing mode requested by Blink for the virtual keyboard.
  ui::mojom::VirtualKeyboardMode virtual_keyboard_mode_ =
      ui::mojom::VirtualKeyboardMode::kUnset;

  // The last reported character encoding, not canonicalized.
  std::string last_reported_encoding_;
  // The canonicalized character encoding.
  std::string canonical_encoding_;

  // Nonce to be used for initializing the storage key and the network isolation
  // key of credentialless iframes which are children of this page's main
  // document.
  const base::UnguessableToken credentialless_iframes_nonce_ =
      base::UnguessableToken::Create();

  // This is only set for primary pages.
  // Created by NavigationRequest; ownership is maintained until the frame has
  // stopped loading, or we navigate away from the page before it finishes
  // loading.
  std::unique_ptr<input::PeakGpuMemoryTracker> loading_memory_tracker_;

  // Whether the page is overriding the user agent or not.
  bool is_overriding_user_agent_ = false;

  // This is used to re-commit when restoring a page from the BackForwardCache
  // or when activating a prerendered page, with the same params as the original
  // navigation.
  mojom::DidCommitProvisionalLoadParamsPtr last_commit_params_;

  base::WeakPtrFactory<PageImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
