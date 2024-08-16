/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"

#include <memory>

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/features.h"
#include "cc/layers/picture_layer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache_base.h"
#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/screen_metrics_emulator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/core/page/page_popup_controller.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/input/input_metrics.h"
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {
namespace {
ScrollableArea* ToScrollableArea(Node* node) {
  DCHECK(node);
  LayoutBox* scrolling_box = node->GetLayoutBox();
  if (auto* element = DynamicTo<Element>(node))
    scrolling_box = element->GetLayoutBoxForScrolling();
  return scrolling_box ? scrolling_box->GetScrollableArea() : nullptr;
}

bool CanScroll(Node* node) {
  if (!node)
    return false;
  return ToScrollableArea(node);
}

Node* FindFirstScroller(Node* event_target) {
  DCHECK(event_target);
  Node* cur_node = nullptr;
  bool found = false;
  LayoutBox* cur_box = event_target->GetLayoutObject()
                           ? event_target->GetLayoutObject()->EnclosingBox()
                           : nullptr;
  while (cur_box) {
    cur_node = cur_box->GetNode();
    if (CanScroll(cur_node)) {
      found = true;
      break;
    }
    cur_box = cur_box->ContainingBlock();
  }
  if (found && cur_node)
    return cur_node;
  return nullptr;
}

Page* CreatePage(ChromeClient& chrome_client, WebViewImpl& opener_web_view) {
  Settings& main_settings = opener_web_view.GetPage()->GetSettings();
  Page* page = Page::CreateNonOrdinary(
      chrome_client,
      opener_web_view.GetPage()->GetPageScheduler()->GetAgentGroupScheduler(),
      &opener_web_view.GetPage()->GetColorProviderColorMaps());
  page->GetSettings().SetAcceleratedCompositingEnabled(true);
  page->GetSettings().SetScriptEnabled(true);
  page->GetSettings().SetAllowScriptsToCloseWindows(true);
  page->GetSettings().SetMinimumFontSize(main_settings.GetMinimumFontSize());
  page->GetSettings().SetMinimumLogicalFontSize(
      main_settings.GetMinimumLogicalFontSize());
  page->GetSettings().SetScrollAnimatorEnabled(
      main_settings.GetScrollAnimatorEnabled());
  page->GetSettings().SetAvailablePointerTypes(
      main_settings.GetAvailablePointerTypes());
  page->GetSettings().SetPrimaryPointerType(
      main_settings.GetPrimaryPointerType());
  page->GetSettings().SetPreferredColorScheme(
      main_settings.GetPreferredColorScheme());
  page->GetSettings().SetForceDarkModeEnabled(
      main_settings.GetForceDarkModeEnabled());
  page->GetSettings().SetInForcedColors(main_settings.GetInForcedColors());

  const MediaFeatureOverrides* media_feature_overrides =
      opener_web_view.GetPage()->GetMediaFeatureOverrides();
  if (media_feature_overrides &&
      media_feature_overrides->GetPreferredColorScheme().has_value()) {
    page->SetMediaFeatureOverride(
        AtomicString("prefers-color-scheme"),
        media_feature_overrides->GetPreferredColorScheme().value() ==
                mojom::blink::PreferredColorScheme::kDark
            ? "dark"
            : "light");
  }
  return page;
}

}  // namespace

class PagePopupChromeClient final : public EmptyChromeClient {
 public:
  explicit PagePopupChromeClient(WebPagePopupImpl* popup) : popup_(popup) {}

  void SetWindowRect(const gfx::Rect& rect, LocalFrame&) override {
    popup_->SetWindowRect(rect);
  }

  bool IsPopup() override { return true; }

 private:
  void CloseWindow() override {
    // This skips past the PopupClient by calling ClosePopup() instead of
    // Cancel().
    popup_->ClosePopup();
  }

  gfx::Rect RootWindowRect(LocalFrame&) override {
    // There is only one frame/widget in a WebPagePopup, so we can ignore the
    // param.
    return popup_->WindowRectInScreen();
  }

  gfx::Rect LocalRootToScreenDIPs(const gfx::Rect& rect_in_local_root,
                                  const LocalFrameView* view) const override {
    DCHECK(view);
    DCHECK_EQ(view->GetChromeClient(), this);

    gfx::Rect window_rect = popup_->WindowRectInScreen();
    gfx::Rect rect_in_dips =
        popup_->widget_base_->BlinkSpaceToEnclosedDIPs(rect_in_local_root);
    rect_in_dips.Offset(window_rect.x(), window_rect.y());
    return rect_in_dips;
  }

  float WindowToViewportScalar(LocalFrame*,
                               const float scalar_value) const override {
    return popup_->widget_base_->DIPsToBlinkSpace(scalar_value);
  }

  void AddMessageToConsole(LocalFrame*,
                           mojom::ConsoleMessageSource,
                           mojom::ConsoleMessageLevel,
                           const String& message,
                           unsigned line_number,
                           const String&,
                           const String&) override {
#ifndef NDEBUG
    fprintf(stderr, "CONSOLE MESSAGE:%u: %s\n", line_number,
            message.Utf8().c_str());
#endif
  }

  void ScheduleAnimation(const LocalFrameView*,
                         base::TimeDelta delay = base::TimeDelta()) override {
    // Destroying/removing the popup's content can be seen as a mutation that
    // ends up calling ScheduleAnimation(). Since the popup is going away, we
    // do not wish to actually do anything.
    if (popup_->closing_)
      return;

    // When the renderer has a compositor thread we need to follow the
    // normal code path.
    if (WebTestSupport::IsRunningWebTest() && !Thread::CompositorThread()) {
      // In single-threaded web tests, the owner frame tree runs the composite
      // step for the popup. Popup widgets don't run any composite step on their
      // own. And we don't run popup tests with a compositor thread, so no need
      // to check for that.
      Document& opener_document =
          popup_->popup_client_->OwnerElement().GetDocument();
      if (Page* page = opener_document.GetPage()) {
        page->GetChromeClient().ScheduleAnimation(
            opener_document.GetFrame()->View(), delay);
      }
      return;
    }
    popup_->widget_base_->RequestAnimationAfterDelay(delay);
  }

  cc::AnimationHost* GetCompositorAnimationHost(LocalFrame&) const override {
    return popup_->widget_base_->AnimationHost();
  }

  cc::AnimationTimeline* GetScrollAnimationTimeline(
      LocalFrame&) const override {
    return popup_->widget_base_->ScrollAnimationTimeline();
  }

  const display::ScreenInfo& GetScreenInfo(LocalFrame&) const override {
    // LocalFrame is ignored since there is only 1 frame in a popup.
    return popup_->GetScreenInfo();
  }

  const display::ScreenInfos& GetScreenInfos(LocalFrame&) const override {
    // LocalFrame is ignored since there is only 1 frame in a popup.
    return popup_->GetScreenInfos();
  }

  gfx::Size MinimumWindowSize() const override { return gfx::Size(0, 0); }

  void SetEventListenerProperties(
      LocalFrame* frame,
      cc::EventListenerClass event_class,
      cc::EventListenerProperties properties) override {
    // WebPagePopup always routes input to main thread (set up in RenderWidget),
    // so no need to update listener properties.
  }

  void SetHasScrollEventHandlers(LocalFrame* frame,
                                 bool has_event_handlers) override {
    // WebPagePopup's compositor does not handle compositor thread input (set up
    // in RenderWidget) so there is no need to signal this.
  }

  void SetTouchAction(LocalFrame* frame, TouchAction touch_action) override {
    // Touch action is not used in the compositor for WebPagePopup.
  }

  void AttachRootLayer(scoped_refptr<cc::Layer> layer,
                       LocalFrame* local_root) override {
    popup_->SetRootLayer(layer.get());
  }

  void UpdateTooltipUnderCursor(LocalFrame&,
                                const String& tooltip_text,
                                TextDirection dir) override {
    popup_->widget_base_->UpdateTooltipUnderCursor(tooltip_text, dir);
  }

  void UpdateTooltipFromKeyboard(LocalFrame&,
                                 const String& tooltip_text,
                                 TextDirection dir,
                                 const gfx::Rect& bounds) override {
    popup_->widget_base_->UpdateTooltipFromKeyboard(tooltip_text, dir, bounds);
  }

  void ClearKeyboardTriggeredTooltip(LocalFrame&) override {
    popup_->widget_base_->ClearKeyboardTriggeredTooltip();
  }

  void InjectScrollbarGestureScroll(
      LocalFrame& local_frame,
      const gfx::Vector2dF& delta,
      ui::ScrollGranularity granularity,
      cc::ElementId scrollable_area_element_id,
      WebInputEvent::Type injected_type) override {
    popup_->InjectScrollbarGestureScroll(
        delta, granularity, scrollable_area_element_id, injected_type);
  }

  WebPagePopupImpl* popup_;
};

// WebPagePopupImpl ----------------------------------------------------------

WebPagePopupImpl::WebPagePopupImpl(
    CrossVariantMojoAssociatedRemote<mojom::blink::PopupWidgetHostInterfaceBase>
        popup_widget_host,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    WebViewImpl* opener_web_view,
    AgentGroupScheduler& agent_group_scheduler,
    const display::ScreenInfos& screen_infos,
    PagePopupClient* popup_client)
    : opener_web_view_(opener_web_view),
      chrome_client_(MakeGarbageCollected<PagePopupChromeClient>(this)),
      local_frame_client_(MakeGarbageCollected<EmptyLocalFrameClient>()),
      page_(CreatePage(*chrome_client_, *opener_web_view)),
      popup_client_(popup_client),
      popup_widget_host_(std::move(popup_widget_host)),
      widget_base_(std::make_unique<WidgetBase>(
          /*widget_base_client=*/this,
          std::move(widget_host),
          std::move(widget),
          agent_group_scheduler.DefaultTaskRunner(),
          /*hidden=*/false,
          /*never_composited=*/false,
          /*is_embedded=*/false,
          /*is_for_scalable_page=*/true)) {
  DCHECK(popup_client_);
  popup_widget_host_.set_disconnect_handler(WTF::BindOnce(
      &WebPagePopupImpl::WidgetHostDisconnected, WTF::Unretained(this)));
  if (auto* main_frame_widget = opener_web_view->MainFrameViewWidget()) {
    if (auto* device_emulator = main_frame_widget->DeviceEmulator()) {
      opener_widget_screen_origin_ = device_emulator->ViewRectOrigin();
      opener_original_widget_screen_origin_ =
          device_emulator->original_view_rect().origin();
      opener_emulator_scale_ = device_emulator->scale();
    }
  }

  InitializeCompositing(screen_infos,
                        /*settings=*/nullptr);

  popup_client_->AdjustSettings(page_->GetSettings());
  popup_client_->CreatePagePopupController(*page_, *this);

  // Creating new WindowAgentFactory because page popup content is owned by the
  // user agent and should be isolated from the main frame. However, if we are a
  // page popup in LayoutTests ensure we use the popup owner's frame for looking
  // up the Agent so tests can possibly access the document via internals API.
  WindowAgentFactory* window_agent_factory = nullptr;
  if (WebTestSupport::IsRunningWebTest()) {
    Document& owner_document = popup_client_->OwnerElement().GetDocument();
    window_agent_factory = &owner_document.GetFrame()->window_agent_factory();
  }

  auto* frame = MakeGarbageCollected<LocalFrame>(
      local_frame_client_, *page_,
      /* FrameOwner* */ nullptr, /* Frame* parent */ nullptr,
      /* Frame* previous_sibling */ nullptr,
      FrameInsertType::kInsertInConstructor, LocalFrameToken(),
      window_agent_factory,
      /* InterfaceRegistry* */ nullptr,
      /* BrowserInterfaceBroker */ mojo::NullRemote());
  frame->SetPagePopupOwner(popup_client_->OwnerElement());
  frame->SetView(MakeGarbageCollected<LocalFrameView>(*frame));

  if (WebTestSupport::IsRunningWebTest()) {
    // In order for the shared WindowAgentFactory for tests to work correctly,
    // we need to also copy settings used in WindowAgent selection over to the
    // popup frame.
    Settings* owner_settings =
        popup_client_->OwnerElement().GetDocument().GetFrame()->GetSettings();
    frame->GetSettings()->SetWebSecurityEnabled(
        owner_settings->GetWebSecurityEnabled());
    frame->GetSettings()->SetAllowUniversalAccessFromFileURLs(
        owner_settings->GetAllowUniversalAccessFromFileURLs());
  }

  // TODO(https://crbug.com/1355751) Initialize `storage_key`.
  frame->Init(/*opener=*/nullptr, DocumentToken(), /*policy_container=*/nullptr,
              StorageKey(), /*document_ukm_source_id=*/ukm::kInvalidSourceId,
              /*creator_base_url=*/KURL());
  frame->View()->SetParentVisible(true);
  frame->View()->SetSelfVisible(true);

  DCHECK(frame->DomWindow());
  DCHECK_EQ(popup_client_->OwnerElement().GetDocument().ExistingAXObjectCache(),
            frame->GetDocument()->ExistingAXObjectCache());
  if (AXObjectCache* cache = frame->GetDocument()->ExistingAXObjectCache())
    cache->ChildrenChanged(&popup_client_->OwnerElement());

  page_->DidInitializeCompositing(*widget_base_->AnimationHost());

  SegmentedBuffer data;
  popup_client_->WriteDocument(data);
  frame->SetLayoutZoomFactor(popup_client_->ZoomFactor());
  frame->ForceSynchronousDocumentInstall(AtomicString("text/html"),
                                         std::move(data));

  popup_owner_client_rect_ =
      popup_client_->OwnerElement().GetBoundingClientRect();
  popup_widget_host_->ShowPopup(
      initial_rect_, GetAnchorRectInScreen(),
      WTF::BindOnce(&WebPagePopupImpl::DidShowPopup, WTF::Unretained(this)));
  should_defer_setting_window_rect_ = false;
  widget_base_->SetPendingWindowRect(initial_rect_);

  SetFocus(true);
}

WebPagePopupImpl::~WebPagePopupImpl() {
  // Ensure DestroyPage was called.
  DCHECK(!page_);
}

void WebPagePopupImpl::DidShowPopup() {
  if (!widget_base_)
    return;
  widget_base_->AckPendingWindowRect();
}

void WebPagePopupImpl::DidSetBounds() {
  if (!widget_base_)
    return;
  widget_base_->AckPendingWindowRect();
}

void WebPagePopupImpl::InitializeCompositing(
    const display::ScreenInfos& screen_infos,
    const cc::LayerTreeSettings* settings) {
  // Careful Initialize() is called after InitializeCompositing, so don't do
  // much work here.
  widget_base_->InitializeCompositing(*page_->GetPageScheduler(), screen_infos,
                                      settings,
                                      /*frame_widget_input_handler=*/nullptr,
                                      /*previous_widget=*/nullptr);
  cc::LayerTreeDebugState debug_state =
      widget_base_->LayerTreeHost()->GetDebugState();
  debug_state.TurnOffHudInfoDisplay();
  widget_base_->LayerTreeHost()->SetDebugState(debug_state);
}

void WebPagePopupImpl::SetCursor(const ui::Cursor& cursor) {
  widget_base_->SetCursor(cursor);
}

bool WebPagePopupImpl::HandlingInputEvent() {
  return widget_base_->input_handler().handling_input_event();
}

void WebPagePopupImpl::SetHandlingInputEvent(bool handling) {
  widget_base_->input_handler().set_handling_input_event(handling);
}

void WebPagePopupImpl::ProcessInputEventSynchronouslyForTesting(
    const WebCoalescedInputEvent& event) {
  widget_base_->input_handler().HandleInputEvent(event, nullptr,
                                                 base::DoNothing());
}

void WebPagePopupImpl::DispatchNonBlockingEventForTesting(
    std::unique_ptr<WebCoalescedInputEvent> event) {
  widget_base_->widget_input_handler_manager()
      ->DispatchEventOnInputThreadForTesting(
          std::move(event),
          mojom::blink::WidgetInputHandler::DispatchEventCallback());
}

void WebPagePopupImpl::UpdateTextInputState() {
  widget_base_->UpdateTextInputState();
}

void WebPagePopupImpl::UpdateSelectionBounds() {
  widget_base_->UpdateSelectionBounds();
}

void WebPagePopupImpl::ShowVirtualKeyboard() {
  widget_base_->ShowVirtualKeyboard();
}

void WebPagePopupImpl::SetFocus(bool focus) {
  widget_base_->SetFocus(focus
                             ? mojom::blink::FocusState::kFocused
                             : mojom::blink::FocusState::kNotFocusedAndActive);
}

bool WebPagePopupImpl::HasFocus() {
  return widget_base_->has_focus();
}

void WebPagePopupImpl::FlushInputProcessedCallback() {
  widget_base_->FlushInputProcessedCallback();
}

void WebPagePopupImpl::CancelCompositionForPepper() {
  widget_base_->CancelCompositionForPepper();
}

void WebPagePopupImpl::ApplyVisualProperties(
    const VisualProperties& visual_properties) {
  widget_base_->UpdateVisualProperties(visual_properties);
}

const display::ScreenInfo& WebPagePopupImpl::GetScreenInfo() {
  return widget_base_->GetScreenInfo();
}

const display::ScreenInfos& WebPagePopupImpl::GetScreenInfos() {
  return widget_base_->screen_infos();
}

const display::ScreenInfo& WebPagePopupImpl::GetOriginalScreenInfo() {
  return widget_base_->GetScreenInfo();
}

const display::ScreenInfos& WebPagePopupImpl::GetOriginalScreenInfos() {
  return widget_base_->screen_infos();
}

gfx::Rect WebPagePopupImpl::WindowRect() {
  return widget_base_->WindowRect();
}

gfx::Rect WebPagePopupImpl::ViewRect() {
  return widget_base_->ViewRect();
}

void WebPagePopupImpl::SetScreenRects(const gfx::Rect& widget_screen_rect,
                                      const gfx::Rect& window_screen_rect) {
  widget_base_->SetScreenRects(widget_screen_rect, window_screen_rect);
}

gfx::Size WebPagePopupImpl::VisibleViewportSizeInDIPs() {
  return widget_base_->VisibleViewportSizeInDIPs();
}

bool WebPagePopupImpl::IsHidden() const {
  return widget_base_->is_hidden();
}

void WebPagePopupImpl::SetCompositorVisible(bool visible) {
  widget_base_->SetCompositorVisible(visible);
}

void WebPagePopupImpl::WarmUpCompositor() {
  widget_base_->WarmUpCompositor();
}

void WebPagePopupImpl::PostMessageToPopup(const String& message) {
  if (!page_)
    return;
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  MainFrame().DomWindow()->DispatchEvent(*MessageEvent::Create(message));
}

void WebPagePopupImpl::Update() {
  if (!page_ && !popup_client_)
    return;

  DOMRect* dom_rect = popup_client_->OwnerElement().GetBoundingClientRect();
  bool forced_update = (*dom_rect != *popup_owner_client_rect_);
  if (forced_update)
    popup_owner_client_rect_ = dom_rect;

  popup_client_->Update(forced_update);
  if (forced_update)
    SetWindowRect(WindowRectInScreen());
}

void WebPagePopupImpl::DestroyPage() {
  page_->WillStopCompositing();
  page_->WillBeDestroyed();
  page_.Clear();
}

AXObject* WebPagePopupImpl::RootAXObject(Element* popup_owner) {
  if (!page_)
    return nullptr;
  // If |page_| is non-null, the main frame must have a Document.
  Document* document = MainFrame().GetDocument();
  AXObjectCacheBase* cache =
      To<AXObjectCacheBase>(document->ExistingAXObjectCache());
  // There should never be a circumstance when RootAXObject() is triggered
  // and the AXObjectCache doesn't already exist. It's called when trying
  // to attach the accessibility tree of the pop-up to the host page.
  return cache->GetOrCreate(document, cache->Get(popup_owner));
}

void WebPagePopupImpl::SetWindowRect(const gfx::Rect& rect_in_screen) {
  if (ShouldCheckPopupPositionForTelemetry()) {
    gfx::Rect owner_window_rect_in_screen = OwnerWindowRectInScreen();
    Document& document = popup_client_->OwnerElement().GetDocument();
    if (owner_window_rect_in_screen.Contains(rect_in_screen)) {
      UseCounter::Count(document,
                        WebFeature::kPopupDoesNotExceedOwnerWindowBounds);
    } else {
      WebFeature feature =
          document.GetFrame()->IsOutermostMainFrame()
              ? WebFeature::kPopupExceedsOwnerWindowBounds
              : WebFeature::kPopupExceedsOwnerWindowBoundsForIframe;
      UseCounter::Count(document, feature);
    }
  }

  gfx::Rect window_rect = rect_in_screen;

  // Popups aren't emulated, but the WidgetScreenRect and WindowScreenRect
  // given to them are. When they set the WindowScreenRect it is based on those
  // emulated values, so we reverse the emulation.
  if (opener_emulator_scale_)
    EmulatedToScreenRect(window_rect);

  if (!should_defer_setting_window_rect_) {
    widget_base_->SetPendingWindowRect(window_rect);
    popup_widget_host_->SetPopupBounds(
        window_rect,
        WTF::BindOnce(&WebPagePopupImpl::DidSetBounds, WTF::Unretained(this)));
  } else {
    initial_rect_ = window_rect;
  }
}

void WebPagePopupImpl::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  root_layer_ = std::move(layer);
  widget_base_->LayerTreeHost()->SetRootLayer(root_layer_);
}

