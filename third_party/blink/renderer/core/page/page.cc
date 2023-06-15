/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All
 * Rights Reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include "third_party/blink/renderer/core/page/page.h"

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink-forward.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/vision_deficiency.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/visited_link_state.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/fenced_frame/document_fenced_frames.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/page_hidden_state.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/core/page/plugins_changed_observer.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/page/validation_message_client_impl.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mobile.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

namespace {
// This seems like a reasonable upper bound, and otherwise mutually
// recursive frameset pages can quickly bring the program to its knees
// with exponential growth in the number of frames.
const int kMaxNumberOfFrames = 1000;

// It is possible to use a reduced frame limit for testing, but only two values
// are permitted, the default or reduced limit.
const int kTenFrames = 10;

bool g_limit_max_frames_to_ten_for_testing = false;

}  // namespace

// Function defined in third_party/blink/public/web/blink.h.
void ResetPluginCache(bool reload_pages) {
  // At this point we already know that the browser has refreshed its list, so
  // it is not necessary to force it to be regenerated.
  DCHECK(!reload_pages);
  Page::ResetPluginData();
}

// Set of all live pages; includes internal Page objects that are
// not observable from scripts.
static Page::PageSet& AllPages() {
  DEFINE_STATIC_LOCAL(Persistent<Page::PageSet>, pages,
                      (MakeGarbageCollected<Page::PageSet>()));
  return *pages;
}

Page::PageSet& Page::OrdinaryPages() {
  DEFINE_STATIC_LOCAL(Persistent<Page::PageSet>, pages,
                      (MakeGarbageCollected<Page::PageSet>()));
  return *pages;
}

void Page::InsertOrdinaryPageForTesting(Page* page) {
  OrdinaryPages().insert(page);
}

HeapVector<Member<Page>> Page::RelatedPages() {
  HeapVector<Member<Page>> result;
  Page* ptr = next_related_page_;
  while (ptr != this) {
    result.push_back(ptr);
    ptr = ptr->next_related_page_;
  }
  return result;
}

Page* Page::CreateNonOrdinary(ChromeClient& chrome_client,
                              AgentGroupScheduler& agent_group_scheduler) {
  return MakeGarbageCollected<Page>(
      base::PassKey<Page>(), chrome_client, agent_group_scheduler,
      BrowsingContextGroupInfo::CreateUnique(), /*is_ordinary=*/false);
}

Page* Page::CreateOrdinary(
    ChromeClient& chrome_client,
    Page* opener,
    AgentGroupScheduler& agent_group_scheduler,
    const BrowsingContextGroupInfo& browsing_context_group_info) {
  Page* page = MakeGarbageCollected<Page>(
      base::PassKey<Page>(), chrome_client, agent_group_scheduler,
      browsing_context_group_info, /*is_ordinary=*/true);

  if (opener) {
    // Before: ... -> opener -> next -> ...
    // After: ... -> opener -> page -> next -> ...
    Page* next = opener->next_related_page_;
    opener->next_related_page_ = page;
    page->prev_related_page_ = opener;
    page->next_related_page_ = next;
    next->prev_related_page_ = page;
  }

  OrdinaryPages().insert(page);
  if (ScopedPagePauser::IsActive())
    page->SetPaused(true);
  return page;
}

