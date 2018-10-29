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
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/visited_link_state.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/dom_timer.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/link_highlights.h"
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
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page_overlay.h"
#include "third_party/blink/renderer/core/page/plugins_changed_observer.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/validation_message_client_impl.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/plugins/plugin_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

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
  DEFINE_STATIC_LOCAL(Persistent<Page::PageSet>, pages, (new Page::PageSet));
  return *pages;
}

Page::PageSet& Page::OrdinaryPages() {
  DEFINE_STATIC_LOCAL(Persistent<Page::PageSet>, pages, (new Page::PageSet));
  return *pages;
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

Page* Page::Create(PageClients& page_clients) {
  Page* page = new Page(page_clients);
  page->SetPageScheduler(
      Platform::Current()->CurrentThread()->Scheduler()->CreatePageScheduler(
          page));
  return page;
}

Page* Page::CreateOrdinary(PageClients& page_clients, Page* opener) {
  Page* page = Create(page_clients);

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

Page::Page(PageClients& page_clients)
    : SettingsDelegate(Settings::Create()),
      main_frame_(nullptr),
      animator_(PageAnimator::Create(*this)),
      autoscroll_controller_(AutoscrollController::Create(*this)),
      chrome_client_(page_clients.chrome_client),
      drag_caret_(DragCaret::Create()),
      drag_controller_(DragController::Create(this)),
      focus_controller_(FocusController::Create(this)),
      context_menu_controller_(ContextMenuController::Create(this)),
      page_scale_constraints_set_(PageScaleConstraintsSet::Create(this)),
      pointer_lock_controller_(PointerLockController::Create(this)),
      browser_controls_(BrowserControls::Create(*this)),
      console_message_storage_(new ConsoleMessageStorage()),
      global_root_scroller_controller_(
          TopDocumentRootScrollerController::Create(*this)),
      visual_viewport_(VisualViewport::Create(*this)),
      overscroll_controller_(
          OverscrollController::Create(GetVisualViewport(), GetChromeClient())),
      link_highlights_(LinkHighlights::Create(*this)),
      plugin_data_(nullptr),
      // TODO(pdr): Initialize |validation_message_client_| lazily.
      validation_message_client_(ValidationMessageClientImpl::Create(*this)),
      opened_by_dom_(false),
      tab_key_cycles_through_elements_(true),
      paused_(false),
      device_scale_factor_(1),
      visibility_state_(mojom::PageVisibilityState::kVisible),
      page_lifecycle_state_(kDefaultPageLifecycleState),
      is_cursor_visible_(true),
      subframe_count_(0),
      next_related_page_(this),
      prev_related_page_(this),
      autoplay_flags_(0) {
  DCHECK(!AllPages().Contains(this));
  AllPages().insert(this);
}

Page::~Page() {
  // willBeDestroyed() must be called before Page destruction.
  DCHECK(!main_frame_);
}

void Page::CloseSoon() {
  // Make sure this Page can no longer be found by JS.
  is_closing_ = true;

  // TODO(dcheng): Try to remove this in a followup, it's not obviously needed.
  if (main_frame_->IsLocalFrame())
    ToLocalFrame(main_frame_)->Loader().StopAllLoaders();

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
    scrolling_coordinator_ = ScrollingCoordinator::Create(this);

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

LinkHighlights& Page::GetLinkHighlights() {
  return *link_highlights_;
}

void Page::SetMainFrame(Frame* main_frame) {
  // Should only be called during initialization or swaps between local and
  // remote frames.
  // FIXME: Unfortunately we can't assert on this at the moment, because this
  // is called in the base constructor for both LocalFrame and RemoteFrame,
  // when the vtables for the derived classes have not yet been setup. Once this
  // is fixed, also call  page_scheduler_->SetIsMainFrameLocal() from here
  // instead of from the callers of this method.
  main_frame_ = main_frame;
}

LocalFrame* Page::DeprecatedLocalMainFrame() const {
  return ToLocalFrame(main_frame_);
}

void Page::DocumentDetached(Document* document) {
  pointer_lock_controller_->DocumentDetached(document);
  context_menu_controller_->DocumentDetached(document);
  if (validation_message_client_)
    validation_message_client_->DocumentDetached(*document);
  hosts_using_features_.DocumentDetached(*document);
}

bool Page::OpenedByDOM() const {
  return opened_by_dom_;
}

void Page::SetOpenedByDOM() {
  opened_by_dom_ = true;
}

void Page::PlatformColorsChanged() {
  for (const Page* page : AllPages())
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (frame->IsLocalFrame())
        ToLocalFrame(frame)->GetDocument()->PlatformColorsChanged();
    }
}

void Page::InitialStyleChanged() {
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    ToLocalFrame(frame)->GetDocument()->GetStyleEngine().InitialStyleChanged();
  }
}

PluginData* Page::GetPluginData(const SecurityOrigin* main_frame_origin) {
  if (!plugin_data_)
    plugin_data_ = PluginData::Create();

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
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    LocalFrame* local_frame = ToLocalFrame(frame);
    local_frame->Loader().SetDefersLoading(paused);
    local_frame->GetFrameScheduler()->SetPaused(paused);
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
      if (frame->IsLocalFrame())
        ToLocalFrame(frame)
            ->GetDocument()
            ->GetVisitedLinkState()
            .InvalidateStyleForAllLinks(invalidate_visited_link_hashes);
    }
  }
}