void WebPagePopupImpl::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  if (!page_)
    return;
  page_->Animator().SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}

void WebPagePopupImpl::UpdateLifecycle(WebLifecycleUpdate requested_update,
                                       DocumentUpdateReason reason) {
  if (!page_)
    return;
  // Popups always update their lifecycle in the context of the containing
  // document's lifecycle, so explicitly override the reason.
  page_->UpdateLifecycle(MainFrame(), requested_update,
                         DocumentUpdateReason::kPagePopup);
}

void WebPagePopupImpl::Resize(const gfx::Size& new_size_in_viewport) {
  gfx::Size new_size_in_dips =
      widget_base_->BlinkSpaceToFlooredDIPs(new_size_in_viewport);
  gfx::Rect window_rect_in_dips = WindowRectInScreen();

  // TODO(bokan): We should only call into this if the bounds actually changed
  // but this reveals a bug in Aura. crbug.com/633140.
  window_rect_in_dips.set_size(new_size_in_dips);
  SetWindowRect(window_rect_in_dips);

  if (page_) {
    MainFrame().View()->Resize(new_size_in_viewport);
    page_->GetVisualViewport().SetSize(new_size_in_viewport);
  }
}

WebInputEventResult WebPagePopupImpl::HandleKeyEvent(
    const WebKeyboardEvent& event) {
  if (closing_)
    return WebInputEventResult::kNotHandled;

  if (suppress_next_keypress_event_) {
    suppress_next_keypress_event_ = false;
    return WebInputEventResult::kHandledSuppressed;
  }

  if (WebInputEvent::Type::kRawKeyDown == event.GetType()) {
    Element* focused_element = FocusedElement();
    if (event.windows_key_code == VKEY_TAB && focused_element &&
        focused_element->IsKeyboardFocusable()) {
      // If the tab key is pressed while a keyboard focusable element is
      // focused, we should not send a corresponding keypress event.
      suppress_next_keypress_event_ = true;
    }
  }
  LocalFrame::NotifyUserActivation(
      popup_client_->OwnerElement().GetDocument().GetFrame(),
      mojom::blink::UserActivationNotificationType::kInteraction);
  return MainFrame().GetEventHandler().KeyEvent(event);
}

