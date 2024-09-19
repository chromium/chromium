// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/web_test/web_test_web_frame_widget_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "content/web_test/renderer/event_sender.h"
#include "content/web_test/renderer/test_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

WebFrameWidget* FrameWidgetTestHelper::CreateTestWebFrameWidget(
    base::PassKey<WebLocalFrame> pass_key,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited,
    bool is_for_child_local_root,
    bool is_for_nested_main_frame,
    bool is_for_scalable_page,
    content::TestRunner* test_runner) {
  return MakeGarbageCollected<WebTestWebFrameWidgetImpl>(
      pass_key, std::move(frame_widget_host), std::move(frame_widget),
      std::move(widget_host), std::move(widget), std::move(task_runner),
      frame_sink_id, hidden, never_composited, is_for_child_local_root,
      is_for_nested_main_frame, is_for_scalable_page, test_runner);
}

WebTestWebFrameWidgetImpl::WebTestWebFrameWidgetImpl(
    base::PassKey<WebLocalFrame> pass_key,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited,
    bool is_for_child_local_root,
    bool is_for_nested_main_frame,
    bool is_for_scalable_page,
    content::TestRunner* test_runner)
    : WebFrameWidgetImpl(pass_key,
                         std::move(frame_widget_host),
                         std::move(frame_widget),
                         std::move(widget_host),
                         std::move(widget),
                         std::move(task_runner),
                         frame_sink_id,
                         hidden,
                         never_composited,
                         is_for_child_local_root,
                         is_for_nested_main_frame,
                         is_for_scalable_page),
      test_runner_(test_runner) {}

WebTestWebFrameWidgetImpl::~WebTestWebFrameWidgetImpl() = default;

void WebTestWebFrameWidgetImpl::BindLocalRoot(WebLocalFrame& local_root) {
  WebFrameWidgetImpl::BindLocalRoot(local_root);
  // We need to initialize EventSender after the binding of the local root
  // as the EventSender constructor accesses LocalRoot and that is not
  // set until BindLocalRoot is called.
  event_sender_ = std::make_unique<content::EventSender>(this, test_runner_);
}

void WebTestWebFrameWidgetImpl::WillBeginMainFrame() {
  // WillBeginMainFrame occurs before we run BeginMainFrame() in the base
  // class, which will change states. TestFinished() wants to grab the current
  // state.
  GetTestRunner()->FinishTestIfReady(*LocalRootImpl());

  WebFrameWidgetImpl::WillBeginMainFrame();
}

void WebTestWebFrameWidgetImpl::ScheduleAnimation() {
  ScheduleAnimationInternal(GetTestRunner()->animation_requires_raster());
}

void WebTestWebFrameWidgetImpl::ScheduleAnimationForWebTests() {
  // Single threaded web tests must explicitly schedule commits.
  //
  // Pass true for |do_raster| to ensure the compositor is actually run, rather
  // than just doing the main frame animate step. That way we know it will
  // submit a frame and later trigger the presentation callback in order to make
  // progress in the test.
  ScheduleAnimationInternal(/*do_raster=*/true);
}

void WebTestWebFrameWidgetImpl::WasShown(bool was_evicted) {
  WebFrameWidgetImpl::WasShown(was_evicted);

  if (animation_deferred_while_hidden_) {
    animation_deferred_while_hidden_ = false;
    ScheduleAnimationInternal(composite_requested_);
  }
}

void WebTestWebFrameWidgetImpl::UpdateAllLifecyclePhasesAndComposite(
    base::OnceClosure callback) {
  LayerTreeHost()->RequestSuccessfulPresentationTimeForNextFrame(
      base::IgnoreArgs<const viz::FrameTimingDetails&>(std::move(callback)));
  LayerTreeHost()->SetNeedsCommitWithForcedRedraw();
  ScheduleAnimationForWebTests();
}

