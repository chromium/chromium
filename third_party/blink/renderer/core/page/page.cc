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

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/visited_link_state.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/execution_context/agent_metrics_collector.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/dom_timer.h"
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
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
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
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

// Wrapper function defined in WebKit.h
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

static AgentMetricsCollector& GlobalAgentMetricsCollector() {
  DEFINE_STATIC_LOCAL(Persistent<AgentMetricsCollector>, metrics_collector,
                      (MakeGarbageCollected<AgentMetricsCollector>()));
  return *metrics_collector;
}

void Page::InsertOrdinaryPageForTesting(Page* page) {
  OrdinaryPages().insert(page);
}

HeapVector<Member<Page>> Page::RelatedPages() {
  HeapVector<Member<Page>> result;
  Page* ptr = this->next_related_page_;
  while (ptr != this) {
    result.push_back(ptr);
    ptr = ptr->next_related_page_;
  }
  return result;
}

float DeviceScaleFactorDeprecated(LocalFrame* frame) {
  if (!frame)
    return 1;
  Page* page = frame->GetPage();
  if (!page)
    return 1;
  return page->DeviceScaleFactorDeprecated();
}

Page* Page::CreateNonOrdinary(PageClients& page_clients) {
  Page* page = MakeGarbageCollected<Page>(page_clients);
  page->SetPageScheduler(ThreadScheduler::Current()->CreatePageScheduler(page));
  return page;
}

Page* Page::CreateOrdinary(PageClients& page_clients, Page* opener) {
  Page* page = MakeGarbageCollected<Page>(page_clients);
  page->is_ordinary_ = true;
  page->agent_metrics_collector_ = &GlobalAgentMetricsCollector();
  page->SetPageScheduler(ThreadScheduler::Current()->CreatePageScheduler(page));

  if (opener) {
    // Before: ... -> opener -> next -> ...
    // After: ... -> opener -> page -> next -> ...
    Page* next = opener->next_related_page_;
    opener->next_related_page_ = page;
    page->prev_related_page_ = opener;
    page->next_related_page_ = next;
    next->prev_related_page_ = page;

    // No need to update |prev| here as if |next| != |prev|, |prev| was already
    // marked as having related pages.
    next->UpdateHasRelatedPages();
    page->UpdateHasRelatedPages();
  }

  OrdinaryPages().insert(page);
  if (ScopedPagePauser::IsActive())
    page->SetPaused(true);
  return page;
}

Page::Page(PageClients& page_clients)
    : SettingsDelegate(std::make_unique<Settings>()),
      main_frame_(nullptr),
      animator_(MakeGarbageCollected<PageAnimator>(*this)),
      autoscroll_controller_(MakeGarbageCollected<AutoscrollController>(*this)),
      chrome_client_(page_clients.chrome_client),
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
      paused_(false),
      device_scale_factor_(1),
      visibility_state_(PageVisibilityState::kVisible),
      is_ordinary_(false),
      page_lifecycle_state_(kDefaultPageLifecycleState),
      is_cursor_visible_(true),
      subframe_count_(0),
      next_related_page_(this),
      prev_related_page_(this),
      autoplay_flags_(0),
      web_text_autosizer_page_info_({0, 0, 1.f}) {
  DCHECK(!AllPages().Contains(this));
  AllPages().insert(this);
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
    main_local_frame->Loader().StopAllLoaders();

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
  // |has_related_pages_| is only reported when the main frame is local, so make
  // sure it's updated after the main frame changes.
  UpdateHasRelatedPages();
}

LocalFrame* Page::DeprecatedLocalMainFrame() const {
  return To<LocalFrame>(main_frame_.Get());
}