cc::LayerTreeHost* WebPagePopupImpl::LayerTreeHostForTesting() {
  return widget_base_->LayerTreeHost();
}

void WebPagePopupImpl::OnCommitRequested() {
  if (page_ && page_->MainFrame()) {
    if (auto* view = MainFrame().View())
      view->OnCommitRequested();
  }
}

void WebPagePopupImpl::BeginMainFrame(base::TimeTicks last_frame_time) {
  if (!page_)
    return;
  // FIXME: This should use lastFrameTimeMonotonic but doing so
  // breaks tests.
  page_->Animate(base::TimeTicks::Now());
}

void WebPagePopupImpl::WillHandleGestureEvent(const WebGestureEvent& event,
                                              bool* suppress) {}

void WebPagePopupImpl::WillHandleMouseEvent(const WebMouseEvent& event) {}

void WebPagePopupImpl::ObserveGestureEventAndResult(
    const WebGestureEvent& gesture_event,
    const gfx::Vector2dF& unused_delta,
    const cc::OverscrollBehavior& overscroll_behavior,
    bool event_processed) {
}

WebInputEventResult WebPagePopupImpl::HandleCharEvent(
    const WebKeyboardEvent& event) {
  if (suppress_next_keypress_event_) {
    suppress_next_keypress_event_ = false;
    return WebInputEventResult::kHandledSuppressed;
  }
  return HandleKeyEvent(event);
}