void Page::VisitedStateChanged(LinkHash link_hash) {
  for (const Page* page : OrdinaryPages()) {
    for (Frame* frame = page->main_frame_; frame;
         frame = frame->Tree().TraverseNext()) {
      if (frame->IsLocalFrame())
        ToLocalFrame(frame)
            ->GetDocument()
            ->GetVisitedLinkState()
            .InvalidateStyleForLink(link_hash);
    }
  }
}

void Page::SetVisibilityState(mojom::PageVisibilityState visibility_state,
                              bool is_initial_state) {
  if (visibility_state_ == visibility_state)
    return;
  visibility_state_ = visibility_state;

  if (is_initial_state)
    return;
  NotifyPageVisibilityChanged();

  if (main_frame_) {
    if (IsPageVisible())
      RestoreSVGImageAnimations();
    main_frame_->DidChangeVisibilityState();
  }
}

mojom::PageVisibilityState Page::VisibilityState() const {
  return visibility_state_;
}

bool Page::IsPageVisible() const {
  return VisibilityState() == mojom::PageVisibilityState::kVisible;
}

void Page::SetLifecycleState(PageLifecycleState state) {
  if (state == page_lifecycle_state_)
    return;
  DCHECK_NE(state, PageLifecycleState::kUnknown);

  if (RuntimeEnabledFeatures::PageLifecycleEnabled()) {
    if (state == PageLifecycleState::kFrozen) {
      for (Frame* frame = main_frame_.Get(); frame;
           frame = frame->Tree().TraverseNext()) {
        frame->DidFreeze();
      }
    } else if (page_lifecycle_state_ == PageLifecycleState::kFrozen) {
      // TODO(fmeawad): Only resume the page that just became visible, blocked
      // on task queues per frame.
      DCHECK(state == PageLifecycleState::kActive ||
             state == PageLifecycleState::kHiddenBackgrounded ||
             state == PageLifecycleState::kHiddenForegrounded);
      for (Frame* frame = main_frame_.Get(); frame;
           frame = frame->Tree().TraverseNext()) {
        frame->DidResume();
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
        // The text autosizer has dependencies on the viewport.
        if (TextAutosizer* text_autosizer =
                DeprecatedLocalMainFrame()->GetDocument()->GetTextAutosizer())
          text_autosizer->UpdatePageInfoInAllFrames();
      }
      break;
    case SettingsDelegate::kViewportScrollbarChange:
      GetVisualViewport().InitializeScrollbars();
      break;
    case SettingsDelegate::kDNSPrefetchingChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (frame->IsLocalFrame())
          ToLocalFrame(frame)->GetDocument()->InitDNSPrefetch();
      }
      break;
    case SettingsDelegate::kImageLoadingChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (frame->IsLocalFrame()) {
          ToLocalFrame(frame)->GetDocument()->Fetcher()->SetImagesEnabled(
              GetSettings().GetImagesEnabled());
          ToLocalFrame(frame)->GetDocument()->Fetcher()->SetAutoLoadImages(
              GetSettings().GetLoadsImagesAutomatically());
        }
      }
      break;
    case SettingsDelegate::kTextAutosizingChange:
      if (!MainFrame() || !MainFrame()->IsLocalFrame())
        break;
      if (TextAutosizer* text_autosizer =
              DeprecatedLocalMainFrame()->GetDocument()->GetTextAutosizer())
        text_autosizer->UpdatePageInfoInAllFrames();
      break;
    case SettingsDelegate::kFontFamilyChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (frame->IsLocalFrame())
          ToLocalFrame(frame)
              ->GetDocument()
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
        if (frame->IsLocalFrame())
          ToLocalFrame(frame)->GetDocument()->MediaQueryAffectingValueChanged();
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
      if (!MainFrame() || !MainFrame()->IsLocalFrame())
        break;
      if (Document* doc = ToLocalFrame(MainFrame())->GetDocument())
        doc->GetStyleEngine().ViewportRulesChanged();
      break;
    }
    case SettingsDelegate::kTextTrackKindUserPreferenceChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (frame->IsLocalFrame()) {
          Document* doc = ToLocalFrame(frame)->GetDocument();
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
        if (!frame->IsLocalFrame())
          continue;
        LocalFrame* local_frame = ToLocalFrame(frame);
        if (!local_frame->Loader()
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
        if (!frame->IsLocalFrame())
          continue;
        Document* doc = ToLocalFrame(frame)->GetDocument();
        if (doc)
          HTMLMediaElement::OnMediaControlsEnabledChange(doc);
      }
      break;
    case SettingsDelegate::kPluginsChange: {
      NotifyPluginsChanged();
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
    if (!frame->IsLocalFrame())
      continue;
    if (LocalFrameView* view = ToLocalFrame(frame)->View())
      view->UpdateAcceleratedCompositingSettings();
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
    GetVisualViewport().SetScrollOffset(ScrollOffset(), kProgrammaticScroll);
    hosts_using_features_.UpdateMeasurementsAndClear();
  }
  GetLinkHighlights().ResetForPageNavigation();
}