Page::Page(base::PassKey<Page>,
           ChromeClient& chrome_client,
           AgentGroupScheduler& agent_group_scheduler,
           const BrowsingContextGroupInfo& browsing_context_group_info,
           bool is_ordinary)
    : SettingsDelegate(std::make_unique<Settings>()),
      main_frame_(nullptr),
      agent_group_scheduler_(agent_group_scheduler),
      animator_(MakeGarbageCollected<PageAnimator>(*this)),
      autoscroll_controller_(MakeGarbageCollected<AutoscrollController>(*this)),
      chrome_client_(&chrome_client),
      drag_caret_(MakeGarbageCollected<DragCaret>()),
      drag_controller_(MakeGarbageCollected<DragController>(this)),
      focus_controller_(MakeGarbageCollected<FocusController>(this)),
      context_menu_controller_(
          MakeGarbageCollected<ContextMenuController>(this)),
      page_scale_constraints_set_(
          MakeGarbageCollected<PageScaleConstraintsSet>(this)),
      pointer_lock_controller_(
          MakeGarbageCollected<PointerLockController>(this)),
      browser_controls_(MakeGarbageCollected<BrowserControls>(*this)),
      console_message_storage_(MakeGarbageCollected<ConsoleMessageStorage>()),
      global_root_scroller_controller_(
          MakeGarbageCollected<TopDocumentRootScrollerController>(*this)),
      visual_viewport_(MakeGarbageCollected<VisualViewport>(*this)),
      overscroll_controller_(
          MakeGarbageCollected<OverscrollController>(GetVisualViewport(),
                                                     GetChromeClient())),
      link_highlight_(MakeGarbageCollected<LinkHighlight>(*this)),
      plugin_data_(nullptr),
      // TODO(pdr): Initialize |validation_message_client_| lazily.
      validation_message_client_(
          MakeGarbageCollected<ValidationMessageClientImpl>(*this)),
      opened_by_dom_(false),
      tab_key_cycles_through_elements_(true),
      inspector_device_scale_factor_override_(1),
      lifecycle_state_(mojom::blink::PageLifecycleState::New()),
      is_ordinary_(is_ordinary),
      is_cursor_visible_(true),
      subframe_count_(0),
      next_related_page_(this),
      prev_related_page_(this),
      autoplay_flags_(0),
      web_text_autosizer_page_info_({0, 0, 1.f}),
      v8_compile_hints_(
          MakeGarbageCollected<V8CrowdsourcedCompileHintsProducer>(this)),
      browsing_context_group_info_(browsing_context_group_info) {
  DCHECK(!AllPages().Contains(this));
  AllPages().insert(this);

  page_scheduler_ = agent_group_scheduler_->CreatePageScheduler(this);
  // The scheduler should be set before the main frame.
  DCHECK(!main_frame_);
  if (auto* virtual_time_controller =
          page_scheduler_->GetVirtualTimeController()) {
    history_navigation_virtual_time_pauser_ =
        virtual_time_controller->CreateWebScopedVirtualTimePauser(
            "HistoryNavigation",
            WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant);
  }
}

Page::~Page() {
  // WillBeDestroyed() must be called before Page destruction.
  DCHECK(!main_frame_);
}

void Page::CloseSoon() {
  // Make sure this Page can no longer be found by JS.
  is_closing_ = true;

  // TODO(dcheng): Try to remove this in a followup, it's not obviously needed.
  if (auto* main_local_frame = DynamicTo<LocalFrame>(main_frame_.Get()))
    main_local_frame->Loader().StopAllLoaders(/*abort_client=*/true);

  GetChromeClient().CloseWindowSoon();
}

ViewportDescription Page::GetViewportDescription() const {
  return MainFrame() && MainFrame()->IsLocalFrame() &&
                 DeprecatedLocalMainFrame()->GetDocument()
             ? DeprecatedLocalMainFrame()
                   ->GetDocument()
                   ->GetViewportData()
                   .GetViewportDescription()
             : ViewportDescription();
}

ScrollingCoordinator* Page::GetScrollingCoordinator() {
  if (!scrolling_coordinator_ && settings_->GetAcceleratedCompositingEnabled())
    scrolling_coordinator_ = MakeGarbageCollected<ScrollingCoordinator>(this);

  return scrolling_coordinator_.Get();
}

PageScaleConstraintsSet& Page::GetPageScaleConstraintsSet() {
  return *page_scale_constraints_set_;
}

const PageScaleConstraintsSet& Page::GetPageScaleConstraintsSet() const {
  return *page_scale_constraints_set_;
}

BrowserControls& Page::GetBrowserControls() {
  return *browser_controls_;
}

const BrowserControls& Page::GetBrowserControls() const {
  return *browser_controls_;
}

ConsoleMessageStorage& Page::GetConsoleMessageStorage() {
  return *console_message_storage_;
}

const ConsoleMessageStorage& Page::GetConsoleMessageStorage() const {
  return *console_message_storage_;
}

InspectorIssueStorage& Page::GetInspectorIssueStorage() {
  return inspector_issue_storage_;
}

const InspectorIssueStorage& Page::GetInspectorIssueStorage() const {
  return inspector_issue_storage_;
}

TopDocumentRootScrollerController& Page::GlobalRootScrollerController() const {
  return *global_root_scroller_controller_;
}

VisualViewport& Page::GetVisualViewport() {
  return *visual_viewport_;
}

const VisualViewport& Page::GetVisualViewport() const {
  return *visual_viewport_;
}

OverscrollController& Page::GetOverscrollController() {
  return *overscroll_controller_;
}

const OverscrollController& Page::GetOverscrollController() const {
  return *overscroll_controller_;
}

LinkHighlight& Page::GetLinkHighlight() {
  return *link_highlight_;
}