WebInputEventResult WebPagePopupImpl::HandleGestureEvent(
    const WebGestureEvent& event) {
  if (closing_)
    return WebInputEventResult::kNotHandled;
  if (event.GetType() == WebInputEvent::Type::kGestureTap ||
      event.GetType() == WebInputEvent::Type::kGestureTapDown) {
    if (!IsViewportPointInWindow(event.PositionInWidget().x(),
                                 event.PositionInWidget().y())) {
      Cancel();
      return WebInputEventResult::kNotHandled;
    }
    LocalFrame::NotifyUserActivation(
        popup_client_->OwnerElement().GetDocument().GetFrame(),
        mojom::blink::UserActivationNotificationType::kInteraction);
    CheckScreenPointInOwnerWindowAndCount(
        event.PositionInScreen(),
        WebFeature::kPopupGestureTapExceedsOwnerWindowBounds);
  }
  if (event.GetType() == WebInputEvent::Type::kGestureScrollBegin) {
    HitTestLocation locationScroll(event.PositionInWidget());
    HitTestResult resultScroll =
        MainFrame().GetEventHandler().HitTestResultAtLocation(locationScroll);
    scrollable_node_ = FindFirstScroller(resultScroll.InnerNode());
    RecordScrollReasonsMetric(
        event.SourceDevice(),
        cc::MainThreadScrollingReason::kPopupNoThreadedInput);
    return WebInputEventResult::kHandledSystem;
  }
  if (event.GetType() == WebInputEvent::Type::kGestureScrollUpdate) {
    if (!scrollable_node_) {
      return WebInputEventResult::kNotHandled;
    }

    ScrollableArea* scrollable = ToScrollableArea(scrollable_node_);

    if (!scrollable) {
      return WebInputEventResult::kNotHandled;
    }
    ScrollOffset scroll_offset(-event.data.scroll_update.delta_x,
                               -event.data.scroll_update.delta_y);
    scrollable->UserScroll(event.data.scroll_update.delta_units, scroll_offset,
                           ScrollableArea::ScrollCallback());
    return WebInputEventResult::kHandledSystem;
  }
  if (event.GetType() == WebInputEvent::Type::kGestureScrollEnd) {
    scrollable_node_ = nullptr;
    return WebInputEventResult::kHandledSystem;
  }
  WebGestureEvent scaled_event =
      TransformWebGestureEvent(MainFrame().View(), event);
  return MainFrame().GetEventHandler().HandleGestureEvent(scaled_event);
}