void Page::DocumentDetached(Document* document) {
  pointer_lock_controller_->DocumentDetached(document);
  context_menu_controller_->DocumentDetached(document);
  if (validation_message_client_)
    validation_message_client_->DocumentDetached(*document);
  hosts_using_features_.DocumentDetached(*document);

  if (agent_metrics_collector_)
    agent_metrics_collector_->DidDetachDocument(*document);

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

void Page::PlatformColorsChanged() {
  for (const Page* page : AllPages())
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (auto* local_frame = DynamicTo<LocalFrame>(frame))
        local_frame->GetDocument()->PlatformColorsChanged();
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

void Page::InitialStyleChanged() {
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    local_frame->GetDocument()->GetStyleEngine().InitialStyleChanged();
  }
}

PluginData* Page::GetPluginData(const SecurityOrigin* main_frame_origin) {
  if (!plugin_data_)
    plugin_data_ = MakeGarbageCollected<PluginData>();

  if (!plugin_data_->Origin() ||
      !main_frame_origin->IsSameSchemeHostPort(plugin_data_->Origin()))
    plugin_data_->UpdatePluginList(main_frame_origin);

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
            ToSVGImageChromeClientOrNull(page->GetChromeClient()))
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
  mojom::FrameLifecycleState state = paused
                                         ? mojom::FrameLifecycleState::kPaused
                                         : mojom::FrameLifecycleState::kRunning;
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame))
      local_frame->SetLifecycleState(state);
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