void Page::SetMainFrame(Frame* main_frame) {
  // TODO(https://crbug.com/952836): Assert that this is only called during
  // initialization or swaps between local and remote frames.
  main_frame_ = main_frame;

  page_scheduler_->SetIsMainFrameLocal(main_frame->IsLocalFrame());
}

Frame* Page::TakePreviousMainFrameForLocalSwap() {
  Frame* frame = previous_main_frame_for_local_swap_;
  previous_main_frame_for_local_swap_ = nullptr;
  return frame;
}

LocalFrame* Page::DeprecatedLocalMainFrame() const {
  return To<LocalFrame>(main_frame_.Get());
}

void Page::DocumentDetached(Document* document) {
  pointer_lock_controller_->DocumentDetached(document);
  context_menu_controller_->DocumentDetached(document);
  if (validation_message_client_)
    validation_message_client_->DocumentDetached(*document);

  GetChromeClient().DocumentDetached(*document);
}

bool Page::OpenedByDOM() const {
  return opened_by_dom_;
}

void Page::SetOpenedByDOM() {
  opened_by_dom_ = true;
}

SpatialNavigationController& Page::GetSpatialNavigationController() {
  if (!spatial_navigation_controller_) {
    spatial_navigation_controller_ =
        MakeGarbageCollected<SpatialNavigationController>(*this);
  }
  return *spatial_navigation_controller_;
}

void Page::UsesOverlayScrollbarsChanged() {
  for (Page* page : AllPages()) {
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (auto* local_frame = DynamicTo<LocalFrame>(frame))
        local_frame->View()->UsesOverlayScrollbarsChanged();
    }
  }
}

void Page::PlatformColorsChanged() {
  for (const Page* page : AllPages()) {
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
        local_frame->GetDocument()->PlatformColorsChanged();
        if (LayoutView* view = local_frame->ContentLayoutObject())
          view->InvalidatePaintForViewAndDescendants();
      }
    }
  }
}

void Page::ColorSchemeChanged() {
  for (const Page* page : AllPages())
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (auto* local_frame = DynamicTo<LocalFrame>(frame))
        local_frame->GetDocument()->ColorSchemeChanged();
    }
}

void Page::ColorProvidersChanged() {
  for (Page* page : AllPages())
    page->InvalidatePaint();
}

void Page::InitialStyleChanged() {
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    local_frame->GetDocument()->GetStyleEngine().InitialStyleChanged();
  }
}

PluginData* Page::GetPluginData() {
  if (!plugin_data_)
    plugin_data_ = MakeGarbageCollected<PluginData>();

  plugin_data_->UpdatePluginList();
  return plugin_data_.Get();
}

void Page::ResetPluginData() {
  for (Page* page : AllPages()) {
    if (page->plugin_data_) {
      page->plugin_data_->ResetPluginData();
      page->NotifyPluginsChanged();
    }
  }
}

static void RestoreSVGImageAnimations() {
  for (const Page* page : AllPages()) {
    if (auto* svg_image_chrome_client =
            DynamicTo<SVGImageChromeClient>(page->GetChromeClient()))
      svg_image_chrome_client->RestoreAnimationIfNeeded();
  }
}

void Page::SetValidationMessageClientForTesting(
    ValidationMessageClient* client) {
  validation_message_client_ = client;
}

void Page::SetPaused(bool paused) {
  if (paused == paused_)
    return;

  paused_ = paused;
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
      local_frame->OnPageLifecycleStateUpdated();
    }
  }
}

void Page::SetDefaultPageScaleLimits(float min_scale, float max_scale) {
  PageScaleConstraints new_defaults =
      GetPageScaleConstraintsSet().DefaultConstraints();
  new_defaults.minimum_scale = min_scale;
  new_defaults.maximum_scale = max_scale;

  if (new_defaults == GetPageScaleConstraintsSet().DefaultConstraints())
    return;

  GetPageScaleConstraintsSet().SetDefaultConstraints(new_defaults);
  GetPageScaleConstraintsSet().ComputeFinalConstraints();
  GetPageScaleConstraintsSet().SetNeedsReset(true);

  if (!MainFrame() || !MainFrame()->IsLocalFrame())
    return;

  LocalFrameView* root_view = DeprecatedLocalMainFrame()->View();

  if (!root_view)
    return;

  root_view->SetNeedsLayout();
}

void Page::SetUserAgentPageScaleConstraints(
    const PageScaleConstraints& new_constraints) {
  if (new_constraints == GetPageScaleConstraintsSet().UserAgentConstraints())
    return;

  GetPageScaleConstraintsSet().SetUserAgentConstraints(new_constraints);

  if (!MainFrame() || !MainFrame()->IsLocalFrame())
    return;

  LocalFrameView* root_view = DeprecatedLocalMainFrame()->View();

  if (!root_view)
    return;

  root_view->SetNeedsLayout();
}