void WebPagePopupImpl::HandleMouseDown(LocalFrame& main_frame,
                                       const WebMouseEvent& event) {
  if (IsViewportPointInWindow(event.PositionInWidget().x(),
                              event.PositionInWidget().y())) {
    LocalFrame::NotifyUserActivation(
        popup_client_->OwnerElement().GetDocument().GetFrame(),
        mojom::blink::UserActivationNotificationType::kInteraction);
    CheckScreenPointInOwnerWindowAndCount(
        event.PositionInScreen(),
        WebFeature::kPopupMouseDownExceedsOwnerWindowBounds);
    WidgetEventHandler::HandleMouseDown(main_frame, event);
  } else {
    Cancel();
  }
}

WebInputEventResult WebPagePopupImpl::HandleMouseWheel(
    LocalFrame& main_frame,
    const WebMouseWheelEvent& event) {
  if (IsViewportPointInWindow(event.PositionInWidget().x(),
                              event.PositionInWidget().y())) {
    CheckScreenPointInOwnerWindowAndCount(
        event.PositionInScreen(),
        WebFeature::kPopupMouseWheelExceedsOwnerWindowBounds);
    return WidgetEventHandler::HandleMouseWheel(main_frame, event);
  }
  Cancel();
  return WebInputEventResult::kNotHandled;
}