void WebTestWebFrameWidgetImpl::ScheduleAnimationInternal(bool do_raster) {
  if (!GetTestRunner()->TestIsRunning()) {
    return;
  }

  // When using threaded compositing, have the WeFrameWidgetImpl normally
  // schedule a request for a frame, as we use the compositor's scheduler.
  if (Thread::CompositorThread()) {
    WebFrameWidgetImpl::ScheduleAnimation();
    return;
  }

  // If an animation already scheduled we'll make it composite, otherwise we'll
  // schedule another animation step with composite now.
  composite_requested_ |= do_raster;

  if (!animation_scheduled_) {
    animation_scheduled_ = true;

    WebLocalFrame* frame = LocalRoot();

    frame->GetTaskRunner(TaskType::kInternalTest)
        ->PostDelayedTask(FROM_HERE,
                          WTF::BindOnce(&WebTestWebFrameWidgetImpl::AnimateNow,
                                        WrapWeakPersistent(this)),
                          base::Milliseconds(1));
  }
}

bool WebTestWebFrameWidgetImpl::RequestedMainFramePending() {
  if (Thread::CompositorThread()) {
    return WebFrameWidgetImpl::RequestedMainFramePending();
  }
  return animation_scheduled_;
}

void WebTestWebFrameWidgetImpl::StartDragging(
    LocalFrame* source_frame,
    const WebDragData& data,
    DragOperationsMask mask,
    const SkBitmap& drag_image,
    const gfx::Vector2d& cursor_offset,
    const gfx::Rect& drag_obj_rect) {
  if (!GetTestRunner()->AutomaticDragDropEnabled()) {
    return WebFrameWidgetImpl::StartDragging(
        source_frame, data, mask, drag_image, cursor_offset, drag_obj_rect);
  }

  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  doing_drag_and_drop_ = true;
  GetTestRunner()->SetDragImage(drag_image);
  event_sender_->DoDragDrop(data, mask);
}

FrameWidgetTestHelper*
WebTestWebFrameWidgetImpl::GetFrameWidgetTestHelperForTesting() {
  return this;
}

void WebTestWebFrameWidgetImpl::Reset() {
  event_sender_->Reset();

  // Ends any synthetic gestures started in |event_sender_|.
  FlushInputProcessedCallback();

  // Reset state in the  base class.
  ClearEditCommands();

  SetDeviceScaleFactorForTesting(0);
  ReleaseMouseLockAndPointerCaptureForTesting();

  // These things are only modified/valid for the main frame's widget.
  if (ForMainFrame()) {
    ResetZoomLevelForTesting();

    SetMainFrameOverlayColor(SK_ColorTRANSPARENT);
    SetTextZoomFactor(1);
    LocalRootImpl()
        ->GetFrame()
        ->GetEventHandler()
        .ResetLastMousePositionForWebTest();
  }
}

content::EventSender* WebTestWebFrameWidgetImpl::GetEventSender() {
  return event_sender_.get();
}

void WebTestWebFrameWidgetImpl::SynchronouslyCompositeAfterTest(
    base::OnceClosure callback) {
  // We could DCHECK(!GetTestRunner()->TestIsRunning()) except that frames in
  // other processes than the main frame do not hear when the test ends.

  // This would be very weird and prevent us from producing pixels.
  DCHECK(!in_synchronous_composite_);

  SynchronouslyComposite(std::move(callback), /*do_raster=*/true);
}

content::TestRunner* WebTestWebFrameWidgetImpl::GetTestRunner() {
  return test_runner_;
}

// static
void WebTestWebFrameWidgetImpl::DoComposite(cc::LayerTreeHost* layer_tree_host,
                                            bool do_raster,
                                            base::OnceClosure callback) {
  // Ensure that there is damage so that the compositor submits, and the display
  // compositor draws this frame.
  if (do_raster) {
    layer_tree_host->SetNeedsCommitWithForcedRedraw();
  }

  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), do_raster,
                                    std::move(callback));
}

