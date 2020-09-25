// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_widget_test_proxy.h"

#include "content/renderer/compositor/compositor_dependencies.h"
#include "content/web_test/renderer/blink_test_helpers.h"
#include "content/web_test/renderer/test_runner.h"
#include "content/web_test/renderer/web_view_test_proxy.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "ui/display/display_switches.h"

namespace content {

WebWidgetTestProxy::~WebWidgetTestProxy() = default;

void WebWidgetTestProxy::WillBeginMainFrame() {
  // WillBeginMainFrame occurs before we run BeginMainFrame() in the base
  // class, which will change states. TestFinished() wants to grab the current
  // state.
  GetTestRunner()->FinishTestIfReady();

  RenderWidget::WillBeginMainFrame();
}

void WebWidgetTestProxy::ScheduleAnimation() {
  if (GetTestRunner()->TestIsRunning())
    ScheduleAnimationInternal(GetTestRunner()->animation_requires_raster());
}

void WebWidgetTestProxy::ScheduleAnimationForWebTests() {
  // Single threaded web tests must explicitly schedule commits.
  //
  // Pass true for |do_raster| to ensure the compositor is actually run, rather
  // than just doing the main frame animate step. That way we know it will
  // submit a frame and later trigger the presentation callback in order to make
  // progress in the test.
  if (GetTestRunner()->TestIsRunning())
    ScheduleAnimationInternal(/*do_raster=*/true);
}

void WebWidgetTestProxy::RequestPresentation(
    PresentationTimeCallback callback) {
  RenderWidget::RequestPresentation(std::move(callback));
  ScheduleAnimationForWebTests();
}

void WebWidgetTestProxy::ScheduleAnimationInternal(bool do_raster) {
  // When using threaded compositing, have the RenderWidget schedule a request
  // for a frame, as we use the compositor's scheduler. Otherwise the testing
  // WebWidgetClient schedules it.
  // Note that for WebWidgetTestProxy the RenderWidget is subclassed to override
  // the WebWidgetClient, so we must call up to the base class RenderWidget
  // explicitly here to jump out of the test harness as intended.
  if (!RenderWidget::compositor_deps()->IsSingleThreaded()) {
    RenderWidget::ScheduleAnimation();
    return;
  }

  // If an animation already scheduled we'll make it composite, otherwise we'll
  // schedule another animation step with composite now.
  composite_requested_ |= do_raster;

  if (!animation_scheduled_) {
    animation_scheduled_ = true;

    auto* web_widget = static_cast<blink::WebFrameWidget*>(GetWebWidget());
    blink::WebLocalFrame* frame = web_widget->LocalRoot();

    frame->GetTaskRunner(blink::TaskType::kInternalTest)
        ->PostDelayedTask(FROM_HERE,
                          base::BindOnce(&WebWidgetTestProxy::AnimateNow,
                                         weak_factory_.GetWeakPtr()),
                          base::TimeDelta::FromMilliseconds(1));
  }
}

bool WebWidgetTestProxy::RequestPointerLock(
    blink::WebLocalFrame* requester_frame,
    blink::WebWidgetClient::PointerLockCallback callback,
    bool request_unadjusted_movement) {
  return event_sender_.RequestPointerLock(requester_frame, std::move(callback));
}

bool WebWidgetTestProxy::RequestPointerLockChange(
    blink::WebLocalFrame* requester_frame,
    blink::WebWidgetClient::PointerLockCallback callback,
    bool request_unadjusted_movement) {
  // This isn't implemented yet for web tests.
  CHECK(false);
  return false;
}

void WebWidgetTestProxy::RequestPointerUnlock() {
  event_sender_.RequestPointerUnlock();
}

bool WebWidgetTestProxy::IsPointerLocked() {
  return event_sender_.IsPointerLocked();
}

bool WebWidgetTestProxy::InterceptStartDragging(
    const blink::WebDragData& data,
    blink::DragOperationsMask mask,
    const SkBitmap& drag_image,
    const gfx::Point& image_offset) {
  GetTestRunner()->SetDragImage(drag_image);

  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  event_sender_.DoDragDrop(data, mask);
  return true;
}

WebViewTestProxy* WebWidgetTestProxy::GetWebViewTestProxy() {
  if (delegate()) {
    // TODO(https://crbug.com/545684): Because WebViewImpl still inherits
    // from WebWidget and infact, before a frame is attached, IS actually
    // the WebWidget used in blink, it is not possible to walk the object
    // relations in the blink side to find the associated RenderViewImpl
    // consistently. Thus, here, just directly cast the delegate(). Since
    // all creations of RenderViewImpl in the test_runner layer haved been
    // shimmed to return a WebViewTestProxy, it is safe to downcast here.
    return static_cast<WebViewTestProxy*>(delegate());
  } else {
    auto* web_widget = static_cast<blink::WebFrameWidget*>(GetWebWidget());
    blink::WebView* web_view = web_widget->LocalRoot()->View();

    RenderView* render_view = RenderView::FromWebView(web_view);
    // RenderViews are always WebViewTestProxy within the test_runner namespace.
    return static_cast<WebViewTestProxy*>(render_view);
  }
}

blink::WebFrameWidget* WebWidgetTestProxy::GetWebFrameWidget() {
  return static_cast<blink::WebFrameWidget*>(GetWebWidget());
}

void WebWidgetTestProxy::Reset() {
  event_sender_.Reset();
  // Ends any synthetic gestures started in |event_sender_|.
  GetWebWidget()->FlushInputProcessedCallback();

  // Reset state in the RenderWidget base class.
  GetWebFrameWidget()->ClearEditCommands();

  GetTestRunner()->ResetWebWidget(this);
}

void WebWidgetTestProxy::Install(blink::WebLocalFrame* frame) {
  event_sender_.Install(frame);
}

void WebWidgetTestProxy::SynchronouslyCompositeAfterTest() {
  // We could DCHECK(!GetTestRunner()->TestIsRunning()) except that frames in
  // other processes than the main frame do not hear when the test ends.

  // This would be very weird and prevent us from producing pixels.
  DCHECK(!in_synchronous_composite_);

  SynchronouslyComposite(/*do_raster=*/true);
}

TestRunner* WebWidgetTestProxy::GetTestRunner() {
  return GetWebViewTestProxy()->GetTestRunner();
}

// static
void WebWidgetTestProxy::DoComposite(content::RenderWidget* widget,
                                     bool do_raster) {
  // Ensure that there is damage so that the compositor submits, and the display
  // compositor draws this frame.
  if (do_raster) {
    widget->layer_tree_host()->SetNeedsCommitWithForcedRedraw();
  }

  widget->layer_tree_host()->Composite(base::TimeTicks::Now(), do_raster);
}

void WebWidgetTestProxy::SynchronouslyComposite(bool do_raster) {
  DCHECK(compositor_deps()->IsSingleThreaded());
  DCHECK(!layer_tree_host()->GetSettings().single_thread_proxy_scheduler);

  if (!layer_tree_host()->IsVisible())
    return;

  if (in_synchronous_composite_) {
    // Web tests can use a nested message loop to pump frames while inside a
    // frame, but the compositor does not support this. In this case, we only
    // run blink's lifecycle updates.
    GetWebWidget()->UpdateAllLifecyclePhases(
        blink::DocumentUpdateReason::kTest);
    return;
  }

  in_synchronous_composite_ = true;

  // Composite() can detach the frame, which would destroy |this|.
  base::WeakPtr<WebWidgetTestProxy> weak_this = weak_factory_.GetWeakPtr();
  DoComposite(this, do_raster);
  if (!weak_this)
    return;

  in_synchronous_composite_ = false;

  // If the RenderWidget is for the main frame, we also composite the current
  // PagePopup afterward.
  //
  // TODO(danakj): This means that an OOPIF's popup, which is attached to a
  // WebView without a main frame, would have no opportunity to execute this
  // method call.
  if (delegate()) {
    blink::WebView* view = GetWebViewTestProxy()->GetWebView();
    if (blink::WebPagePopup* popup = view->GetPagePopup()) {
      auto* popup_render_widget =
          static_cast<RenderWidget*>(popup->GetClientForTesting());
      DoComposite(popup_render_widget, do_raster);
    }
  }
}

void WebWidgetTestProxy::AnimateNow() {
  bool do_raster = composite_requested_;
  animation_scheduled_ = false;
  composite_requested_ = false;
  // Composite may destroy |this|, so don't use it afterward.
  SynchronouslyComposite(do_raster);
}

}  // namespace content