void Page::AcceptLanguagesChanged() {
  HeapVector<Member<LocalFrame>> frames;

  // Even though we don't fire an event from here, the LocalDOMWindow's will
  // fire an event so we keep the frames alive until we are done.
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (frame->IsLocalFrame())
      frames.push_back(ToLocalFrame(frame));
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
  visitor->Trace(link_highlights_);
  visitor->Trace(main_frame_);
  visitor->Trace(plugin_data_);
  visitor->Trace(validation_message_client_);
  visitor->Trace(plugins_changed_observers_);
  visitor->Trace(next_related_page_);
  visitor->Trace(prev_related_page_);
  Supplementable<Page>::Trace(visitor);
  PageVisibilityNotifier::Trace(visitor);
}

void Page::LayerTreeViewInitialized(WebLayerTreeView& layer_tree_view,
                                    LocalFrameView* view) {
  if (GetScrollingCoordinator())
    GetScrollingCoordinator()->LayerTreeViewInitialized(layer_tree_view, view);
  GetLinkHighlights().LayerTreeViewInitialized(layer_tree_view);
}

void Page::WillCloseLayerTreeView(WebLayerTreeView& layer_tree_view,
                                  LocalFrameView* view) {
  if (scrolling_coordinator_)
    scrolling_coordinator_->WillCloseLayerTreeView(layer_tree_view, view);
  GetLinkHighlights().WillCloseLayerTreeView(layer_tree_view);
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
  }

  if (scrolling_coordinator_)
    scrolling_coordinator_->WillBeDestroyed();

  GetChromeClient().ChromeDestroyed();
  if (validation_message_client_)
    validation_message_client_->WillBeDestroyed();
  main_frame_ = nullptr;

  PageVisibilityNotifier::NotifyContextDestroyed();

  page_scheduler_.reset();
}

