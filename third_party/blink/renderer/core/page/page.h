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

#include "base/macros.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/page/page.mojom-blink.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/vision_deficiency.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/settings_delegate.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap_observer_set.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace cc {
class AnimationHost;
}

namespace blink {
class AgentMetricsCollector;
class AutoscrollController;
class BrowserControls;
class ChromeClient;
class ConsoleMessageStorage;
class InspectorIssueStorage;
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
struct PageScaleConstraints;
class PageScaleConstraintsSet;
class PluginData;
class PluginsChangedObserver;
class PointerLockController;
class ScopedPagePauser;
class ScrollingCoordinator;
class ScrollbarTheme;
class SecurityOrigin;
class Settings;
class SpatialNavigationController;
class TopDocumentRootScrollerController;
class ValidationMessageClient;
class VisualViewport;

typedef uint64_t LinkHash;

float DeviceScaleFactorDeprecated(LocalFrame*);

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
  // It is up to the platform to ensure that non-null clients are provided where
  // required.
  struct CORE_EXPORT PageClients final {
    STACK_ALLOCATED();

   public:
    PageClients();

    ChromeClient* chrome_client;
    DISALLOW_COPY_AND_ASSIGN(PageClients);
  };

  // Any pages not owned by a web view should be created using this method.
  static Page* CreateNonOrdinary(PageClients& pages_clients);

  // An "ordinary" page is a fully-featured page owned by a web view.
  static Page* CreateOrdinary(PageClients&, Page* opener);

  explicit Page(PageClients&);
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

  static void PlatformColorsChanged();
  static void ColorSchemeChanged();

  void InitialStyleChanged();
  void UpdateAcceleratedCompositingSettings();

  ViewportDescription GetViewportDescription() const;

  // Returns the plugin data associated with |main_frame_origin|.
  PluginData* GetPluginData(const SecurityOrigin* main_frame_origin);

  // Resets the plugin data for all pages in the renderer process and notifies
  // PluginsChangedObservers.
  static void ResetPluginData();

  // When this method is called, page_scheduler_->SetIsMainFrameLocal should
  // also be called to update accordingly.
  // TODO(npm): update the |page_scheduler_| directly in this method.
  void SetMainFrame(Frame*);
  Frame* MainFrame() const { return main_frame_; }
  // Escape hatch for existing code that assumes that the root frame is
  // always a LocalFrame. With OOPI, this is not always the case. Code that
  // depends on this will generally have to be rewritten to propagate any
  // necessary state through all renderer processes for that page and/or
  // coordinate/rely on the browser process to help dispatch/coordinate work.
  LocalFrame* DeprecatedLocalMainFrame() const;

  void DocumentDetached(Document*);

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
  AgentMetricsCollector* GetAgentMetricsCollector() const {
    return agent_metrics_collector_.Get();
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

  // Corresponds to pixel density of the device where this Page is
  // being displayed. In multi-monitor setups this can vary between pages.
  // This value does not account for Page zoom, use LocalFrame::devicePixelRatio
  // instead.  This is to be deprecated. Use this with caution.
  // 1) If you need to scale the content per device scale factor, this is still
  //    valid.  In use-zoom-for-dsf mode, this is always 1, and will be remove
  //    when transition is complete.
  // 2) If you want to compute the device related measure (such as device pixel
  //    height, or the scale factor for drag image), use
  //    ChromeClient::screenInfo() instead.
  float DeviceScaleFactorDeprecated() const { return device_scale_factor_; }
  void SetDeviceScaleFactorDeprecated(float);

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

  void AnimationHostInitialized(cc::AnimationHost&, LocalFrameView*);
  void WillCloseAnimationHost(LocalFrameView*);

  void WillBeDestroyed();

  void RegisterPluginsChangedObserver(PluginsChangedObserver*);

  ScrollbarTheme& GetScrollbarTheme() const;

  PageScheduler* GetPageScheduler() const;

  // PageScheduler::Delegate implementation.
  bool IsOrdinary() const override;
  void ReportIntervention(const String& message) override;
  bool RequestBeginMainFrameNotExpected(bool new_state) override;
  void OnSetPageFrozen(bool is_frozen) override;
  bool LocalMainFrameNetworkIsAlmostIdle() const override;

  void AddAutoplayFlags(int32_t flags);
  void ClearAutoplayFlags();

  int32_t AutoplayFlags() const;

  void SetInsidePortal(bool inside_portal);
  bool InsidePortal() const;

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

  HeapObserverSet<PageVisibilityObserver>& PageVisibilityObserverSet() {
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

  static void PrepareForLeakDetection();

 private:
  friend class ScopedPagePauser;

  void InitGroup();

  // SettingsDelegate overrides.
  void SettingsChanged(SettingsDelegate::ChangeType) override;

  // Notify |plugins_changed_observers_| that plugins have changed.
  void NotifyPluginsChanged() const;

  void SetPageScheduler(std::unique_ptr<PageScheduler>);

  void InvalidateColorScheme();
  void InvalidatePaint();
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
  Member<Frame> main_frame_;

  Member<PageAnimator> animator_;
  const Member<AutoscrollController> autoscroll_controller_;
  Member<ChromeClient> chrome_client_;
  const Member<DragCaret> drag_caret_;
  const Member<DragController> drag_controller_;
  const Member<FocusController> focus_controller_;
  const Member<ContextMenuController> context_menu_controller_;
  const Member<PageScaleConstraintsSet> page_scale_constraints_set_;
  HeapObserverSet<PageVisibilityObserver> page_visibility_observer_set_;
  const Member<PointerLockController> pointer_lock_controller_;
  Member<ScrollingCoordinator> scrolling_coordinator_;
  const Member<BrowserControls> browser_controls_;
  const Member<ConsoleMessageStorage> console_message_storage_;
  const Member<InspectorIssueStorage> inspector_issue_storage_;
  const Member<TopDocumentRootScrollerController>
      global_root_scroller_controller_;
  const Member<VisualViewport> visual_viewport_;
  const Member<OverscrollController> overscroll_controller_;
  const Member<LinkHighlight> link_highlight_;
  Member<SpatialNavigationController> spatial_navigation_controller_;

  Member<PluginData> plugin_data_;

  Member<ValidationMessageClient> validation_message_client_;

  // Stored only for ordinary pages to avoid adding metrics from things like
  // overlays, popups and SVG.
  Member<AgentMetricsCollector> agent_metrics_collector_;

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

  float device_scale_factor_;

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

  mojom::blink::TextAutosizerPageInfo web_text_autosizer_page_info_;

  WebScopedVirtualTimePauser history_navigation_virtual_time_pauser_;

  DISALLOW_COPY_AND_ASSIGN(Page);
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<Page>;

class CORE_EXPORT InternalSettingsPageSupplementBase : public Supplement<Page> {
 public:
  using Supplement<Page>::Supplement;
  static const char kSupplementName[];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_PAGE_H_