void Page::SetPageScaleFactor(float scale) {
  GetVisualViewport().SetScale(scale);
}

float Page::PageScaleFactor() const {
  return GetVisualViewport().Scale();
}

void Page::AllVisitedStateChanged(bool invalidate_visited_link_hashes) {
  for (const Page* page : OrdinaryPages()) {
    for (Frame* frame = page->main_frame_; frame;
         frame = frame->Tree().TraverseNext()) {
      if (auto* main_local_frame = DynamicTo<LocalFrame>(frame))
        main_local_frame->GetDocument()
            ->GetVisitedLinkState()
            .InvalidateStyleForAllLinks(invalidate_visited_link_hashes);
    }
  }
}

void Page::VisitedStateChanged(LinkHash link_hash) {
  for (const Page* page : OrdinaryPages()) {
    for (Frame* frame = page->main_frame_; frame;
         frame = frame->Tree().TraverseNext()) {
      if (auto* main_local_frame = DynamicTo<LocalFrame>(frame))
        main_local_frame->GetDocument()
            ->GetVisitedLinkState()
            .InvalidateStyleForLink(link_hash);
    }
  }
}

void Page::SetVisibilityState(
    mojom::blink::PageVisibilityState visibility_state,
    bool is_initial_state) {
  if (lifecycle_state_->visibility == visibility_state)
    return;
  lifecycle_state_->visibility = visibility_state;

  if (is_initial_state)
    return;

  for (auto observer : page_visibility_observer_set_) {
    observer->PageVisibilityChanged();
  }

  if (main_frame_) {
    if (lifecycle_state_->visibility ==
        mojom::blink::PageVisibilityState::kVisible)
      RestoreSVGImageAnimations();
    main_frame_->DidChangeVisibilityState();
  }
}

mojom::blink::PageVisibilityState Page::GetVisibilityState() const {
  return lifecycle_state_->visibility;
}

bool Page::IsPageVisible() const {
  return lifecycle_state_->visibility ==
         mojom::blink::PageVisibilityState::kVisible;
}

bool Page::DispatchedPagehideAndStillHidden() {
  return lifecycle_state_->pagehide_dispatch !=
         mojom::blink::PagehideDispatch::kNotDispatched;
}

bool Page::DispatchedPagehidePersistedAndStillHidden() {
  return lifecycle_state_->pagehide_dispatch ==
         mojom::blink::PagehideDispatch::kDispatchedPersisted;
}

void Page::OnSetPageFrozen(bool frozen) {
  if (frozen_ == frozen)
    return;
  frozen_ = frozen;

  for (Frame* frame = main_frame_.Get(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
      local_frame->OnPageLifecycleStateUpdated();
    }
  }
}

bool Page::IsCursorVisible() const {
  return is_cursor_visible_;
}

// static
int Page::MaxNumberOfFrames() {
  if (UNLIKELY(g_limit_max_frames_to_ten_for_testing))
    return kTenFrames;
  return kMaxNumberOfFrames;
}

// static
void Page::SetMaxNumberOfFramesToTenForTesting(bool enabled) {
  g_limit_max_frames_to_ten_for_testing = enabled;
}

#if DCHECK_IS_ON()
void CheckFrameCountConsistency(int expected_frame_count, Frame* frame) {
  DCHECK_GE(expected_frame_count, 0);

  int actual_frame_count = 0;

  if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
    if (auto* portals = DocumentPortals::Get(*local_frame->GetDocument())) {
      actual_frame_count += static_cast<int>(portals->GetPortals().size());
    }
  }

  for (; frame; frame = frame->Tree().TraverseNext()) {
    ++actual_frame_count;

    // Check the ``DocumentFencedFrames`` on every local frame beneath
    // the ``frame`` to get an accurate count (i.e. if an iframe embeds
    // a fenced frame and creates a new ``DocumentFencedFrames`` object).
    if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
      if (auto* fenced_frames =
              DocumentFencedFrames::Get(*local_frame->GetDocument())) {
        actual_frame_count +=
            static_cast<int>(fenced_frames->GetFencedFrames().size());
      }
    }
  }

  DCHECK_EQ(expected_frame_count, actual_frame_count);
}
#endif

int Page::SubframeCount() const {
#if DCHECK_IS_ON()
  CheckFrameCountConsistency(subframe_count_ + 1, MainFrame());
#endif
  return subframe_count_;
}