void WebTestWebFrameWidgetImpl::SynchronouslyComposite(
    base::OnceClosure callback,
    bool do_raster) {
  if (!LocalRootImpl()->ViewImpl()->does_composite()) {
    if (callback) {
      std::move(callback).Run();
    }
    return;
  }
  DCHECK(!LayerTreeHost()->GetSettings().single_thread_proxy_scheduler);

  if (!LayerTreeHost()->IsVisible()) {
    if (callback) {
      std::move(callback).Run();
    }
    return;
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kNoForcedFrameUpdatesForWebTests) &&
      LayerTreeHost()->MainFrameUpdatesAreDeferred()) {
    if (callback) {
      std::move(callback).Run();
    }
    return;
  }

  if (in_synchronous_composite_) {
    // Web tests can use a nested message loop to pump frames while inside a
    // frame, but the compositor does not support this. In this case, we only
    // run blink's lifecycle updates.
    UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
    if (callback) {
      std::move(callback).Run();
    }
    return;
  }

  in_synchronous_composite_ = true;

  auto wrapped_callback = WTF::BindOnce(
      [](base::OnceClosure cb, bool* in_synchronous_composite) {
        *in_synchronous_composite = false;
        if (cb) {
          std::move(cb).Run();
        }
      },
      // base::Unretained is safe by construction, because WebFrameWidgetImpl
      // must always outlive the compositing machinery.
      std::move(callback), base::Unretained(&in_synchronous_composite_));

  // If there's a visible popup, then we will update its compositing after
  // updating the host frame.
  WebPagePopupImpl* popup = LocalRootImpl()->ViewImpl()->GetPagePopup();

  if (!popup) {
    DoComposite(LayerTreeHost(), do_raster, std::move(wrapped_callback));
    return;
  }

  DoComposite(LayerTreeHost(), do_raster, base::OnceClosure());

  // DoComposite() can detach the frame, in which case we don't update the
  // popup. Because DoComposite was called with a no-op callback, we need to run
  // the actual callback here.
  if (!LocalRoot()) {
    std::move(wrapped_callback).Run();
    return;
  }

  DoComposite(popup->LayerTreeHostForTesting(), do_raster,
              std::move(wrapped_callback));
}

void WebTestWebFrameWidgetImpl::AnimateNow() {
  // If we have been Closed but not destroyed yet, return early.
  if (!LocalRootImpl()) {
    return;
  }

  animation_scheduled_ = false;

  if (LocalRootImpl()->ViewImpl()->does_composite() &&
      !LayerTreeHost()->IsVisible()) {
    // If the widget is hidden, SynchronouslyComposite will early-out which may
    // leave a test waiting (e.g. waiting on a requestAnimationFrame). Setting
    // this bit will reschedule the animation request when the widget becomes
    // visible.
    animation_deferred_while_hidden_ = true;
    return;
  }

  bool do_raster = composite_requested_;
  composite_requested_ = false;
  // Composite may destroy |this|, so don't use it afterward.
  SynchronouslyComposite(base::OnceClosure(), do_raster);
}

void WebTestWebFrameWidgetImpl::RequestDecode(
    const PaintImage& image,
    base::OnceCallback<void(bool)> callback) {
  WebFrameWidgetImpl::RequestDecode(image, std::move(callback));

  // In web tests the request does not actually cause a commit, because the
  // compositor is scheduled by the test runner to avoid flakiness. So for this
  // case we must request a main frame.
  ScheduleAnimationForWebTests();
}

void WebTestWebFrameWidgetImpl::DidAutoResize(const gfx::Size& size) {
  WebFrameWidgetImpl::DidAutoResize(size);

  // Window rect resize for threaded compositing is delivered via requesting a
  // new surface. The browser then reacts to the bounds of the surface changing
  // and adjusts the WindowRect. For single threaded compositing the
  // surface size is never processed so we force the WindowRect to be the
  // same size as the WidgetSize when AutoResize is applied.
  if (LayerTreeHost()->IsSingleThreaded()) {
    gfx::Rect new_pos(Size());
    SetWindowRect(new_pos, new_pos);
  }
}

}  // namespace blink
