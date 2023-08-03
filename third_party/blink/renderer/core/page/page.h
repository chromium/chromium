/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_PAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_PAGE_H_

#include <memory>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/text_autosizer_page_info.mojom-blink.h"
#include "third_party/blink/public/mojom/page/page.mojom-blink.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/web/web_lifecycle_update.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_producer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/vision_deficiency.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/settings_delegate.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace cc {
class AnimationHost;
}

namespace blink {
class AutoscrollController;
class BrowserControls;
class ChromeClient;
class ConsoleMessageStorage;
class ContextMenuController;
class Document;
class DragCaret;
class DragController;
class FocusController;
class Frame;
class LinkHighlight;
class LocalFrame;
class LocalFrameView;
class MediaFeatureOverrides;
class OverscrollController;
class PageAnimator;
struct PageScaleConstraints;
class PageScaleConstraintsSet;
class PluginData;
class PluginsChangedObserver;
class PointerLockController;
class ScopedPagePauser;
class ScrollingCoordinator;
class ScrollbarTheme;
class Settings;
class SpatialNavigationController;
class TopDocumentRootScrollerController;
class ValidationMessageClient;
class VisualViewport;

typedef uint64_t LinkHash;

// A Page roughly corresponds to a tab or popup window in a browser. It owns a
// tree of frames (a blink::FrameTree). The root frame is called the main frame.
//
// Note that frames can be local or remote to this process.
class CORE_EXPORT Page final : public GarbageCollected<Page>,
                               public Supplementable<Page>,
                               public SettingsDelegate,
                               public PageScheduler::Delegate {
  friend class Settings;

 public:
  // Any pages not owned by a web view should be created using this method.
  static Page* CreateNonOrdinary(ChromeClient& chrome_client,
                                 AgentGroupScheduler& agent_group_scheduler);

  // An "ordinary" page is a fully-featured page owned by a web view.
  static Page* CreateOrdinary(
      ChromeClient& chrome_client,
      Page* opener,
      AgentGroupScheduler& agent_group_scheduler,
      const BrowsingContextGroupInfo& browsing_context_group_info);

  Page(base::PassKey<Page>,
       ChromeClient& chrome_client,
       AgentGroupScheduler& agent_group_scheduler,
       const BrowsingContextGroupInfo& browsing_context_group_info,
       bool is_ordinary);
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;
  ~Page() override;

  void CloseSoon();
  bool IsClosing() const { return is_closing_; }

  using PageSet = HeapHashSet<WeakMember<Page>>;

  // Return the current set of full-fledged, ordinary pages.
  // Each created and owned by a WebView.
  //
  // This set does not include Pages created for other, internal purposes
  // (SVGImages, inspector overlays, page popups etc.)
  static PageSet& OrdinaryPages();
  static void InsertOrdinaryPageForTesting(Page*);

  // Returns pages related to the current browsing context (excluding the
  // current page).  See also
  // https://html.spec.whatwg.org/C/#unit-of-related-browsing-contexts
  HeapVector<Member<Page>> RelatedPages();

  // Should be called when |GetScrollbarTheme().UsesOverlayScrollbars()|
  // changes.
  static void UsesOverlayScrollbarsChanged();
  static void PlatformColorsChanged();
  static void ColorSchemeChanged();
  static void ColorProvidersChanged();

  void InitialStyleChanged();
  void UpdateAcceleratedCompositingSettings();

  ViewportDescription GetViewportDescription() const;

  // Returns the plugin data.
  PluginData* GetPluginData();

  // Resets the plugin data for all pages in the renderer process and notifies
  // PluginsChangedObservers.
  static void ResetPluginData();

  // When this method is called, page_scheduler_->SetIsMainFrameLocal should
  // also be called to update accordingly.
  // TODO(npm): update the |page_scheduler_| directly in this method.
  void SetMainFrame(Frame*);
  Frame* MainFrame() const { return main_frame_; }

  void SetPreviousMainFrameForLocalSwap(
      LocalFrame* previous_main_frame_for_local_swap) {
    previous_main_frame_for_local_swap_ = previous_main_frame_for_local_swap;
  }

  LocalFrame* GetPreviousMainFrameForLocalSwap() {
    return previous_main_frame_for_local_swap_;
  }

  // Escape hatch for existing code that assumes that the root frame is
  // always a LocalFrame. With OOPI, this is not always the case. Code that
  // depends on this will generally have to be rewritten to propagate any
  // necessary state through all renderer processes for that page and/or
  // coordinate/rely on the browser process to help dispatch/coordinate work.
  LocalFrame* DeprecatedLocalMainFrame() const;

  void DocumentDetached(Document*);

  void Animate(base::TimeTicks monotonic_frame_begin_time);

  // The |root| argument indicates a root LocalFrame from which to start
  // performing the operation. See comment on WebWidget::UpdateLifecycle.
  void UpdateLifecycle(LocalFrame& root,
                       WebLifecycleUpdate requested_update,
                       DocumentUpdateReason reason);

  bool OpenedByDOM() const;
  void SetOpenedByDOM();

  PageAnimator& Animator() { return *animator_; }
  ChromeClient& GetChromeClient() const {
    DCHECK(chrome_client_) << "No chrome client";
    return *chrome_client_;
  }
  AutoscrollController& GetAutoscrollController() const {
    return *autoscroll_controller_;
  }
  DragCaret& GetDragCaret() const { return *drag_caret_; }
  DragController& GetDragController() const { return *drag_controller_; }
  FocusController& GetFocusController() const { return *focus_controller_; }
  SpatialNavigationController& GetSpatialNavigationController();
  ContextMenuController& GetContextMenuController() const {
    return *context_menu_controller_;
  }
  PointerLockController& GetPointerLockController() const {
    return *pointer_lock_controller_;
  }
  ValidationMessageClient& GetValidationMessageClient() const {
    return *validation_message_client_;
  }
  void SetValidationMessageClientForTesting(ValidationMessageClient*);

  ScrollingCoordinator* GetScrollingCoordinator();

  Settings& GetSettings() const { return *settings_; }

  Deprecation& GetDeprecation() { return deprecation_; }

  void SetWindowFeatures(const WebWindowFeatures& features) {
    window_features_ = features;
  }
  const WebWindowFeatures& GetWindowFeatures() const {
    return window_features_;
  }

  PageScaleConstraintsSet& GetPageScaleConstraintsSet();
  const PageScaleConstraintsSet& GetPageScaleConstraintsSet() const;

  BrowserControls& GetBrowserControls();
  const BrowserControls& GetBrowserControls() const;

  ConsoleMessageStorage& GetConsoleMessageStorage();
  const ConsoleMessageStorage& GetConsoleMessageStorage() const;

  InspectorIssueStorage& GetInspectorIssueStorage();
  const InspectorIssueStorage& GetInspectorIssueStorage() const;

  TopDocumentRootScrollerController& GlobalRootScrollerController() const;

  VisualViewport& GetVisualViewport();
  const VisualViewport& GetVisualViewport() const;

  LinkHighlight& GetLinkHighlight();

  OverscrollController& GetOverscrollController();
  const OverscrollController& GetOverscrollController() const;

  void SetTabKeyCyclesThroughElements(bool b) {
    tab_key_cycles_through_elements_ = b;
  }
  bool TabKeyCyclesThroughElements() const {
    return tab_key_cycles_through_elements_;
  }

  // Pausing is used to implement the "Optionally, pause while waiting for
  // the user to acknowledge the message" step of simple dialog processing:
  // https://html.spec.whatwg.org/C/#simple-dialogs
  //
  // Per https://html.spec.whatwg.org/C/#pause, no loads
  // are allowed to start/continue in this state, and all background processing
  // is also paused.
  bool Paused() const { return paused_; }
  void SetPaused(bool);

  // Frozen state corresponds to "lifecycle state for CPU suspension"
  // https://wicg.github.io/page-lifecycle/#sec-lifecycle-states
  bool Frozen() const { return frozen_; }

  void SetPageScaleFactor(float);
  float PageScaleFactor() const;

  float InspectorDeviceScaleFactorOverride() const {
    return inspector_device_scale_factor_override_;
  }
  void SetInspectorDeviceScaleFactorOverride(float override) {
    inspector_device_scale_factor_override_ = override;
  }

  static void AllVisitedStateChanged(bool invalidate_visited_link_hashes);
  static void VisitedStateChanged(LinkHash visited_hash);

  void SetVisibilityState(mojom::blink::PageVisibilityState visibility_state,
                          bool is_initial_state);
  mojom::blink::PageVisibilityState GetVisibilityState() const;
  bool IsPageVisible() const;

  bool IsCursorVisible() const;
  void SetIsCursorVisible(bool is_visible) { is_cursor_visible_ = is_visible; }

  // Don't allow more than a certain number of frames in a page.
  static int MaxNumberOfFrames();
  static void SetMaxNumberOfFramesToTenForTesting(bool enabled);

  void IncrementSubframeCount() { ++subframe_count_; }
  void DecrementSubframeCount() {
    DCHECK_GT(subframe_count_, 0);
    --subframe_count_;
  }
  int SubframeCount() const;

  void SetDefaultPageScaleLimits(float min_scale, float max_scale);
  void SetUserAgentPageScaleConstraints(
      const PageScaleConstraints& new_constraints);

#if DCHECK_IS_ON()
  void SetIsPainting(bool painting) { is_painting_ = painting; }
  bool IsPainting() const { return is_painting_; }
#endif

  void DidCommitLoad(LocalFrame*);

  void AcceptLanguagesChanged();

  void Trace(Visitor*) const override;

  void DidInitializeCompositing(cc::AnimationHost&);
  void WillStopCompositing();

  void WillBeDestroyed();

  void RegisterPluginsChangedObserver(PluginsChangedObserver*);

  ScrollbarTheme& GetScrollbarTheme() const;

  AgentGroupScheduler& GetAgentGroupScheduler() const;
  PageScheduler* GetPageScheduler() const;

  // PageScheduler::Delegate implementation.
  bool IsOrdinary() const override;
  void ReportIntervention(const String& message) override;
  bool RequestBeginMainFrameNotExpected(bool new_state) override;
  void OnSetPageFrozen(bool is_frozen) override;

  void AddAutoplayFlags(int32_t flags);
  void ClearAutoplayFlags();

  int32_t AutoplayFlags() const;

  void SetInsidePortal(bool inside_portal);
  bool InsidePortal() const;

  void SetIsPrerendering(bool is_prerendering) {
    is_prerendering_ = is_prerendering;
  }
  bool IsPrerendering() const { return is_prerendering_; }

  void SetTextAutosizerPageInfo(
      const mojom::blink::TextAutosizerPageInfo& page_info) {
    web_text_autosizer_page_info_ = page_info;
  }
  const mojom::blink::TextAutosizerPageInfo& TextAutosizerPageInfo() const {
    return web_text_autosizer_page_info_;
  }

  void SetMediaFeatureOverride(const AtomicString& media_feature,
                               const String& value);
  const MediaFeatureOverrides* GetMediaFeatureOverrides() const {
    return media_feature_overrides_.get();
  }
  void ClearMediaFeatureOverrides();

  void SetVisionDeficiency(VisionDeficiency new_vision_deficiency);
  VisionDeficiency GetVisionDeficiency() const { return vision_deficiency_; }

  WebScopedVirtualTimePauser& HistoryNavigationVirtualTimePauser() {
    return history_navigation_virtual_time_pauser_;
  }

  HeapLinkedHashSet<WeakMember<PageVisibilityObserver>>&
  PageVisibilityObserverSet() {
    return page_visibility_observer_set_;
  }

  void SetPageLifecycleState(
      mojom::blink::PageLifecycleStatePtr lifecycle_state) {
    lifecycle_state_ = std::move(lifecycle_state);
  }

  const mojom::blink::PageLifecycleStatePtr& GetPageLifecycleState() {
    return lifecycle_state_;
  }

  // Whether we've dispatched "pagehide" on this page previously, and haven't
  // dispatched the "pageshow" event after the last time we've dispatched
  // "pagehide". This means that we've navigated away from the page and it's
  // still hidden (possibly preserved in the back-forward cache, or unloaded).
  bool DispatchedPagehideAndStillHidden();

  // Similar to above, but will only return true if we've dispatched 'pagehide'
  // with the 'persisted' property set to 'true'.
  bool DispatchedPagehidePersistedAndStillHidden();

  static void PrepareForLeakDetection();

  // Fully invalidate paint of all local frames in this page.
  void InvalidatePaint();

  // Should be invoked when the main frame of this frame tree is a fenced frame.
  void SetIsMainFrameFencedFrameRoot();
  // Returns if the main frame of this frame tree is a fenced frame.
  bool IsMainFrameFencedFrameRoot() const;

  void SetDeprecatedFencedFrameMode(
      blink::FencedFrame::DeprecatedFencedFrameMode mode) {
    fenced_frame_mode_ = mode;
  }
  blink::FencedFrame::DeprecatedFencedFrameMode DeprecatedFencedFrameMode() {
    return fenced_frame_mode_;
  }

  v8_compile_hints::V8CrowdsourcedCompileHintsProducer&
  GetV8CrowdsourcedCompileHintsProducer() {
    return *v8_compile_hints_producer_;
  }

  // Returns the token uniquely identifying the browsing context group this page
  // lives in.
  const base::UnguessableToken& BrowsingContextGroupToken();

  // Returns the token uniquely identifying the CoopRelatedGroup this page lives
  // in.
  const base::UnguessableToken& CoopRelatedGroupToken();

  // Update this Page's browsing context group after a navigation has taken
  // place.
  void UpdateBrowsingContextGroup(const blink::BrowsingContextGroupInfo&);

 private:
  friend class ScopedPagePauser;

  void InitGroup();

  // SettingsDelegate overrides.
  void SettingsChanged(SettingsDelegate::ChangeType) override;

  // Notify |plugins_changed_observers_| that plugins have changed.
  void NotifyPluginsChanged() const;

  void InvalidateColorScheme();

  // Typically, the main frame and Page should both be owned by the embedder,
  // which must call Page::willBeDestroyed() prior to destroying Page. This
  // call detaches the main frame and clears this pointer, thus ensuring that
  // this field only references a live main frame.
  //
  // However, there are several locations (InspectorOverlay, SVGImage, and
  // WebPagePopupImpl) which don't hold a reference to the main frame at all
  // after creating it. These are still safe because they always create a
  // Frame with a LocalFrameView. LocalFrameView and Frame hold references to
  // each other, thus keeping each other alive. The call to willBeDestroyed()
  // breaks this cycle, so the frame is still properly destroyed once no
  // longer needed.
  // Note that the main frame can either be a LocalFrame or a RemoteFrame. When
  // the main frame is a RemoteFrame, it's possible for the RemoteFrame to not
  // be connected to any RenderFrameProxyHost on the browser side, if the Page
  // is a new page created for a provisional main frame. In that case, the main
  // frame is solely used as a placeholder to be swapped out by the provisional
  // main frame later on.
  // See comments in `AgentSchedulingGroup::CreateWebView()` for more details.
  Member<Frame> main_frame_;

  // When a Page is created for a provisional main frame, which is intended to
  // do a local main frame swap when its navigation commits, this will point to
  // the previous Page's main frame. This is so that the provisional main frame
  // can trigger the detach and "swap out" the previous Page's main frame. This
  // is a WeakMember because the lifetime of this page and the previous Page
  // should be independent. If the previous Page gets destroyed, the provisional
  // Page can still exist (but the browser might trigger its deletion later on).
  WeakMember<LocalFrame> previous_main_frame_for_local_swap_;

  Member<AgentGroupScheduler> agent_group_scheduler_;
  Member<PageAnimator> animator_;
  const Member<AutoscrollController> autoscroll_controller_;
  Member<ChromeClient> chrome_client_;
  const Member<DragCaret> drag_caret_;
  const Member<DragController> drag_controller_;
  const Member<FocusController> focus_controller_;
  const Member<ContextMenuController> context_menu_controller_;
  const Member<PageScaleConstraintsSet> page_scale_constraints_set_;
  HeapLinkedHashSet<WeakMember<PageVisibilityObserver>>
      page_visibility_observer_set_;
  const Member<PointerLockController> pointer_lock_controller_;
  Member<ScrollingCoordinator> scrolling_coordinator_;
  const Member<BrowserControls> browser_controls_;
  const Member<ConsoleMessageStorage> console_message_storage_;
  const Member<TopDocumentRootScrollerController>
      global_root_scroller_controller_;
  const Member<VisualViewport> visual_viewport_;
  const Member<OverscrollController> overscroll_controller_;
  const Member<LinkHighlight> link_highlight_;
  Member<SpatialNavigationController> spatial_navigation_controller_;

  Member<PluginData> plugin_data_;

  Member<ValidationMessageClient> validation_message_client_;

  InspectorIssueStorage inspector_issue_storage_;

  Deprecation deprecation_;
  WebWindowFeatures window_features_;

  bool opened_by_dom_;
  // Set to true when window.close() has been called and the Page will be
  // destroyed. The browsing contexts in this page should no longer be
  // discoverable via JS.
  // TODO(dcheng): Try to remove |DOMWindow::m_windowIsClosing| in favor of
  // this. However, this depends on resolving https://crbug.com/674641
  bool is_closing_;

  bool tab_key_cycles_through_elements_;

  float inspector_device_scale_factor_override_;

  mojom::blink::PageLifecycleStatePtr lifecycle_state_;

  bool is_ordinary_;

  bool is_cursor_visible_;

  // See Page::Paused and Page::Frozen for the detailed description of paused
  // and frozen state. The main distinction is that "frozen" state is
  // web-exposed (onfreeze / onresume) and controlled from the browser process,
  // while "paused" state is an implementation detail of handling sync IPCs and
  // controlled from the renderer.
  bool paused_ = false;
  bool frozen_ = false;

#if DCHECK_IS_ON()
  bool is_painting_ = false;
#endif

  int subframe_count_;

  HeapHashSet<WeakMember<PluginsChangedObserver>> plugins_changed_observers_;

  // A circular, double-linked list of pages that are related to the current
  // browsing context.  See also RelatedPages method.
  Member<Page> next_related_page_;
  Member<Page> prev_related_page_;

  // A handle to notify the scheduler whether this page has other related
  // pages or not.
  FrameScheduler::SchedulingAffectingFeatureHandle has_related_pages_;

  std::unique_ptr<PageScheduler> page_scheduler_;

  // Overrides for various media features, set from DevTools.
  std::unique_ptr<MediaFeatureOverrides> media_feature_overrides_;

  // Emulated vision deficiency, set from DevTools.
  VisionDeficiency vision_deficiency_ = VisionDeficiency::kNoVisionDeficiency;

  int32_t autoplay_flags_;

  // Accessed by frames to determine whether to expose the PortalHost object.
  bool inside_portal_ = false;

  // Whether the page is being prerendered by the Prerender2
  // feature. See content/browser/preloading/prerender/README.md.
  //
  // This is ordinarily initialized by WebViewImpl immediately after creating
  // this Page. Once initialized, it can only transition from true to false on
  // prerender activation; it does not go from false to true.
  bool is_prerendering_ = false;

  // Whether the the Page's main document is a Fenced Frame document. This is
  // only set for the MPArch implementation and is true when the corresponding
  // browser side FrameTree has the FrameTree::Type of kFencedFrame.
  bool is_fenced_frame_tree_ = false;

  // This tracks the mode that the fenced frame is set to.
  blink::FencedFrame::DeprecatedFencedFrameMode fenced_frame_mode_ =
      blink::FencedFrame::DeprecatedFencedFrameMode::kDefault;

  mojom::blink::TextAutosizerPageInfo web_text_autosizer_page_info_;

  WebScopedVirtualTimePauser history_navigation_virtual_time_pauser_;

  Member<v8_compile_hints::V8CrowdsourcedCompileHintsProducer>
      v8_compile_hints_producer_;

  // The information determining the browsing context group this page lives in.
  BrowsingContextGroupInfo browsing_context_group_info_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<Page>;

class CORE_EXPORT InternalSettingsPageSupplementBase : public Supplement<Page> {
 public:
  using Supplement<Page>::Supplement;
  static const char kSupplementName[];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_PAGE_H_