void Page::SettingsChanged(ChangeType change_type) {
  switch (change_type) {
    case ChangeType::kStyle:
      InitialStyleChanged();
      break;
    case ChangeType::kViewportDescription:
      if (MainFrame() && MainFrame()->IsLocalFrame()) {
        DeprecatedLocalMainFrame()
            ->GetDocument()
            ->GetViewportData()
            .UpdateViewportDescription();
        // The text autosizer has dependencies on the viewport. Viewport
        // description only applies to the main frame. On a viewport description
        // change; any changes will be calculated starting from the local main
        // frame renderer and propagated to the OOPIF renderers.
        TextAutosizer::UpdatePageInfoInAllFrames(MainFrame());
      }
      break;
    case ChangeType::kViewportPaintProperties:
      if (GetVisualViewport().IsActiveViewport()) {
        GetVisualViewport().SetNeedsPaintPropertyUpdate();
        GetVisualViewport().InitializeScrollbars();
      }
      if (auto* local_frame = DynamicTo<LocalFrame>(MainFrame())) {
        if (LocalFrameView* view = local_frame->View())
          view->SetNeedsPaintPropertyUpdate();
      }
      break;
    case ChangeType::kDNSPrefetching:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame))
          local_frame->GetDocument()->InitDNSPrefetch();
      }
      break;
    case ChangeType::kImageLoading:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
          local_frame->GetDocument()->Fetcher()->SetImagesEnabled(
              GetSettings().GetImagesEnabled());
          local_frame->GetDocument()->Fetcher()->SetAutoLoadImages(
              GetSettings().GetLoadsImagesAutomatically());
        }
      }
      break;
    case ChangeType::kTextAutosizing:
      if (!MainFrame())
        break;
      // We need to update even for remote main frames since this setting
      // could be changed via InternalSettings.
      TextAutosizer::UpdatePageInfoInAllFrames(MainFrame());
      break;
    case ChangeType::kFontFamily:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame))
          local_frame->GetDocument()
              ->GetStyleEngine()
              .UpdateGenericFontFamilySettings();
      }
      break;
    case ChangeType::kAcceleratedCompositing:
      UpdateAcceleratedCompositingSettings();
      break;
    case ChangeType::kMediaQuery:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
          local_frame->GetDocument()->MediaQueryAffectingValueChanged(
              MediaValueChange::kOther);
        }
      }
      break;
    case ChangeType::kAccessibilityState:
      if (!MainFrame() || !MainFrame()->IsLocalFrame()) {
        break;
      }
      DeprecatedLocalMainFrame()->GetDocument()->RefreshAccessibilityTree();
      break;
    case ChangeType::kViewportStyle: {
      auto* main_local_frame = DynamicTo<LocalFrame>(MainFrame());
      if (!main_local_frame)
        break;
      if (Document* doc = main_local_frame->GetDocument())
        doc->GetStyleEngine().ViewportStyleSettingChanged();
      break;
    }
    case ChangeType::kTextTrackKindUserPreference:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
          Document* doc = local_frame->GetDocument();
          if (doc)
            HTMLMediaElement::SetTextTrackKindUserPreferenceForAllMediaElements(
                doc);
        }
      }
      break;
    case ChangeType::kDOMWorlds: {
      if (!GetSettings().GetForceMainWorldInitialization())
        break;
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* window = DynamicTo<LocalDOMWindow>(frame->DomWindow())) {
          // Forcibly instantiate WindowProxy.
          window->GetScriptController().WindowProxy(
              DOMWrapperWorld::MainWorld());
        }
      }
      break;
    }
    case ChangeType::kMediaControls:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        auto* local_frame = DynamicTo<LocalFrame>(frame);
        if (!local_frame)
          continue;
        Document* doc = local_frame->GetDocument();
        if (doc)
          HTMLMediaElement::OnMediaControlsEnabledChange(doc);
      }
      break;
    case ChangeType::kPlugins: {
      NotifyPluginsChanged();
      break;
    }
    case ChangeType::kHighlightAds: {
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame))
          local_frame->UpdateAdHighlight();
      }
      break;
    }
    case ChangeType::kPaint: {
      InvalidatePaint();
      break;
    }
    case ChangeType::kScrollbarLayout: {
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        auto* local_frame = DynamicTo<LocalFrame>(frame);
        if (!local_frame)
          continue;
        // Iterate through all of the scrollable areas and mark their layout
        // objects for layout.
        if (LocalFrameView* view = local_frame->View()) {
          if (const auto* scrollable_areas = view->UserScrollableAreas()) {
            for (const auto& scrollable_area : *scrollable_areas) {
              if (scrollable_area->ScrollsOverflow()) {
                if (auto* layout_box = scrollable_area->GetLayoutBox()) {
                  layout_box->SetNeedsLayout(
                      layout_invalidation_reason::kScrollbarChanged);
                }
              }
            }
          }
        }
      }
      break;
    }
    case ChangeType::kColorScheme:
      InvalidateColorScheme();
      break;
    case ChangeType::kSpatialNavigation:
      if (spatial_navigation_controller_ ||
          GetSettings().GetSpatialNavigationEnabled()) {
        GetSpatialNavigationController().OnSpatialNavigationSettingChanged();
      }
      break;
    case ChangeType::kUniversalAccess: {
      if (!GetSettings().GetAllowUniversalAccessFromFileURLs())
        break;
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        // If we got granted universal access from file urls we need to grant
        // any outstanding security origin cross agent cluster access since
        // newly allocated agent clusters will be the universal agent.
        if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
          auto* window = local_frame->DomWindow();
          window->GetMutableSecurityOrigin()->GrantCrossAgentClusterAccess();
        }
      }
      break;
    }
    case ChangeType::kVisionDeficiency: {
      if (auto* main_local_frame = DynamicTo<LocalFrame>(MainFrame()))
        main_local_frame->GetDocument()->VisionDeficiencyChanged();
      break;
    }
  }
}