void Page::SetDeviceScaleFactorDeprecated(float scale_factor) {
  if (device_scale_factor_ == scale_factor)
    return;

  device_scale_factor_ = scale_factor;

  if (MainFrame() && MainFrame()->IsLocalFrame())
    DeprecatedLocalMainFrame()->DeviceScaleFactorChanged();
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

void Page::SetVisibilityState(PageVisibilityState visibility_state,
                              bool is_initial_state) {
  if (visibility_state_ == visibility_state)
    return;
  visibility_state_ = visibility_state;

  if (is_initial_state)
    return;

  NotifyPageVisibilityChanged();
  if (main_frame_) {
    if (visibility_state_ == PageVisibilityState::kVisible)
      RestoreSVGImageAnimations();
    main_frame_->DidChangeVisibilityState();
  }
}

PageVisibilityState Page::GetVisibilityState() const {
  return visibility_state_;
}

bool Page::IsPageVisible() const {
  return visibility_state_ == PageVisibilityState::kVisible;
}

void Page::SetLifecycleState(PageLifecycleState state) {
  if (state == page_lifecycle_state_)
    return;
  DCHECK_NE(state, PageLifecycleState::kUnknown);

  base::Optional<mojom::FrameLifecycleState> next_state;
  if (state == PageLifecycleState::kFrozen) {
    next_state = mojom::FrameLifecycleState::kFrozen;
  } else if (page_lifecycle_state_ == PageLifecycleState::kFrozen) {
    // TODO(fmeawad): Only resume the page that just became visible, blocked
    // on task queues per frame.
    DCHECK(state == PageLifecycleState::kActive ||
           state == PageLifecycleState::kHiddenBackgrounded ||
           state == PageLifecycleState::kHiddenForegrounded);
    next_state = mojom::FrameLifecycleState::kRunning;
  }

  if (next_state) {
    for (Frame* frame = main_frame_.Get(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
        // TODO(chrisha): Determine if dispatching the before unload
        // makes sense and if so put it into a specification.
        if (next_state == mojom::FrameLifecycleState::kFrozen)
          local_frame->DispatchBeforeUnloadEventForFreeze();
        local_frame->SetLifecycleState(next_state.value());
      }
    }
  }
  page_lifecycle_state_ = state;
}

PageLifecycleState Page::LifecycleState() const {
  return page_lifecycle_state_;
}

bool Page::IsCursorVisible() const {
  return is_cursor_visible_;
}

#if DCHECK_IS_ON()
void CheckFrameCountConsistency(int expected_frame_count, Frame* frame) {
  DCHECK_GE(expected_frame_count, 0);

  int actual_frame_count = 0;
  for (; frame; frame = frame->Tree().TraverseNext())
    ++actual_frame_count;

  DCHECK_EQ(expected_frame_count, actual_frame_count);
}
#endif

int Page::SubframeCount() const {
#if DCHECK_IS_ON()
  CheckFrameCountConsistency(subframe_count_ + 1, MainFrame());
#endif
  return subframe_count_;
}

void Page::SettingsChanged(SettingsDelegate::ChangeType change_type) {
  switch (change_type) {
    case SettingsDelegate::kStyleChange:
      InitialStyleChanged();
      break;
    case SettingsDelegate::kViewportDescriptionChange:
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
    case SettingsDelegate::kViewportScrollbarChange:
      GetVisualViewport().InitializeScrollbars();
      break;
    case SettingsDelegate::kDNSPrefetchingChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame))
          local_frame->GetDocument()->InitDNSPrefetch();
      }
      break;
    case SettingsDelegate::kImageLoadingChange:
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
    case SettingsDelegate::kTextAutosizingChange:
      if (!MainFrame())
        break;
      // We need to update even for remote main frames since this setting
      // could be changed via InternalSettings.
      TextAutosizer::UpdatePageInfoInAllFrames(MainFrame());
      break;
    case SettingsDelegate::kFontFamilyChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame))
          local_frame->GetDocument()
              ->GetStyleEngine()
              .UpdateGenericFontFamilySettings();
      }
      break;
    case SettingsDelegate::kAcceleratedCompositingChange:
      UpdateAcceleratedCompositingSettings();
      break;
    case SettingsDelegate::kMediaQueryChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame))
          local_frame->GetDocument()->MediaQueryAffectingValueChanged();
      }
      break;
    case SettingsDelegate::kAccessibilityStateChange:
      if (!MainFrame() || !MainFrame()->IsLocalFrame())
        break;
      DeprecatedLocalMainFrame()
          ->GetDocument()
          ->AXObjectCacheOwner()
          .ClearAXObjectCache();
      break;
    case SettingsDelegate::kViewportRuleChange: {
      auto* main_local_frame = DynamicTo<LocalFrame>(MainFrame());
      if (!main_local_frame)
        break;
      if (Document* doc = main_local_frame->GetDocument())
        doc->GetStyleEngine().ViewportRulesChanged();
      break;
    }
    case SettingsDelegate::kTextTrackKindUserPreferenceChange:
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
    case SettingsDelegate::kDOMWorldsChange: {
      if (!GetSettings().GetForceMainWorldInitialization())
        break;
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        LocalFrame* local_frame = nullptr;
        if ((local_frame = DynamicTo<LocalFrame>(frame)) &&
            !local_frame->Loader()
                 .StateMachine()
                 ->CreatingInitialEmptyDocument()) {
          // Forcibly instantiate WindowProxy.
          local_frame->GetScriptController().WindowProxy(
              DOMWrapperWorld::MainWorld());
        }
      }
      break;
    }
    case SettingsDelegate::kMediaControlsChange:
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
    case SettingsDelegate::kPluginsChange: {
      NotifyPluginsChanged();
      break;
    }
    case SettingsDelegate::kHighlightAdsChange: {
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame))
          local_frame->UpdateAdHighlight();
      }
      break;
    }
    case SettingsDelegate::kPaintChange: {
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        auto* local_frame = DynamicTo<LocalFrame>(frame);
        if (!local_frame)
          continue;
        if (LayoutView* view = local_frame->ContentLayoutObject())
          view->InvalidatePaintForViewAndCompositedLayers();
      }
      break;
    }
    case SettingsDelegate::kScrollbarLayoutChange: {
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        auto* local_frame = DynamicTo<LocalFrame>(frame);
        if (!local_frame)
          continue;
        // Iterate through all of the scrollable areas and mark their layout
        // objects for layout.
        if (LocalFrameView* view = local_frame->View()) {
          if (const auto* scrollable_areas = view->ScrollableAreas()) {
            for (const auto& scrollable_area : *scrollable_areas) {
              if (auto* layout_box = scrollable_area->GetLayoutBox()) {
                layout_box->SetNeedsLayout(
                    layout_invalidation_reason::kScrollbarChanged);
              }
            }
          }
        }
      }
      break;
    }
    case SettingsDelegate::kColorSchemeChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (auto* local_frame = DynamicTo<LocalFrame>(frame))
          local_frame->GetDocument()->ColorSchemeChanged();
      }
      break;
    case SettingsDelegate::kSpatialNavigationChange:
      if (spatial_navigation_controller_ ||
          GetSettings().GetSpatialNavigationEnabled()) {
        GetSpatialNavigationController().OnSpatialNavigationSettingChanged();
      }
      break;
    case SettingsDelegate::kUniversalAccessChange: {
      if (!GetSettings().GetAllowUniversalAccessFromFileURLs())
        break;
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        // If we got granted universal access from file urls we need to grant
        // any outstanding security origin cross agent cluster access since
        // newly allocated agent clusters will be the universal agent.
        if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
          local_frame->GetDocument()
              ->GetMutableSecurityOrigin()
              ->GrantCrossAgentClusterAccess();
        }
      }
      break;
    }
  }
}