LocalFrame& WebPagePopupImpl::MainFrame() const {
  DCHECK(page_);
  // The main frame for a popup will never be out-of-process.
  return *To<LocalFrame>(page_->MainFrame());
}

Element* WebPagePopupImpl::FocusedElement() const {
  if (!page_)
    return nullptr;

  LocalFrame* frame = page_->GetFocusController().FocusedFrame();
  if (!frame)
    return nullptr;

  Document* document = frame->GetDocument();
  if (!document)
    return nullptr;

  return document->FocusedElement();
}

bool WebPagePopupImpl::IsViewportPointInWindow(int x, int y) {
  gfx::Point point_in_dips =
      widget_base_->BlinkSpaceToFlooredDIPs(gfx::Point(x, y));
  gfx::Rect window_rect = WindowRectInScreen();
  return gfx::Rect(window_rect.size()).Contains(point_in_dips);
}

bool WebPagePopupImpl::ShouldCheckPopupPositionForTelemetry() const {
  // Avoid doing any telemetry work when the popup is closing or the
  // owner element is not shown anymore.
  return !closing_ && popup_client_->OwnerElement().GetDocument().View();
}

void WebPagePopupImpl::CheckScreenPointInOwnerWindowAndCount(
    const gfx::PointF& point_in_screen,
    WebFeature feature) const {
  if (!ShouldCheckPopupPositionForTelemetry())
    return;

  gfx::Rect owner_window_rect = OwnerWindowRectInScreen();
  if (!owner_window_rect.Contains(point_in_screen.x(), point_in_screen.y()))
    UseCounter::Count(popup_client_->OwnerElement().GetDocument(), feature);
}