void Page::InvalidateColorScheme() {
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame))
      local_frame->GetDocument()->ColorSchemeChanged();
  }
}

void Page::InvalidatePaint() {
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    if (LayoutView* view = local_frame->ContentLayoutObject())
      view->InvalidatePaintForViewAndDescendants();
  }
}

void Page::NotifyPluginsChanged() const {
  HeapVector<Member<PluginsChangedObserver>, 32> observers(
      plugins_changed_observers_);
  for (PluginsChangedObserver* observer : observers)
    observer->PluginsChanged();
}

void Page::UpdateAcceleratedCompositingSettings() {
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    // Mark all scrollable areas as needing a paint property update because the
    // compositing reasons may have changed.
    if (const auto* areas = local_frame->View()->UserScrollableAreas()) {
      for (const auto& scrollable_area : *areas) {
        if (scrollable_area->ScrollsOverflow()) {
          if (auto* layout_box = scrollable_area->GetLayoutBox())
            layout_box->SetNeedsPaintPropertyUpdate();
        }
      }
    }
  }
}

void Page::DidCommitLoad(LocalFrame* frame) {
  if (main_frame_ == frame) {
    GetConsoleMessageStorage().Clear();
    GetInspectorIssueStorage().Clear();
    // TODO(loonybear): Most of this doesn't appear to take into account that
    // each SVGImage gets it's own Page instance.
    GetDeprecation().ClearSuppression();
    // Need to reset visual viewport position here since before commit load we
    // would update the previous history item, Page::didCommitLoad is called
    // after a new history item is created in FrameLoader.
    // See crbug.com/642279
    GetVisualViewport().SetScrollOffset(ScrollOffset(),
                                        mojom::blink::ScrollType::kProgrammatic,
                                        mojom::blink::ScrollBehavior::kInstant,
                                        ScrollableArea::ScrollCallback());
  }
  // crbug/1312107: If DevTools has "Highlight ad frames" checked when the
  // main frame is refreshed or the ad frame is navigated to a different
  // process, DevTools calls `Settings::SetHighlightAds` so early that the
  // local frame is still in provisional state (not swapped in). Explicitly
  // invalidate the settings here as `Page::DidCommitLoad` is only fired after
  // the navigation is committed, at which point the local frame must already
  // be swapped-in.
  //
  // This explicit update is placed outside the above if-block to accommodate
  // iframes. The iframes share the same Page (frame tree) as the main frame,
  // but local frame swap can happen to any of the iframes.
  //
  // TODO(crbug/1357763): Properly apply the settings when the local frame
  // becomes the main frame of the page (i.e. when the navigation is
  // committed).
  frame->UpdateAdHighlight();
  GetLinkHighlight().ResetForPageNavigation();
}

void Page::AcceptLanguagesChanged() {
  HeapVector<Member<LocalFrame>> frames;

  // Even though we don't fire an event from here, the LocalDOMWindow's will
  // fire an event so we keep the frames alive until we are done.
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame))
      frames.push_back(local_frame);
  }

  for (unsigned i = 0; i < frames.size(); ++i)
    frames[i]->DomWindow()->AcceptLanguagesChanged();
}

