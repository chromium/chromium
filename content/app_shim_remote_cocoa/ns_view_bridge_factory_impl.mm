// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/input/web_input_event_builders_mac.h"
#include "content/app_shim_remote_cocoa/render_widget_host_ns_view_bridge.h"
#include "content/app_shim_remote_cocoa/render_widget_host_ns_view_host_helper.h"
#include "content/app_shim_remote_cocoa/web_contents_ns_view_bridge.h"
#include "content/common/render_widget_host_ns_view.mojom.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"
#include "content/public/browser/remote_cocoa.h"
#include "content/public/browser/render_widget_host_view_mac_delegate.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/cocoa/remote_accessibility_api.h"
#include "ui/events/cocoa/cocoa_event_utils.h"

namespace remote_cocoa {

namespace {

class RenderWidgetHostNSViewBridgeOwner
    : public RenderWidgetHostNSViewHostHelper {
 public:
  explicit RenderWidgetHostNSViewBridgeOwner(
      uint64_t view_id,
      mojo::PendingAssociatedRemote<mojom::RenderWidgetHostNSViewHost> client,
      mojo::PendingAssociatedReceiver<mojom::RenderWidgetHostNSView>
          bridge_receiver,
      remote_cocoa::RenderWidgetHostViewMacDelegateCallback
          responder_delegate_creation_callback)
      : host_(std::move(client),
              ui::WindowResizeHelperMac::Get()->task_runner()) {
    bridge_ = std::make_unique<remote_cocoa::RenderWidgetHostNSViewBridge>(
        host_.get(), this, view_id,
        base::BindOnce(&RenderWidgetHostNSViewBridgeOwner::OnMojoDisconnect,
                       base::Unretained(this)));
    bridge_->BindReceiver(std::move(bridge_receiver));
    host_.set_disconnect_handler(
        base::BindOnce(&RenderWidgetHostNSViewBridgeOwner::OnMojoDisconnect,
                       base::Unretained(this)));

    if (responder_delegate_creation_callback) {
      [bridge_->GetNSView()
          setResponderDelegate:std::move(responder_delegate_creation_callback)
                                   .Run()];
    }
  }

  RenderWidgetHostNSViewBridgeOwner(const RenderWidgetHostNSViewBridgeOwner&) =
      delete;
  RenderWidgetHostNSViewBridgeOwner& operator=(
      const RenderWidgetHostNSViewBridgeOwner&) = delete;

 private:
  NSAccessibilityRemoteUIElement* __strong remote_accessibility_element_;
  void OnMojoDisconnect() { delete this; }

  std::unique_ptr<blink::WebCoalescedInputEvent> TranslateEvent(
      const blink::WebInputEvent& web_event) {
    return std::make_unique<blink::WebCoalescedInputEvent>(
        web_event.Clone(), std::vector<std::unique_ptr<blink::WebInputEvent>>{},
        std::vector<std::unique_ptr<blink::WebInputEvent>>{},
        ui::LatencyInfo());
  }

  id GetAccessibilityElement() override {
    if (!remote_accessibility_element_) {
      base::ProcessId browser_pid = base::kNullProcessId;
      std::vector<uint8_t> element_token;
      host_->GetRenderWidgetAccessibilityToken(&browser_pid, &element_token);
      [NSAccessibilityRemoteUIElement
          registerRemoteUIProcessIdentifier:browser_pid];
      remote_accessibility_element_ =
          ui::RemoteAccessibility::GetRemoteElementFromToken(element_token);
    }
    return remote_accessibility_element_;
  }

  // RenderWidgetHostNSViewHostHelper implementation.
  id GetRootBrowserAccessibilityElement() override {
    // The RenderWidgetHostViewCocoa in the app shim process does not
    // participate in the accessibility tree. Only the instance in the browser
    // process does.
    return nil;
  }
  id GetFocusedBrowserAccessibilityElement() override {
    // Some ATs (e.g. Text To Speech) need to access the focused
    // element in the app shim process. We make these apps work by
    // returning the `accessibilityFocusedUIElement` of the BridgedContentView,
    // which is an NSAccessibilityRemoteUIElement in app shim process.
    NSView* bridgedContentView = [[bridge_->GetNSView() superview] superview];
    return [bridgedContentView accessibilityFocusedUIElement];
  }
  void SetAccessibilityWindow(NSWindow* window) override {
    host_->SetRemoteAccessibilityWindowToken(
        ui::RemoteAccessibility::GetTokenForLocalElement(window));
  }

  void ForwardKeyboardEvent(const input::NativeWebKeyboardEvent& key_event,
                            const ui::LatencyInfo& latency_info) override {
    ForwardKeyboardEventWithCommands(
        key_event, latency_info, std::vector<blink::mojom::EditCommandPtr>());
  }
  void ForwardKeyboardEventWithCommands(
      const input::NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      std::vector<blink::mojom::EditCommandPtr> edit_commands) override {
    const blink::WebKeyboardEvent* web_event =
        static_cast<const blink::WebKeyboardEvent*>(&key_event);
    std::unique_ptr<blink::WebCoalescedInputEvent> input_event =
        std::make_unique<blink::WebCoalescedInputEvent>(
            web_event->Clone(),
            std::vector<std::unique_ptr<blink::WebInputEvent>>{},
            std::vector<std::unique_ptr<blink::WebInputEvent>>{}, latency_info);
    std::vector<uint8_t> native_event_data =
        ui::EventToData(key_event.os_event.Get());
    host_->ForwardKeyboardEventWithCommands(
        std::move(input_event), native_event_data, key_event.skip_if_unhandled,
        std::move(edit_commands));
  }
  void RouteOrProcessMouseEvent(
      const blink::WebMouseEvent& web_event) override {
    host_->RouteOrProcessMouseEvent(TranslateEvent(web_event));
  }
  void RouteOrProcessTouchEvent(
      const blink::WebTouchEvent& web_event) override {
    host_->RouteOrProcessTouchEvent(TranslateEvent(web_event));
  }
  void RouteOrProcessWheelEvent(
      const blink::WebMouseWheelEvent& web_event) override {
    host_->RouteOrProcessWheelEvent(TranslateEvent(web_event));
  }
  void ForwardMouseEvent(const blink::WebMouseEvent& web_event) override {
    host_->ForwardMouseEvent(TranslateEvent(web_event));
  }
  void ForwardWheelEvent(const blink::WebMouseWheelEvent& web_event) override {
    host_->ForwardWheelEvent(TranslateEvent(web_event));
  }
  void GestureBegin(blink::WebGestureEvent begin_event,
                    bool is_synthetically_injected) override {
    // The gesture type is not yet known, but assign a type to avoid
    // serialization asserts (the type will be stripped on the other side).
    begin_event.SetType(blink::WebInputEvent::Type::kGestureScrollBegin);
    host_->GestureBegin(TranslateEvent(begin_event), is_synthetically_injected);
  }
  void GestureUpdate(blink::WebGestureEvent update_event) override {
    host_->GestureUpdate(TranslateEvent(update_event));
  }
  void GestureEnd(blink::WebGestureEvent end_event) override {
    host_->GestureEnd(TranslateEvent(end_event));
  }
  void SmartMagnify(const blink::WebGestureEvent& web_event) override {
    host_->SmartMagnify(TranslateEvent(web_event));
  }

  mojo::AssociatedRemote<mojom::RenderWidgetHostNSViewHost> host_;
  std::unique_ptr<RenderWidgetHostNSViewBridge> bridge_;
};
}

void CreateRenderWidgetHostNSView(
    uint64_t view_id,
    mojo::ScopedInterfaceEndpointHandle host_handle,
    mojo::ScopedInterfaceEndpointHandle view_receiver_handle,
    RenderWidgetHostViewMacDelegateCallback
        responder_delegate_creation_callback) {
  // Cast from the stub interface to the mojom::RenderWidgetHostNSViewHost
  // and mojom::RenderWidgetHostNSView private interfaces.
  // TODO(ccameron): Remove the need for this cast.
  // https://crbug.com/888290
  mojo::PendingAssociatedRemote<mojom::RenderWidgetHostNSViewHost> host(
      std::move(host_handle), 0);

  // Create a RenderWidgetHostNSViewBridgeOwner. The resulting object will be
  // destroyed when its underlying pipe is closed.
  std::ignore = new RenderWidgetHostNSViewBridgeOwner(
      view_id, std::move(host),
      mojo::PendingAssociatedReceiver<mojom::RenderWidgetHostNSView>(
          std::move(view_receiver_handle)),
      std::move(responder_delegate_creation_callback));
}

void CreateWebContentsNSView(
    uint64_t view_id,
    mojo::ScopedInterfaceEndpointHandle host_handle,
    mojo::ScopedInterfaceEndpointHandle view_request_handle) {
  mojo::PendingAssociatedRemote<mojom::WebContentsNSViewHost> host(
      std::move(host_handle), 0);
  mojo::PendingAssociatedReceiver<mojom::WebContentsNSView> ns_view_receiver(
      std::move(view_request_handle));
  // Note that the resulting object will be destroyed when its underlying pipe
  // is closed.
  (new WebContentsNSViewBridge(view_id, std::move(host)))
      ->Bind(std::move(ns_view_receiver),
             ui::WindowResizeHelperMac::Get()->task_runner());
}

}  // namespace remote_cocoa