void Page::NotifyPluginsChanged() const {
  HeapVector<Member<PluginsChangedObserver>, 32> observers;
  CopyToVector(plugins_changed_observers_, observers);
  for (PluginsChangedObserver* observer : observers)
    observer->PluginsChanged();
}

void Page::UpdateAcceleratedCompositingSettings() {
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    LayoutView* layout_view = local_frame->ContentLayoutObject();
    layout_view->Compositor()->UpdateAcceleratedCompositingSettings();
  }
}

void Page::DidCommitLoad(LocalFrame* frame) {
  if (main_frame_ == frame) {
    GetConsoleMessageStorage().Clear();
    // TODO(loonybear): Most of this doesn't appear to take into account that
    // each SVGImage gets it's own Page instance.
    GetDeprecation().ClearSuppression();
    GetVisualViewport().SendUMAMetrics();
    // Need to reset visual viewport position here since before commit load we
    // would update the previous history item, Page::didCommitLoad is called
    // after a new history item is created in FrameLoader.
    // See crbug.com/642279
    GetVisualViewport().SetScrollOffset(ScrollOffset(), kProgrammaticScroll,
                                        kScrollBehaviorInstant,
                                        ScrollableArea::ScrollCallback());
    hosts_using_features_.UpdateMeasurementsAndClear();
    // Update |has_related_pages_| as features are reset after navigation.
    UpdateHasRelatedPages();
  }
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

void Page::Trace(blink::Visitor* visitor) {
  visitor->Trace(animator_);
  visitor->Trace(autoscroll_controller_);
  visitor->Trace(chrome_client_);
  visitor->Trace(drag_caret_);
  visitor->Trace(drag_controller_);
  visitor->Trace(focus_controller_);
  visitor->Trace(context_menu_controller_);
  visitor->Trace(page_scale_constraints_set_);
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
  visitor->Trace(plugin_data_);
  visitor->Trace(validation_message_client_);
  visitor->Trace(agent_metrics_collector_);
  visitor->Trace(plugins_changed_observers_);
  visitor->Trace(next_related_page_);
  visitor->Trace(prev_related_page_);
  Supplementable<Page>::Trace(visitor);
  PageVisibilityNotifier::Trace(visitor);
}

void Page::AnimationHostInitialized(cc::AnimationHost& animation_host,
                                    LocalFrameView* view) {
  if (GetScrollingCoordinator()) {
    GetScrollingCoordinator()->AnimationHostInitialized(animation_host, view);
  }
  GetLinkHighlight().AnimationHostInitialized(animation_host);
}

void Page::WillCloseAnimationHost(LocalFrameView* view) {
  if (scrolling_coordinator_)
    scrolling_coordinator_->WillCloseAnimationHost(view);
  GetLinkHighlight().WillCloseAnimationHost();
}

void Page::WillBeDestroyed() {
  Frame* main_frame = main_frame_;

  main_frame->Detach(FrameDetachType::kRemove);

  DCHECK(AllPages().Contains(this));
  AllPages().erase(this);
  OrdinaryPages().erase(this);

  {
    // Before: ... -> prev -> this -> next -> ...
    // After: ... -> prev -> next -> ...
    // (this is ok even if |this| is the only element on the list).
    Page* prev = this->prev_related_page_;
    Page* next = this->next_related_page_;
    next->prev_related_page_ = prev;
    prev->next_related_page_ = next;
    this->prev_related_page_ = nullptr;
    this->next_related_page_ = nullptr;
    if (prev != this)
      prev->UpdateHasRelatedPages();
    if (next != this)
      next->UpdateHasRelatedPages();
  }

  if (scrolling_coordinator_)
    scrolling_coordinator_->WillBeDestroyed();

  GetChromeClient().ChromeDestroyed();
  if (validation_message_client_)
    validation_message_client_->WillBeDestroyed();
  main_frame_ = nullptr;

  if (agent_metrics_collector_)
    agent_metrics_collector_->ReportMetrics();

  PageVisibilityNotifier::NotifyContextDestroyed();

  page_scheduler_.reset();
}

void Page::RegisterPluginsChangedObserver(PluginsChangedObserver* observer) {
  plugins_changed_observers_.insert(observer);
}

ScrollbarTheme& Page::GetScrollbarTheme() const {
  if (settings_->GetForceAndroidOverlayScrollbar())
    return ScrollbarThemeOverlayMobile::GetInstance();
  return ScrollbarTheme::GetTheme();
}

PageScheduler* Page::GetPageScheduler() const {
  return page_scheduler_.get();
}

void Page::SetPageScheduler(std::unique_ptr<PageScheduler> page_scheduler) {
  page_scheduler_ = std::move(page_scheduler);
  // The scheduler should be set before the main frame.
  DCHECK(!main_frame_);
  history_navigation_virtual_time_pauser_ =
      page_scheduler_->CreateWebScopedVirtualTimePauser(
          "HistoryNavigation",
          WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant);
}

bool Page::IsOrdinary() const {
  return is_ordinary_;
}

void Page::ReportIntervention(const String& text) {
  if (LocalFrame* local_frame = DeprecatedLocalMainFrame()) {
    ConsoleMessage* message = ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning, text,
        std::make_unique<SourceLocation>(String(), 0, 0, nullptr));
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

bool Page::LocalMainFrameNetworkIsAlmostIdle() const {
  LocalFrame* frame = DynamicTo<LocalFrame>(MainFrame());
  if (!frame)
    return true;
  return frame->GetIdlenessDetector()->NetworkIsAlmostIdle();
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
  inside_portal_ = inside_portal;
}

bool Page::InsidePortal() const {
  return inside_portal_;
}

void Page::UpdateHasRelatedPages() {
  bool has_related_pages = next_related_page_ != this;
  if (!has_related_pages) {
    has_related_pages_.reset();
  } else {
    LocalFrame* local_main_frame = DynamicTo<LocalFrame>(main_frame_.Get());
    // We want to record this only for the pages which have local main frame,
    // which is fine as we are aggregating results across all processes.
    if (!local_main_frame || !local_main_frame->IsAttached())
      return;
    has_related_pages_ = local_main_frame->GetFrameScheduler()->RegisterFeature(
        SchedulingPolicy::Feature::kHasScriptableFramesInMultipleTabs,
        {SchedulingPolicy::RecordMetricsForBackForwardCache()});
  }
}

void Page::SetMediaFeatureOverride(const AtomicString& media_feature,
                                   const String& value) {
  if (!media_feature_overrides_) {
    if (value.IsEmpty())
      return;
    media_feature_overrides_ = std::make_unique<MediaFeatureOverrides>();
  }
  media_feature_overrides_->SetOverride(media_feature, value);
  if (media_feature == "prefers-color-scheme")
    SettingsChanged(SettingsDelegate::kColorSchemeChange);
  else
    SettingsChanged(SettingsDelegate::kMediaQueryChange);
}

void Page::ClearMediaFeatureOverrides() {
  media_feature_overrides_.reset();
  SettingsChanged(SettingsDelegate::kMediaQueryChange);
  SettingsChanged(SettingsDelegate::kColorSchemeChange);
}

Page::PageClients::PageClients() : chrome_client(nullptr) {}

Page::PageClients::~PageClients() = default;

template class CORE_TEMPLATE_EXPORT Supplement<Page>;

const char InternalSettingsPageSupplementBase::kSupplementName[] =
    "InternalSettings";

// static
void Page::PrepareForLeakDetection() {
  // Internal settings are ScriptWrappable and thus may retain documents
  // depending on whether the garbage collector(s) are able to find the settings
  // object through the Page supplement. Prepares for leak detection by removing
  // all InternalSetting objects from Pages.
  for (Page* page : OrdinaryPages())
    page->RemoveSupplement<InternalSettingsPageSupplementBase>();
}

}  // namespace blink