void Page::Trace(Visitor* visitor) const {
  visitor->Trace(animator_);
  visitor->Trace(autoscroll_controller_);
  visitor->Trace(chrome_client_);
  visitor->Trace(drag_caret_);
  visitor->Trace(drag_controller_);
  visitor->Trace(focus_controller_);
  visitor->Trace(context_menu_controller_);
  visitor->Trace(page_scale_constraints_set_);
  visitor->Trace(page_visibility_observer_set_);
  visitor->Trace(pointer_lock_controller_);
  visitor->Trace(scrolling_coordinator_);
  visitor->Trace(browser_controls_);
  visitor->Trace(console_message_storage_);
  visitor->Trace(global_root_scroller_controller_);
  visitor->Trace(visual_viewport_);
  visitor->Trace(overscroll_controller_);
  visitor->Trace(link_highlight_);
  visitor->Trace(spatial_navigation_controller_);
  visitor->Trace(main_frame_);
  visitor->Trace(previous_main_frame_for_local_swap_);
  visitor->Trace(plugin_data_);
  visitor->Trace(validation_message_client_);
  visitor->Trace(plugins_changed_observers_);
  visitor->Trace(next_related_page_);
  visitor->Trace(prev_related_page_);
  visitor->Trace(agent_group_scheduler_);
  visitor->Trace(v8_compile_hints_);
  Supplementable<Page>::Trace(visitor);
}

void Page::DidInitializeCompositing(cc::AnimationHost& host) {
  GetLinkHighlight().AnimationHostInitialized(host);
}

void Page::WillStopCompositing() {
  GetLinkHighlight().WillCloseAnimationHost();
  // We may have disconnected the associated LayerTreeHost during
  // the frame lifecycle so ensure the PageAnimator is reset to the
  // default state.
  animator_->SetSuppressFrameRequestsWorkaroundFor704763Only(false);
}

void Page::WillBeDestroyed() {
  Frame* main_frame = main_frame_;

  // TODO(https://crbug.com/838348): Sadly, there are situations where Blink may
  // attempt to detach a main frame twice due to a bug. That rewinds
  // FrameLifecycle from kDetached to kDetaching, but GetPage() will already be
  // null. Since Detach() has already happened, just skip the actual Detach()
  // call to try to limit the side effects of this bug on the rest of frame
  // detach.
  if (main_frame->GetPage()) {
    main_frame->Detach(FrameDetachType::kRemove);
  }

  DCHECK(AllPages().Contains(this));
  AllPages().erase(this);
  OrdinaryPages().erase(this);

  {
    // Before: ... -> prev -> this -> next -> ...
    // After: ... -> prev -> next -> ...
    // (this is ok even if |this| is the only element on the list).
    Page* prev = prev_related_page_;
    Page* next = next_related_page_;
    next->prev_related_page_ = prev;
    prev->next_related_page_ = next;
    prev_related_page_ = nullptr;
    next_related_page_ = nullptr;
  }

  if (scrolling_coordinator_)
    scrolling_coordinator_->WillBeDestroyed();

  GetChromeClient().ChromeDestroyed();
  if (validation_message_client_)
    validation_message_client_->WillBeDestroyed();
  main_frame_ = nullptr;

  for (auto observer : page_visibility_observer_set_) {
    observer->ObserverSetWillBeCleared();
  }
  page_visibility_observer_set_.clear();

  page_scheduler_ = nullptr;
}

void Page::RegisterPluginsChangedObserver(PluginsChangedObserver* observer) {
  plugins_changed_observers_.insert(observer);
}

ScrollbarTheme& Page::GetScrollbarTheme() const {
  if (settings_->GetForceAndroidOverlayScrollbar())
    return ScrollbarThemeOverlayMobile::GetInstance();

  // Ensures that renderer preferences are set.
  DCHECK(main_frame_);
  return ScrollbarTheme::GetTheme();
}

AgentGroupScheduler& Page::GetAgentGroupScheduler() const {
  return *agent_group_scheduler_;
}

PageScheduler* Page::GetPageScheduler() const {
  DCHECK(page_scheduler_);
  return page_scheduler_.get();
}

bool Page::IsOrdinary() const {
  return is_ordinary_;
}

void Page::ReportIntervention(const String& text) {
  if (LocalFrame* local_frame = DeprecatedLocalMainFrame()) {
    auto* message = MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning, text,
        std::make_unique<SourceLocation>(String(), String(), 0, 0, nullptr));
    local_frame->GetDocument()->AddConsoleMessage(message);
  }
}

bool Page::RequestBeginMainFrameNotExpected(bool new_state) {
  if (!main_frame_ || !main_frame_->IsLocalFrame())
    return false;

  chrome_client_->RequestBeginMainFrameNotExpected(*DeprecatedLocalMainFrame(),
                                                   new_state);
  return true;
}