gfx::Rect WebPagePopupImpl::OwnerWindowRectInScreen() const {
  LocalFrameView* view = popup_client_->OwnerElement().GetDocument().View();
  DCHECK(view);
  gfx::Rect frame_rect = view->FrameRect();
  return view->FrameToScreen(frame_rect);
}

gfx::Rect WebPagePopupImpl::GetAnchorRectInScreen() const {
  LocalFrameView* view = popup_client_->OwnerElement().GetDocument().View();
  DCHECK(view);

  return view->GetFrame().GetChromeClient().LocalRootToScreenDIPs(
      popup_client_->OwnerElement().VisibleBoundsInLocalRoot(), view);
}

WebInputEventResult WebPagePopupImpl::DispatchBufferedTouchEvents() {
  if (closing_)
    return WebInputEventResult::kNotHandled;
  return MainFrame().GetEventHandler().DispatchBufferedTouchEvents();
}

WebInputEventResult WebPagePopupImpl::HandleInputEvent(
    const WebCoalescedInputEvent& event) {
  if (closing_)
    return WebInputEventResult::kNotHandled;
  DCHECK(!WebInputEvent::IsTouchEventType(event.Event().GetType()));
  return WidgetEventHandler::HandleInputEvent(event, &MainFrame());
}

void WebPagePopupImpl::FocusChanged(mojom::blink::FocusState focus_state) {
  if (!page_)
    return;
  page_->GetFocusController().SetActive(
      focus_state == mojom::blink::FocusState::kFocused ||
      focus_state == mojom::blink::FocusState::kNotFocusedAndActive);
  page_->GetFocusController().SetFocused(focus_state ==
                                         mojom::blink::FocusState::kFocused);
}

void WebPagePopupImpl::ScheduleAnimation() {
  widget_base_->LayerTreeHost()->SetNeedsAnimate();
}

void WebPagePopupImpl::UpdateVisualProperties(
    const VisualProperties& visual_properties) {
  widget_base_->UpdateSurfaceAndScreenInfo(
      visual_properties.local_surface_id.value_or(viz::LocalSurfaceId()),
      visual_properties.compositor_viewport_pixel_rect,
      visual_properties.screen_infos);
  widget_base_->SetVisibleViewportSizeInDIPs(
      visual_properties.visible_viewport_size);

  // TODO(crbug.com/1155388): Popups are a single "global" object that don't
  // inherit the scale factor of the frame containing the corresponding element
  // so compositing_scale_factor is always 1 and has no effect.
  float combined_scale_factor = visual_properties.page_scale_factor *
                                visual_properties.compositing_scale_factor;
  widget_base_->LayerTreeHost()->SetExternalPageScaleFactor(
      combined_scale_factor, visual_properties.is_pinch_gesture_active);

  Resize(widget_base_->DIPsToCeiledBlinkSpace(visual_properties.new_size));
}

gfx::Rect WebPagePopupImpl::ViewportVisibleRect() {
  return widget_base_->CompositorViewportRect();
}

KURL WebPagePopupImpl::GetURLForDebugTrace() {
  if (!page_)
    return {};
  WebFrame* main_frame = opener_web_view_->MainFrame();
  if (main_frame->IsWebLocalFrame())
    return main_frame->ToWebLocalFrame()->GetDocument().Url();
  return {};
}

void WebPagePopupImpl::WidgetHostDisconnected() {
  Close();
  // Careful, this is now destroyed.
}

void WebPagePopupImpl::Close() {
  // If the popup is closed from the renderer via Cancel(), then ClosePopup()
  // has already run on another stack, and destroyed |page_|. If the popup is
  // closed from the browser via IPC to RenderWidget, then we come here first
  // and want to synchronously Cancel() immediately.
  if (page_) {
    // We set |closing_| here to inform ClosePopup() that it is being run
    // synchronously from inside Close().
    closing_ = true;
    // This should end up running ClosePopup() though the PopupClient.
    Cancel();
  }

  // TODO(dtapuska): WidgetBase shutdown should happen before Page is
  // disposed if the PageScheduler get used more. See crbug.com/1340914
  // for a crash.
  widget_base_->Shutdown();
  widget_base_.reset();

  // Self-delete on Close().
  Release();
}