void Page::RegisterPluginsChangedObserver(PluginsChangedObserver* observer) {
  plugins_changed_observers_.insert(observer);
}

ScrollbarTheme& Page::GetScrollbarTheme() const {
  if (settings_->GetForceAndroidOverlayScrollbar())
    return ScrollbarThemeOverlay::MobileTheme();
  return ScrollbarTheme::DeprecatedStaticGetTheme();
}

PageScheduler* Page::GetPageScheduler() const {
  return page_scheduler_.get();
}

void Page::SetPageScheduler(std::unique_ptr<PageScheduler> page_scheduler) {
  page_scheduler_ = std::move(page_scheduler);
  // The scheduler should be set before the main frame.
  DCHECK(!main_frame_);
}

void Page::ReportIntervention(const String& text) {
  if (LocalFrame* local_frame = DeprecatedLocalMainFrame()) {
    ConsoleMessage* message =
        ConsoleMessage::Create(kOtherMessageSource, kWarningMessageLevel, text,
                               SourceLocation::Create(String(), 0, 0, nullptr));
    local_frame->GetDocument()->AddConsoleMessage(message);
  }
}

bool Page::RequestBeginMainFrameNotExpected(bool new_state) {
  if (!main_frame_ || !main_frame_->IsLocalFrame())
    return false;

  base::debug::StackTrace main_frame_created_trace =
      main_frame_->CreateStackForDebugging();
  base::debug::Alias(&main_frame_created_trace);
  base::debug::StackTrace main_frame_detached_trace =
      main_frame_->DetachStackForDebugging();
  base::debug::Alias(&main_frame_detached_trace);
  CHECK(main_frame_->IsAttached());
  if (LocalFrame* main_frame = DeprecatedLocalMainFrame()) {
    if (WebLayerTreeView* layer_tree_view =
            chrome_client_->GetWebLayerTreeView(main_frame)) {
      layer_tree_view->RequestBeginMainFrameNotExpected(new_state);
      return true;
    }
  }
  return false;
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

namespace {

class ColorOverlay final : public PageOverlay::Delegate {
 public:
  explicit ColorOverlay(SkColor color) : color_(color) {}

 private:
  void PaintPageOverlay(const PageOverlay& page_overlay,
                        GraphicsContext& graphics_context,
                        const IntSize& size) const override {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            graphics_context, page_overlay, DisplayItem::kPageOverlay))
      return;
    FloatRect rect(0, 0, size.Width(), size.Height());
    DrawingRecorder recorder(graphics_context, page_overlay,
                             DisplayItem::kPageOverlay);
    graphics_context.FillRect(rect, color_);
  }

  SkColor color_;
};

}  // namespace

void Page::SetPageOverlayColor(SkColor color) {
  if (page_color_overlay_)
    page_color_overlay_.reset();

  if (color == Color::kTransparent)
    return;

  if (!MainFrame() || !MainFrame()->IsLocalFrame())
    return;
  auto* local_frame = ToLocalFrame(MainFrame());
  page_color_overlay_ =
      PageOverlay::Create(local_frame, std::make_unique<ColorOverlay>(color));

  // Update compositing which will create graphics layers so the page color
  // update below will be able to attach to the root graphics layer.
  local_frame->View()->UpdateLifecycleToCompositingCleanPlusScrolling();
  page_color_overlay_->Update();
}

void Page::UpdatePageColorOverlay() {
  if (page_color_overlay_)
    page_color_overlay_->Update();
}

void Page::PaintPageColorOverlay() {
  if (page_color_overlay_)
    page_color_overlay_->GetGraphicsLayer()->Paint(nullptr);
}

Page::PageClients::PageClients() : chrome_client(nullptr) {}

Page::PageClients::~PageClients() = default;

template class CORE_TEMPLATE_EXPORT Supplement<Page>;

}  // namespace blink