void Page::AddAutoplayFlags(int32_t value) {
  autoplay_flags_ |= value;
}

void Page::ClearAutoplayFlags() {
  autoplay_flags_ = 0;
}

int32_t Page::AutoplayFlags() const {
  return autoplay_flags_;
}

void Page::SetInsidePortal(bool inside_portal) {
  if (inside_portal_ == inside_portal)
    return;

  inside_portal_ = inside_portal;

  if (MainFrame() && MainFrame()->IsLocalFrame())
    DeprecatedLocalMainFrame()->PortalStateChanged();
}

bool Page::InsidePortal() const {
  return inside_portal_;
}

void Page::SetIsMainFrameFencedFrameRoot() {
  is_fenced_frame_tree_ = true;
}

bool Page::IsMainFrameFencedFrameRoot() const {
  return is_fenced_frame_tree_;
}

void Page::SetMediaFeatureOverride(const AtomicString& media_feature,
                                   const String& value) {
  if (!media_feature_overrides_) {
    if (value.empty())
      return;
    media_feature_overrides_ = std::make_unique<MediaFeatureOverrides>();
  }
  media_feature_overrides_->SetOverride(media_feature, value);
  if (media_feature == "prefers-color-scheme" ||
      media_feature == "forced-colors")
    SettingsChanged(ChangeType::kColorScheme);
  else
    SettingsChanged(ChangeType::kMediaQuery);
}

void Page::ClearMediaFeatureOverrides() {
  media_feature_overrides_.reset();
  SettingsChanged(ChangeType::kMediaQuery);
  SettingsChanged(ChangeType::kColorScheme);
}

void Page::SetVisionDeficiency(VisionDeficiency new_vision_deficiency) {
  if (new_vision_deficiency != vision_deficiency_) {
    vision_deficiency_ = new_vision_deficiency;
    SettingsChanged(ChangeType::kVisionDeficiency);
  }
}

void Page::Animate(base::TimeTicks monotonic_frame_begin_time) {
  GetAutoscrollController().Animate();
  Animator().ServiceScriptedAnimations(monotonic_frame_begin_time);
  // The ValidationMessage overlay manages its own internal Page that isn't
  // hooked up the normal BeginMainFrame flow, so we manually tick its
  // animations here.
  GetValidationMessageClient().ServiceScriptedAnimations(
      monotonic_frame_begin_time);
}

void Page::UpdateLifecycle(LocalFrame& root,
                           WebLifecycleUpdate requested_update,
                           DocumentUpdateReason reason) {
  if (requested_update == WebLifecycleUpdate::kLayout) {
    Animator().UpdateLifecycleToLayoutClean(root, reason);
  } else if (requested_update == WebLifecycleUpdate::kPrePaint) {
    Animator().UpdateLifecycleToPrePaintClean(root, reason);
  } else {
    Animator().UpdateAllLifecyclePhases(root, reason);
  }
}

const base::UnguessableToken& Page::BrowsingContextGroupToken() {
  return browsing_context_group_info_.browsing_context_group_token;
}

const base::UnguessableToken& Page::CoopRelatedGroupToken() {
  return browsing_context_group_info_.coop_related_group_token;
}

void Page::UpdateBrowsingContextGroup(
    const blink::BrowsingContextGroupInfo& browsing_context_group_info) {
  browsing_context_group_info_ = browsing_context_group_info;
}

template class CORE_TEMPLATE_EXPORT Supplement<Page>;

const char InternalSettingsPageSupplementBase::kSupplementName[] =
    "InternalSettings";

// static
void Page::PrepareForLeakDetection() {
  // Internal settings are ScriptWrappable and thus may retain documents
  // depending on whether the garbage collector(s) are able to find the settings
  // object through the Page supplement. Prepares for leak detection by removing
  // all InternalSetting objects from Pages.
  for (Page* page : OrdinaryPages()) {
    page->RemoveSupplement<InternalSettingsPageSupplementBase>();

    // V8CompileHintsProducer keeps v8::Script objects alive until the page
    // becomes interactive. Give it a chance to clean up.
    page->v8_compile_hints_->ClearData();
  }
}

// Ensure the 10 bits reserved for connected frame count in NodeRareData are
// sufficient.
static_assert(kMaxNumberOfFrames <
                  (1 << NodeRareData::kConnectedFrameCountBits),
              "Frame limit should fit in rare data count");
static_assert(kTenFrames < kMaxNumberOfFrames,
              "Reduced frame limit for testing should actually be lower");

}  // namespace blink