void WebPagePopupImpl::ClosePopup() {
  // There's always a |page_| when we get here because if we Close() this object
  // due to ClosePopupWidgetSoon(), it will see the |page_| destroyed and not
  // run this method again. And the renderer does not close the same popup more
  // than once.
  DCHECK(page_);

  // If the popup is closed from the renderer via Cancel(), then we want to
  // initiate closing immediately here, but send a request for completing the
  // close process through the browser via PopupWidgetHost::RequestClosePopup(),
  // which will disconnect the channel come back to this class to
  // WidgetHostDisconnected(). If |closing_| is already true, then the browser
  // initiated the close on its own, via WidgetHostDisconnected IPC, which means
  // ClosePopup() is being run inside the same stack, and does not need to
  // request the browser to close the widget.
  const bool running_inside_close = closing_;
  if (!running_inside_close) {
    // Bounce through the browser to get it to close the RenderWidget, which
    // will Close() this object too. Only if we're not currently already
    // responding to the browser closing us though. We don't need to do a post
    // task like WebViewImpl::CloseWindowSoon does because we shouldn't be
    // executing javascript influencing this popup widget.
    popup_widget_host_->RequestClosePopup();
  }

  closing_ = true;

  {
    // This function can be called in EventDispatchForbiddenScope for the main
    // document, and the following operations dispatch some events.  It's safe
    // because web authors can't listen the events.
    EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;

    MainFrame().Loader().StopAllLoaders(/*abort_client=*/true);
    PagePopupController::From(*page_)->ClearPagePopupClient();
    DestroyPage();
  }

  // Informs the client to drop any references to this popup as it will be
  // destroyed.
  popup_client_->DidClosePopup();

  // Drops the reference to the popup from WebViewImpl, making |this| the only
  // owner of itself. Note however that WebViewImpl may briefly extend the
  // lifetime of this object since it owns a reference, but it should only be
  // to call HasSamePopupClient().
  opener_web_view_->CleanupPagePopup();
}

LocalDOMWindow* WebPagePopupImpl::Window() {
  return MainFrame().DomWindow();
}

WebDocument WebPagePopupImpl::GetDocument() {
  return WebDocument(MainFrame().GetDocument());
}

void WebPagePopupImpl::Cancel() {
  if (popup_client_)
    popup_client_->CancelPopup();
}

gfx::Rect WebPagePopupImpl::WindowRectInScreen() const {
  return widget_base_->WindowRect();
}

void WebPagePopupImpl::InjectScrollbarGestureScroll(
    const gfx::Vector2dF& delta,
    ui::ScrollGranularity granularity,
    cc::ElementId scrollable_area_element_id,
    WebInputEvent::Type injected_type) {
  widget_base_->input_handler().InjectScrollbarGestureScroll(
      delta, granularity, scrollable_area_element_id, injected_type);
}

void WebPagePopupImpl::ScreenRectToEmulated(gfx::Rect& screen_rect) {
  if (!opener_emulator_scale_)
    return;
  screen_rect.set_x(
      opener_widget_screen_origin_.x() +
      (screen_rect.x() - opener_original_widget_screen_origin_.x()) /
          opener_emulator_scale_);
  screen_rect.set_y(
      opener_widget_screen_origin_.y() +
      (screen_rect.y() - opener_original_widget_screen_origin_.y()) /
          opener_emulator_scale_);
}

void WebPagePopupImpl::EmulatedToScreenRect(gfx::Rect& screen_rect) {
  if (!opener_emulator_scale_)
    return;
  screen_rect.set_x(opener_original_widget_screen_origin_.x() +
                    (screen_rect.x() - opener_widget_screen_origin_.x()) *
                        opener_emulator_scale_);
  screen_rect.set_y(opener_original_widget_screen_origin_.y() +
                    (screen_rect.y() - opener_widget_screen_origin_.y()) *
                        opener_emulator_scale_);
}

std::unique_ptr<cc::LayerTreeFrameSink>
WebPagePopupImpl::AllocateNewLayerTreeFrameSink() {
  return nullptr;
}

// WebPagePopup ----------------------------------------------------------------

WebPagePopupImpl* WebPagePopupImpl::Create(
    CrossVariantMojoAssociatedRemote<mojom::blink::PopupWidgetHostInterfaceBase>
        popup_widget_host,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    WebViewImpl* opener_webview,
    AgentGroupScheduler& agent_group_scheduler,
    const display::ScreenInfos& screen_infos,
    PagePopupClient* popup_client) {
  // A WebPagePopupImpl instance usually has two references.
  //  - One owned by the instance itself. It represents the visible widget.
  //  - One owned by a WebViewImpl. It's released when the WebViewImpl ask the
  //    WebPagePopupImpl to close.
  // We need them because the closing operation is asynchronous and the widget
  // can be closed while the WebViewImpl is unaware of it.
  auto popup = base::AdoptRef(new WebPagePopupImpl(
      std::move(popup_widget_host), std::move(widget_host), std::move(widget),
      opener_webview, agent_group_scheduler, screen_infos, popup_client));
  popup->AddRef();
  return popup.get();
}

}  // namespace blink
