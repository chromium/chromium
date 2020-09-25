// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_WIDGET_TEST_PROXY_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_WIDGET_TEST_PROXY_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/render_widget.h"
#include "content/web_test/renderer/event_sender.h"
#include "third_party/blink/public/web/web_widget_client.h"

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace content {
class RenderViewImpl;
}

namespace content {

class TestRunner;
class EventSender;
class WebViewTestProxy;

// WebWidgetTestProxy is used to run web tests. This class is a partial fake
// implementation of RenderWidget that overrides the minimal necessary portions
// of RenderWidget to allow for use in web tests.
//
// This method of injecting test functionality is an outgrowth of legacy.
// In particular, classic dependency injection does not work easily
// because the RenderWidget class is too large with too much entangled
// stated, and complex creation (subclass using heavy implementation
// inheritance, multiple modes of operation for frames/popups/fullscreen, etc)
// making it hard to factor our creation points for injection.
//
// While implementing a fake via partial overriding of a class leads to
// a fragile base class problem and implicit coupling of the test code
// and production code, it is the most viable mechanism available with a huge
// refactor.
//
// Historically, the overridden functionality has been small enough to not
// cause too much trouble. If that changes, then this entire testing
// architecture should be revisited.
class WebWidgetTestProxy : public RenderWidget {
 public:
  template <typename... Args>
  explicit WebWidgetTestProxy(Args&&... args)
      : RenderWidget(std::forward<Args>(args)...) {}
  ~WebWidgetTestProxy() override;

  // RenderWidget overrides.
  void WillBeginMainFrame() override;
  void RequestPresentation(PresentationTimeCallback callback) override;

  // WebWidgetClient implementation.
  void ScheduleAnimation() override;
  void ScheduleAnimationForWebTests() override;
  bool RequestPointerLock(blink::WebLocalFrame* requester_frame,
                          blink::WebWidgetClient::PointerLockCallback callback,
                          bool request_unajusted_movement) override;
  bool RequestPointerLockChange(
      blink::WebLocalFrame* requester_frame,
      blink::WebWidgetClient::PointerLockCallback callback,
      bool request_unadjusted_movement) override;
  void RequestPointerUnlock() override;
  bool IsPointerLocked() override;
  bool InterceptStartDragging(const blink::WebDragData& data,
                              blink::DragOperationsMask mask,
                              const SkBitmap& drag_image,
                              const gfx::Point& image_offset) override;

  // In the test runner code, it can be expected that the RenderViewImpl will
  // actually be a WebViewTestProxy as the creation of RenderView/Frame/Widget
  // are all hooked at the same time to provide a consistent set of fake
  // objects.
  WebViewTestProxy* GetWebViewTestProxy();

  // WebWidgetTestProxy is always a widget for a RenderFrame, so its WebWidget
  // is always a WebFrameWidget.
  blink::WebFrameWidget* GetWebFrameWidget();

  EventSender* event_sender() { return &event_sender_; }
  void Reset();
  void Install(blink::WebLocalFrame* frame);

  // Called to composite when the test has ended, in order to ensure the test
  // produces up-to-date pixel output. This is a separate path as most
  // compositing paths stop running when the test ends, to avoid tests running
  // forever.
  void SynchronouslyCompositeAfterTest();

 private:
  TestRunner* GetTestRunner();

  void ScheduleAnimationInternal(bool do_raster);
  void AnimateNow();

  // When |do_raster| is false, only a main frame animation step is performed,
  // but when true, a full composite is performed and a frame submitted to the
  // display compositor if there is any damage.
  // Note that compositing has the potential to detach the current frame and
  // thus destroy |this| before returning.
  void SynchronouslyComposite(bool do_raster);

  // Perform the synchronous composite step for a given RenderWidget.
  static void DoComposite(RenderWidget* widget, bool do_raster);

  EventSender event_sender_{this};

  // For collapsing multiple simulated ScheduleAnimation() calls.
  bool animation_scheduled_ = false;
  // When true, an AnimateNow() is scheduled that will perform a full composite.
  // Otherwise, any scheduled AnimateNow() calls will only perform the animation
  // step, which calls out to blink but doesn't composite for performance
  // reasons. See setAnimationRequiresRaster() in
  // https://chromium.googlesource.com/chromium/src/+/master/docs/testing/writing_web_tests.md
  // for details on the optimization.
  bool composite_requested_ = false;
  // Synchronous composites should not be nested inside another
  // composite, and this bool is used to guard against that.
  bool in_synchronous_composite_ = false;

  base::WeakPtrFactory<WebWidgetTestProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebWidgetTestProxy);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_WIDGET_TEST_PROXY_H_
