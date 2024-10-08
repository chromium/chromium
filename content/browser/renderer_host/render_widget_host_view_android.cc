// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>

#include <limits>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "cc/base/math_util.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/slim/layer.h"
#include "components/input/input_router.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/web_input_event_builders_android.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/accessibility/web_contents_accessibility_android.h"
#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/android/ime_adapter_android.h"
#include "content/browser/android/overscroll_controller_android.h"
#include "content/browser/android/selection/selection_popup_controller.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/android/text_suggestion_host_android.h"
#include "content/browser/bad_message.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/delegated_frame_host_client_android.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_manager_android.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/common/content_switches_internal.h"
#include "content/common/features.h"
#include "content/common/input/events_helper.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display_util.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/touch_selection/selection_event_type.h"
#include "ui/touch_selection/touch_selection_controller.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/RenderWidgetHostViewImpl_jni.h"

namespace content {

namespace {

static const base::TimeDelta kClickCountInterval = base::Seconds(0.5);
static const float kClickCountRadiusSquaredDIP = 25;
static const base::TimeDelta kThrottleTimeout = base::Milliseconds(200);

std::unique_ptr<ui::TouchSelectionController> CreateSelectionController(
    ui::TouchSelectionControllerClient* client,
    bool has_view_tree) {
  DCHECK(client);
  DCHECK(has_view_tree);
  ui::TouchSelectionController::Config config;
  config.max_tap_duration =
      base::Milliseconds(gfx::ViewConfiguration::GetLongPressTimeoutInMs());
  config.tap_slop = gfx::ViewConfiguration::GetTouchSlopInDips();
  config.enable_adaptive_handle_orientation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAdaptiveSelectionHandleOrientation);
  config.enable_longpress_drag_selection =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLongpressDragSelection);
  config.hide_active_handle =
      base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P;
  return std::make_unique<ui::TouchSelectionController>(client, config);
}

gfx::RectF GetSelectionRect(const ui::TouchSelectionController& controller) {
  // When the touch handles are on the same line, the rect may become simply a
  // one-dimensional rect, and still need to union the handle rect to avoid the
  // context menu covering the touch handle. See detailed comments in
  // TouchSelectionController::GetRectBetweenBounds(). Ensure that the |rect| is
  // not empty by adding a pixel width or height to avoid the wrong menu
  // position.
  gfx::RectF rect = controller.GetVisibleRectBetweenBounds();
  if (rect.IsEmpty()) {
    gfx::SizeF size = rect.size();
    size.SetToMax(gfx::SizeF(1.0f, 1.0f));
    rect.set_size(size);
  }

  rect.Union(controller.GetStartHandleRect());
  rect.Union(controller.GetEndHandleRect());
  return rect;
}

void RecordToolTypeForActionDown(const ui::MotionEventAndroid& event) {
  ui::MotionEventAndroid::Action action = event.GetAction();
  if (action == ui::MotionEventAndroid::Action::DOWN ||
      action == ui::MotionEventAndroid::Action::POINTER_DOWN ||
      action == ui::MotionEventAndroid::Action::BUTTON_PRESS) {
    UMA_HISTOGRAM_ENUMERATION(
        "Event.AndroidActionDown.ToolType",
        static_cast<int>(event.GetToolType(0)),
        static_cast<int>(ui::MotionEventAndroid::ToolType::LAST) + 1);
  }
}

void WakeUpGpu(GpuProcessHost* host) {
  if (host)
    host->gpu_service()->WakeUpGpu();
}

std::string CompressAndSaveBitmap(const std::string& dir,
                                  const SkBitmap& bitmap) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  std::vector<unsigned char> data;
  if (!gfx::JPEGCodec::Encode(bitmap, 85, &data)) {
    LOG(ERROR) << "Failed to encode bitmap to JPEG";
    return std::string();
  }

  base::FilePath screenshot_dir(dir);
  if (!base::DirectoryExists(screenshot_dir)) {
    if (!base::CreateDirectory(screenshot_dir)) {
      LOG(ERROR) << "Failed to create screenshot directory";
      return std::string();
    }
  }

  base::FilePath screenshot_path;
  base::ScopedFILE out_file(base::CreateAndOpenTemporaryStreamInDir(
      screenshot_dir, &screenshot_path));
  if (!out_file) {
    LOG(ERROR) << "Failed to create temporary screenshot file";
    return std::string();
  }
  unsigned int bytes_written =
      fwrite(reinterpret_cast<const char*>(data.data()), 1, data.size(),
             out_file.get());
  out_file.reset();  // Explicitly close before a possible Delete below.

  // If there were errors, don't leave a partial file around.
  if (bytes_written != data.size()) {
    base::DeleteFile(screenshot_path);
    LOG(ERROR) << "Error writing screenshot file to disk";
    return std::string();
  }
  return screenshot_path.value();
}

blink::mojom::RecordContentToVisibleTimeRequestPtr
TakeContentToVisibleTimeRequest(RenderWidgetHostImpl* host) {
  return host->GetVisibleTimeRequestTrigger().TakeRequest();
}

}  // namespace

// static
RenderWidgetHostViewAndroid*
RenderWidgetHostViewAndroid::FromRenderWidgetHostView(
    RenderWidgetHostView* view) {
  if (!view || static_cast<RenderWidgetHostViewBase*>(view)
                   ->IsRenderWidgetHostViewChildFrame()) {
    return nullptr;
  }
  return static_cast<RenderWidgetHostViewAndroid*>(view);
}

RenderWidgetHostViewAndroid::ScreenStateChangeHandler::ScreenStateChangeHandler(
    RenderWidgetHostViewAndroid* rwhva)
    : rwhva_(rwhva) {}

bool RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    CanSynchronizeVisualProperties() const {
  if (pending_screen_state_.is_fullscreen &&
      !pending_screen_state_.any_non_rotation_size_changed) {
    return false;
  }
  return true;
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    OnVisibleViewportSizeChanged(const gfx::Size& visible_viewport_size) {
  // RendereWidgetHostImpl::SendScreenRects will send updated sizes to the
  // Renderer without waiting for SurfaceSync. In some fullscreen transitions
  // we receive neither OnPhysicalBackingChanged nor
  // OnSynchronizedDisplayPropertiesChanged. In those cases verify the new
  // screen state and cause a SurfaceSync so that the Renderer does not attempt
  // to submit new sizes to an old viz::LocalSurfaceId.
  pending_screen_state_.visible_viewport_size = visible_viewport_size;
  HandleScreenStateChanges(cc::DeadlinePolicy::UseDefaultDeadline());
}

bool RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    OnPhysicalBackingSizeChanged(const gfx::Size& physical_backing_size,
                                 int64_t deadline_in_frames) {
  // A fullscreen rotation can include a partial change in height for the
  // initial top-controls layout. Before the full layout arrives in a second
  // OnPhysicalBackingSizeChanged later.
  pending_screen_state_.physical_backing_size = physical_backing_size;
  return HandleScreenStateChanges(
      cc::DeadlinePolicy::UseSpecifiedDeadline(deadline_in_frames));
}

bool RenderWidgetHostViewAndroid::ScreenStateChangeHandler::OnScreenInfoChanged(
    const display::ScreenInfo& screen_info) {
  // TODO(crbug.com/13801170): Once the legacy Killswitch path has been
  // removed we should consider performing no SurfaceSync while hidden. For
  // example multiple conflicting ScreenInfo.rect changes can occur while
  // hidden and the Renderer is doing redundant work.
  pending_screen_state_.screen_info_size = screen_info.rect.size();
  pending_screen_state_.orientation_type = screen_info.orientation_type;
  return HandleScreenStateChanges(cc::DeadlinePolicy::UseDefaultDeadline());
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    EnterFullscreenMode() {
  BeginScreenStateChange();
  pending_screen_state_.is_fullscreen = true;
  HandleScreenStateChanges(cc::DeadlinePolicy::UseDefaultDeadline());

  if (throttle_timeout_.IsRunning())
    throttle_timeout_.Stop();
  throttle_timeout_.Start(
      FROM_HERE, kThrottleTimeout,
      base::BindOnce(
          &RenderWidgetHostViewAndroid::ScreenStateChangeHandler::Unthrottle,
          base::Unretained(this)));
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    ExitFullscreenMode() {
  // There is no guarantee that there will be any updates to visual properties
  // when exiting fullscreen. So we currently cannot throttle.
  // In some rare cases, when we exit fullscreen there is only the update to
  // `visible_viewport_rect`. Such as when we are in Fullscreen Landscape, and
  // are exiting to Landscape, but with a large enough scroll offset to have
  // no top-chrome visible.
  // When in split view, there are no changes to visual properties when exiting
  // fullscreen mode. Even when there are changes upon entering.
  // (crbug.com/1378754)
  BeginScreenStateChange();
  pending_screen_state_.is_fullscreen = false;
  HandleScreenStateChanges(cc::DeadlinePolicy::UseDefaultDeadline());
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::LockOrientation(
    device::mojom::ScreenOrientationLockType orientation) {
  // Orientation Lock is only supported during fullscreen.
  pending_screen_state_.is_expecting_fullscreen_rotation =
      !ScreenOrientationProvider::LockMatchesOrientation(
          orientation, rwhva_->GetScreenInfo().orientation_type);
  HandleScreenStateChanges(cc::DeadlinePolicy::UseDefaultDeadline());
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    UnlockOrientation() {
  BeginScreenStateChange();
  pending_screen_state_.is_expecting_fullscreen_rotation = false;
  pending_screen_state_.has_unlocked_orientation_lock = true;
  // The notification to unlock can occur after the first portion of a rotation
  // has begun. We are no longer guaranteed to receive the remainder of that
  // rotation, in fact a new one may begin. We stop throttling in these cases
  // and sync immediately.
  HandleScreenStateChanges(cc::DeadlinePolicy::UseDefaultDeadline());
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    SetHasPersistentVideo(bool has_persistent_video) {
  bool is_fullscreen = current_screen_state_.is_fullscreen;
  // Picture-in-Picture requires fullscreen, and stays in fullscreen.
  if (has_persistent_video)
    pre_picture_in_picture_ = current_screen_state_;
  else
    is_fullscreen = pending_screen_state_.is_fullscreen;

  BeginScreenStateChange();
  pending_screen_state_.is_picture_in_picture = has_persistent_video;
  pending_screen_state_.is_fullscreen = is_fullscreen;
  // TODO(crbug.com/40872802): We should try to re-establish throttling for
  // Picture-in-Picture mode. Will need better determination of when we have
  // completed entering/exiting.
  pending_screen_state_.any_non_rotation_size_changed = true;
  HandleScreenStateChanges(cc::DeadlinePolicy::UseDefaultDeadline());
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::WasEvicted() {
  // Reset the world upon eviction. We will re-esatblish the world when we next
  // become visible and begin embedding content again. This should not call
  // HandleScreenStateChanges, as we explicitly to not want to do any syncing
  // when we are evicted.
  BeginScreenStateChange();
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    WasShownAfterEviction() {
  // The screen state can change while we were evicted. Reset the world for
  // future changes.
  BeginScreenStateChange();
  HandleScreenStateChanges(cc::DeadlinePolicy::UseDefaultDeadline());
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    BeginScreenStateChange() {
  current_screen_state_.visible_viewport_size = rwhva_->view_.GetSize();
  current_screen_state_.physical_backing_size =
      rwhva_->view_.GetPhysicalBackingSize();
  auto screen_info = rwhva_->GetScreenInfo();
  current_screen_state_.screen_info_size = screen_info.rect.size();
  current_screen_state_.orientation_type = screen_info.orientation_type;
  current_screen_state_.local_surface_id =
      rwhva_->local_surface_id_allocator_.GetCurrentLocalSurfaceId();

  pending_screen_state_ = ScreenState();
}

bool RenderWidgetHostViewAndroid::ScreenStateChangeHandler::
    HandleScreenStateChanges(const cc::DeadlinePolicy& deadline_policy,
                             bool force_fullscreen_sync) {
  bool sync_needed =
      force_fullscreen_sync && pending_screen_state_.is_fullscreen !=
                                   current_screen_state_.is_fullscreen;
  bool start_rotation = false;
  bool end_rotation = false;
  bool exiting_pip = false;

  // When `visible_viewport_size` change is a non-rotation it may be inset
  // changes for System UI, or scaling changes for Picture-in-Picture. Stop
  // throttling fullscreen transitions now, as we cannot be certain if there
  // will be any subsequent updates.
  if (!pending_screen_state_.visible_viewport_size.IsEmpty() &&
      !ScreenState::IsRotation(current_screen_state_.visible_viewport_size,
                               pending_screen_state_.visible_viewport_size)) {
    pending_screen_state_.any_non_rotation_size_changed = true;
  }

  // TODO(crbug.com/40242839): We need a pre-Android S detection of
  // Picture-in-Picture mode. The `visible_viewport_size` and
  // `physical_backing_size` will be shrunk, though it is not guaranteed to be
  // simply a scale from the fullscreen size. As sometimes inset changes are
  // also applied.
  //
  // TODO(crbug.com/40872802): We should try to re-establish throttling for
  // Picture-in-Picture mode. Will need better determination of when we have
  // completed entering/exiting.
  if (pending_screen_state_.is_picture_in_picture) {
    if (rwhva_->in_rotation_)
      end_rotation = true;
    else
      sync_needed = true;
  } else if (pre_picture_in_picture_.IsValid()) {
    if (rwhva_->in_rotation_)
      end_rotation = true;
    else
      sync_needed = true;
    exiting_pip = true;
  } else if (pending_screen_state_.has_unlocked_orientation_lock &&
             rwhva_->in_rotation_) {
    end_rotation = true;
  } else if (!pending_screen_state_.is_fullscreen &&
             current_screen_state_.is_fullscreen) {
    // PWA and WebView may be created as Fullscreen, without marking the
    // WebContents as Fullscreen. In this state the Renderer can still request
    // to toggle Fullscreen, which enables the ScreenOrientation APIs. However
    // there will be no layout changes occuring.
    //
    // To account for this trigger a sync now to release the JavaScript Promise,
    // and to update our `current_screen_state_`.
    sync_needed = true;
  } else {
    bool physical_backing_rotation = false;
    bool screen_info_rotation = false;
    if (!pending_screen_state_.physical_backing_size.IsEmpty()) {
      // When transitioning to a split view, the physical backing will be
      // resized along one single axis. The resize can be significant enough to
      // be rotation, however there will be no subsequent rotation of the
      // ScreenInfo. So do not treat it as such.
      physical_backing_rotation =
          ScreenState::IsRotation(
              current_screen_state_.physical_backing_size,
              pending_screen_state_.physical_backing_size) &&
          !ScreenState::IsSingleAxisResize(
              current_screen_state_.physical_backing_size,
              pending_screen_state_.physical_backing_size);
      if (!physical_backing_rotation) {
        // Inset changes for System UI, or scaling changes for
        // Picture-in-Picture mode.
        pending_screen_state_.any_non_rotation_size_changed = true;
        // If we are expecting a rotation, start rotation throttle now anyways.
        // Otherwise we have no way to know if the transition will ever lead to
        // a rotation so just sync.
        if (pending_screen_state_.is_expecting_fullscreen_rotation) {
          start_rotation = true;
        } else if (rwhva_->in_rotation_) {
          // TODO(crbug.com/40244577): The legacy killswitch path, combined with
          // the legacy kOnShowWithPageVisibility path make it difficult to
          // refactor the hidden rotation handling. Once we clear those we
          // should consider no SurfaceSync while hidden. Then synchronizing the
          // entire world upon OnShowWithPageVisibility. For now detect being
          // left in a rotation throttle and ending it here.
          end_rotation = true;
        } else {
          sync_needed = true;
        }
      }
    }
    if (!pending_screen_state_.screen_info_size.IsEmpty()) {
      screen_info_rotation = ScreenState::ExpectsResizeForOrientationChange(
          current_screen_state_.orientation_type,
          pending_screen_state_.orientation_type);
      if (!screen_info_rotation) {
        pending_screen_state_.any_non_rotation_size_changed = true;
        // This can occur when there is a "rotation" from a primary to a
        // secondary variant of the `orientation_type`. Such as Portrait-Primary
        // to Portrait-Secondary. When this occurs we don't need to force a
        // sync, just update the `current_screen_state_` to be ready for any
        // future comparisons.
        if (pending_screen_state_.screen_info_size ==
            current_screen_state_.screen_info_size) {
          current_screen_state_.orientation_type =
              pending_screen_state_.orientation_type;
          pending_screen_state_.screen_info_size = gfx::Size();
          pending_screen_state_.orientation_type =
              display::mojom::ScreenOrientation::kUndefined;
          pending_screen_state_.on_sync_display_properties_changed_received =
              false;
          // If we are expecting a rotation, start rotation throttle now
          // anyways.
          if (pending_screen_state_.is_expecting_fullscreen_rotation)
            start_rotation = true;
        }
      }
    }

    if (physical_backing_rotation && screen_info_rotation) {
      end_rotation = true;
      pending_screen_state_.any_non_rotation_size_changed = true;
    } else if (physical_backing_rotation &&
               pending_screen_state_.screen_info_size.IsEmpty() &&
               !pending_screen_state_.on_physical_backing_changed_received) {
      // There can be repeated changes to `visible_viewport_rect` in-between the
      // physical backing and screen info updates. So only process the pending
      // state once.
      pending_screen_state_.on_physical_backing_changed_received = true;
      if (ScreenState::IsRotation(pending_screen_state_.physical_backing_size,
                                  current_screen_state_.screen_info_size)) {
        start_rotation = true;
      } else {
        // When transitioning from mixed orientation states, such as Landscape
        // Video in a Portrait Picture-in-Picture screen, we confirm the new
        // transition matches the current screen info.
        sync_needed = true;
      }
    } else if (screen_info_rotation &&
               pending_screen_state_.physical_backing_size.IsEmpty() &&
               !pending_screen_state_
                    .on_sync_display_properties_changed_received) {
      // There can be repeated changes to `visible_viewport_rect` in-between the
      // physical backing and screen info updates. So only process the pending
      // state once.
      pending_screen_state_.on_sync_display_properties_changed_received = true;
      // ScreenInfo explicitly lists an orientation, we always start a rotation
      // when requested. It is possible in split-screen for
      // `physical_backing_size` to become in a mixed orientation states, so we
      // do not compare to them.
      start_rotation = true;
    }
  }

  if (!start_rotation && !end_rotation && !sync_needed)
    return false;

  if (start_rotation) {
    rwhva_->BeginRotationBatching();
    return true;
  } else if (end_rotation) {
    // The rotation timeout is intended to catch edge-cases where Android::View
    // code does not give us some aspects of re-layouts. However on slower
    // devices the timeout may fire before the final signals arrive. In these
    // cases call BeginRotationBatching to properly enqueue the rotation, before
    // immediately embedding the new content.
    if (!rwhva_->in_rotation_)
      rwhva_->BeginRotationBatching();
    rwhva_->EndRotationAndSyncIfNecessary();
  } else if (sync_needed) {
    // If any sync is recorded, disable the fullscreen throttling.
    if (pending_screen_state_.is_fullscreen) {
      pending_screen_state_.any_non_rotation_size_changed = true;
    }
    rwhva_->SynchronizeVisualProperties(
        deadline_policy, std::nullopt,
        /*reuse_current_local_surface_id=*/false,
        /*ignore_ack=*/true);
  }

  current_screen_state_.CopyDefinedAttributes(pending_screen_state_);
  current_screen_state_.local_surface_id =
      rwhva_->local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  pending_screen_state_ = ScreenState();
  pending_screen_state_.is_fullscreen = current_screen_state_.is_fullscreen;
  pending_screen_state_.is_picture_in_picture =
      current_screen_state_.is_picture_in_picture;
  pending_screen_state_.any_non_rotation_size_changed =
      current_screen_state_.any_non_rotation_size_changed;

  // When exiting Picture-in-Picture mode, we can sometimes return to the same
  // state. We can sometimes be in the same orientation, but the insets have
  // changes. Or we can be in a rotation of the original state. Each is valid
  // and a signal we are done the transition.
  if (exiting_pip &&
      ((pre_picture_in_picture_.EqualVisualProperties(current_screen_state_)) ||
       (pre_picture_in_picture_.IsValid() && current_screen_state_.IsValid() &&
        (pre_picture_in_picture_.EqualOrientations(current_screen_state_) ||
         pre_picture_in_picture_.IsRotated(current_screen_state_))))) {
    pre_picture_in_picture_ = ScreenState();
  }

  return true;
}

void RenderWidgetHostViewAndroid::ScreenStateChangeHandler::Unthrottle() {
  pending_screen_state_.any_non_rotation_size_changed = true;
  HandleScreenStateChanges(cc::DeadlinePolicy::UseDefaultDeadline(),
                           true /* force_fullscreen_sync */);
}

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    gfx::NativeView parent_native_view,
    cc::slim::Layer* parent_layer)
    : RenderWidgetHostViewBase(widget_host),
      is_showing_(!widget_host->is_hidden()),
      is_window_visible_(true),
      is_window_activity_started_(true),
      ime_adapter_android_(nullptr),
      selection_popup_controller_(nullptr),
      text_suggestion_host_(nullptr),
      gesture_listener_manager_(nullptr),
      view_(ui::ViewAndroid::LayoutType::MATCH_PARENT),
      gesture_provider_(
          ui::GetGestureProviderConfig(
              ui::GestureProviderConfigType::CURRENT_PLATFORM,
              GetUIThreadTaskRunner({BrowserTaskType::kUserInput})),
          this),
      stylus_text_selector_(this),
      using_browser_compositor_(CompositorImpl::IsInitialized()),
      synchronous_compositor_client_(nullptr),
      observing_root_window_(false),
      prev_top_shown_pix_(0.f),
      prev_top_controls_pix_(0.f),
      prev_top_controls_translate_(0.f),
      prev_top_controls_min_height_offset_pix_(0.f),
      prev_bottom_shown_pix_(0.f),
      prev_bottom_controls_translate_(0.f),
      prev_bottom_controls_min_height_offset_pix_(0.f),
      page_scale_(1.f),
      min_page_scale_(1.f),
      max_page_scale_(1.f),
      mouse_wheel_phase_handler_(this),
      screen_state_change_handler_(this) {
  // Set the layer which will hold the content layer for this view. The content
  // layer is managed by the DelegatedFrameHost.
  view_.SetLayer(cc::slim::Layer::Create());
  view_.set_event_handler(this);

  // If we're showing at creation time, we won't get a visibility change, so
  // generate our initial LocalSurfaceId here.
  if (is_showing_)
    local_surface_id_allocator_.GenerateId();

  delegated_frame_host_client_ =
      std::make_unique<DelegatedFrameHostClientAndroid>(this);
  delegated_frame_host_ = std::make_unique<ui::DelegatedFrameHostAndroid>(
      &view_, GetHostFrameSinkManager(), delegated_frame_host_client_.get(),
      host()->GetFrameSinkId());
  if (is_showing_) {
    delegated_frame_host_->WasShown(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        GetCompositorViewportPixelSize(), host()->delegate()->IsFullscreen(),
        TakeContentToVisibleTimeRequest(host()));
  }

  host()->SetView(this);
  touch_selection_controller_client_manager_ =
      std::make_unique<TouchSelectionControllerClientManagerAndroid>(this);

  // `parent_native_view` and `parent_layer` must be null or non-null at the
  // same time.
  CHECK(!(!!parent_native_view ^ !!parent_layer));

  UpdateNativeViewTree(parent_native_view, parent_layer);
  // This RWHVA may have been created speculatively. We should give any
  // existing RWHVAs priority for receiving input events, otherwise a
  // speculative RWHVA could be sent input events intended for the currently
  // showing RWHVA.
  if (parent_native_view) {
    parent_native_view->MoveToBack(&view_);
  }

  CreateOverscrollControllerIfPossible();

  if (GetTextInputManager())
    GetTextInputManager()->AddObserver(this);

  host()->render_frame_metadata_provider()->AddObserver(this);
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
  UpdateNativeViewTree(/*parent_native_view=*/nullptr,
                       /*parent_layer=*/nullptr);
  view_.set_event_handler(nullptr);
  DCHECK(!ime_adapter_android_);
  DCHECK(!delegated_frame_host_);
  if (obj_) {
    Java_RenderWidgetHostViewImpl_clearNativePtr(
        base::android::AttachCurrentThread(), obj_);
    obj_.Reset();
  }
}

void RenderWidgetHostViewAndroid::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void RenderWidgetHostViewAndroid::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

base::CallbackListSubscription
RenderWidgetHostViewAndroid::SubscribeToSurfaceIdChanges(
    const SurfaceIdChangedCallback& callback) {
  return surface_id_changed_callbacks_.Add(callback);
}

void RenderWidgetHostViewAndroid::OnSurfaceIdChanged() {
  surface_id_changed_callbacks_.Notify(GetCurrentSurfaceId());

  if (selection_popup_controller_) {
    selection_popup_controller_->ChildLocalSurfaceIdChanged();
  }
}

void RenderWidgetHostViewAndroid::InitAsChild(gfx::NativeView parent_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos,
    const gfx::Rect& anchor_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  RenderFrameHostImpl* frame_host = host()->frame_tree()->GetMainFrame();
  if (GetVirtualKeyboardMode() !=
      ui::mojom::VirtualKeyboardMode::kOverlaysContent) {
    return;
  }
  gfx::Rect keyboard_rect_with_scale;
  if (!keyboard_rect.IsEmpty()) {
    // This is necessary because the receiver of this rect in the renderer
    // expects the rect to be in device-independnet pixels, but |keyboard_rect|
    // is in device pixels. See
    // LocalFrameMojoHandler::NotifyVirtualKeyboardOverlayRect.
    // To trigger this code, follow the steps in
    // .../external/wpt/virtual-keyboard/virtual-keyboard-css-env-manual.html
    float scale = 1 / view_.GetDipScale();
    keyboard_rect_with_scale = ScaleToEnclosedRect(keyboard_rect, scale);
    // Intersect the keyboard rect with the `this` bounds which will be sent
    // to the renderer.
    keyboard_rect_with_scale.Intersect(GetViewBounds());
  }
  frame_host->GetPage().NotifyVirtualKeyboardOverlayRect(
      keyboard_rect_with_scale);
}

ui::mojom::VirtualKeyboardMode
RenderWidgetHostViewAndroid::GetVirtualKeyboardMode() {
  RenderFrameHostImpl* frame_host = host()->frame_tree()->GetMainFrame();
  if (!frame_host)
    return ui::mojom::VirtualKeyboardMode::kUnset;

  return frame_host->GetPage().virtual_keyboard_mode();
}

viz::SurfaceId RenderWidgetHostViewAndroid::GetFallbackSurfaceIdForTesting()
    const {
  return delegated_frame_host_->GetFallbackSurfaceIdForTesting();  // IN-TEST
}

bool RenderWidgetHostViewAndroid::SynchronizeVisualProperties(
    const cc::DeadlinePolicy& deadline_policy,
    const std::optional<viz::LocalSurfaceId>& child_local_surface_id,
    bool reuse_current_local_surface_id,
    bool ignore_ack) {
    // Always merge the child_id, even if we cannot sync at this time.
    if (child_local_surface_id)
      local_surface_id_allocator_.UpdateFromChild(*child_local_surface_id);

    if (!CanSynchronizeVisualProperties())
      return false;

    if (!child_local_surface_id && !reuse_current_local_surface_id)
      local_surface_id_allocator_.GenerateId();

  // If we still have an invalid viz::LocalSurfaceId, then we are hidden and
  // evicted. This will have been triggered by a child acknowledging a previous
  // synchronization message via DidUpdateVisualProperties. The child has not
  // prompted any further property changes, so we do not need to continue
  // syncrhonization. Nor do we want to embed an invalid surface.
  if (!local_surface_id_allocator_.HasValidLocalSurfaceId())
    return false;

  if (delegated_frame_host_) {
    delegated_frame_host_->EmbedSurface(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        GetCompositorViewportPixelSize(), deadline_policy,
        host()->delegate()->IsFullscreen());
  }

  if (ignore_ack) {
    return host()->SynchronizeVisualPropertiesIgnoringPendingAck();
  }
  return host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Ignore the given size as only the Java code has the power to
  // resize the view on Android.
  default_bounds_ = gfx::Rect(default_bounds_.origin(), size);
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  default_bounds_ = rect;
}

bool RenderWidgetHostViewAndroid::HasValidFrame() const {
  if (!delegated_frame_host_)
    return false;

  if (!view_.parent())
    return false;

  if (current_surface_size_.IsEmpty())
    return false;

  return delegated_frame_host_->HasSavedFrame();
}

gfx::NativeView RenderWidgetHostViewAndroid::GetNativeView() {
  return &view_;
}

gfx::NativeViewAccessible
RenderWidgetHostViewAndroid::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::GotFocus() {
  host()->GotFocus();
  OnFocusInternal();
}

void RenderWidgetHostViewAndroid::LostFocus() {
  host()->LostFocus();
  LostFocusInternal();
}

void RenderWidgetHostViewAndroid::OnRenderFrameMetadataChangedBeforeActivation(
    const cc::RenderFrameMetadata& metadata) {
  bool is_transparent = metadata.has_transparent_background;

  if (!using_browser_compositor_) {
    // Android WebView ignores transparent background.
    is_transparent = false;
  }

  gesture_provider_.SetDoubleTapSupportForPageEnabled(
      !metadata.is_mobile_optimized);

  float dip_scale = view_.GetDipScale();
  gfx::SizeF root_layer_size_dip = metadata.root_layer_size;
  gfx::SizeF scrollable_viewport_size_dip = metadata.scrollable_viewport_size;
  gfx::PointF root_scroll_offset_dip =
      metadata.root_scroll_offset.value_or(gfx::PointF());
  float pix_to_dip = 1 / dip_scale;
  root_layer_size_dip.Scale(pix_to_dip);
  scrollable_viewport_size_dip.Scale(pix_to_dip);
  root_scroll_offset_dip.Scale(pix_to_dip);

  // Note that the height of browser control is not affected by page scale
  // factor. Thus, |top_content_offset| in CSS pixels is also in DIPs.
  float top_content_offset =
      metadata.top_controls_height * metadata.top_controls_shown_ratio;
  float top_shown_pix = top_content_offset;

  if (ime_adapter_android_) {
    ime_adapter_android_->UpdateFrameInfo(metadata.selection.start, dip_scale,
                                          top_shown_pix);
  }

  if (!gesture_listener_manager_)
    return;

  UpdateTouchSelectionController(metadata.selection, metadata.page_scale_factor,
                                 metadata.top_controls_height,
                                 metadata.top_controls_shown_ratio,
                                 scrollable_viewport_size_dip);

  // ViewAndroid::content_offset() must be in dip.
  float top_content_offset_dip = top_content_offset / dip_scale;
  view_.UpdateFrameInfo({scrollable_viewport_size_dip, top_content_offset_dip});
  bool controls_changed = UpdateControls(
      view_.GetDipScale(), metadata.top_controls_height,
      metadata.top_controls_shown_ratio,
      metadata.top_controls_min_height_offset, metadata.bottom_controls_height,
      metadata.bottom_controls_shown_ratio,
      metadata.bottom_controls_min_height_offset);

  // TODO(crbug.com/40219248): Remove toSkColor and make all SkColor4f.
  SetContentBackgroundColor(is_transparent
                                ? SK_ColorTRANSPARENT
                                : metadata.root_background_color.toSkColor());

  if (overscroll_controller_) {
    overscroll_controller_->OnFrameMetadataUpdated(
        metadata.page_scale_factor, metadata.device_scale_factor,
        metadata.scrollable_viewport_size, metadata.root_layer_size,
        metadata.root_scroll_offset.value_or(gfx::PointF()),
        metadata.root_overflow_y_hidden);
  }

  // All offsets and sizes except |top_shown_pix| are in dip.
  gesture_listener_manager_->UpdateScrollInfo(
      root_scroll_offset_dip, metadata.page_scale_factor,
      metadata.min_page_scale_factor, metadata.max_page_scale_factor,
      root_layer_size_dip, scrollable_viewport_size_dip, top_content_offset_dip,
      top_shown_pix, controls_changed);
  // This needs to be called after GestureListenerManager::UpdateScrollInfo, as
  // it depends on frame info being updated during the UpdateScrollInfo call.
  auto* wcax = GetWebContentsAccessibilityAndroid();
  if (wcax)
    wcax->UpdateFrameInfo(metadata.page_scale_factor);

  page_scale_ = metadata.page_scale_factor;
  min_page_scale_ = metadata.min_page_scale_factor;
  max_page_scale_ = metadata.max_page_scale_factor;
  current_surface_size_ = metadata.viewport_size_in_pixels;

  // With SurfaceSync we no longer call evict frame on every metadata change. We
  // must still call UpdateWebViewBackgroundColorIfNecessary to maintain the
  // associated background color changes.
  UpdateWebViewBackgroundColorIfNecessary();

  if (metadata.new_vertical_scroll_direction !=
      viz::VerticalScrollDirection::kNull) {
    bool can_scroll = metadata.root_layer_size.height() -
                          metadata.viewport_size_in_pixels.height() >
                      std::numeric_limits<float>::epsilon();
    float scroll_ratio = 0.f;
    if (can_scroll && metadata.root_scroll_offset) {
      scroll_ratio = metadata.root_scroll_offset.value().y() /
                     (metadata.root_layer_size.height() -
                      metadata.viewport_size_in_pixels.height());
    }
    view_.OnVerticalScrollDirectionChanged(
        metadata.new_vertical_scroll_direction ==
            viz::VerticalScrollDirection::kUp,
        scroll_ratio);
  }
}

base::android::ScopedJavaLocalRef<jobject>
RenderWidgetHostViewAndroid::GetJavaObject() {
  if (!obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    obj_.Reset(env, Java_RenderWidgetHostViewImpl_create(
                        env, reinterpret_cast<intptr_t>(this))
                        .obj());
  }
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

bool RenderWidgetHostViewAndroid::IsReady(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return HasValidFrame();
}

void RenderWidgetHostViewAndroid::DismissTextHandles(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DismissTextHandles();
}

jint RenderWidgetHostViewAndroid::GetBackgroundColor(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::optional<SkColor> color =
      RenderWidgetHostViewAndroid::GetCachedBackgroundColor();
  if (!color)
    return SK_ColorTRANSPARENT;
  return *color;
}

void RenderWidgetHostViewAndroid::ShowContextMenuAtTouchHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint x,
    jint y) {
  if (GetTouchSelectionControllerClientManager()) {
    GetTouchSelectionControllerClientManager()->ShowContextMenu(
        gfx::Point(x, y));
  }
}

void RenderWidgetHostViewAndroid::OnViewportInsetBottomChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              std::nullopt);
}

void RenderWidgetHostViewAndroid::WriteContentBitmapToDiskAsync(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint width,
    jint height,
    const base::android::JavaParamRef<jstring>& jpath,
    const base::android::JavaParamRef<jobject>& jcallback) {
  base::OnceCallback<void(const SkBitmap&)> result_callback = base::BindOnce(
      &RenderWidgetHostViewAndroid::OnFinishGetContentBitmap,
      weak_ptr_factory_.GetWeakPtr(),
      base::android::ScopedJavaGlobalRef<jobject>(env, obj),
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback),
      base::android::ConvertJavaStringToUTF8(env, jpath));

  CopyFromSurface(gfx::Rect(), gfx::Size(width, height),
                  std::move(result_callback));
}

void RenderWidgetHostViewAndroid::OnResume(JNIEnv* env) {
  // crbug.com/370000831. After activity resume, input state is not refreshed
  // properly. Manually call update state.
  OnUpdateTextInputStateCalled(text_input_manager_, this, true);
}

void RenderWidgetHostViewAndroid::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
  const cc::RenderFrameMetadata& metadata =
      host()->render_frame_metadata_provider()->LastRenderFrameMetadata();

  auto activated_local_surface_id =
      metadata.local_surface_id.value_or(viz::LocalSurfaceId());

  if (activated_local_surface_id.is_valid()) {
    // We have received content, ensure that any subsequent navigation allocates
    // a new surface.
    pre_navigation_content_ = true;

    while (!rotation_metrics_.empty()) {
      auto rotation_target = rotation_metrics_.front();
      // Activation from a previous surface before the new rotation has set a
      // viz::LocalSurfaceId.
      if (!rotation_target.second.is_valid())
        break;

      // In most cases the viz::LocalSurfaceId will be the same.
      //
      // However, if there are two cases where this does not occur.
      //
      // Firstly the Renderer may increment the |child_sequence_number| if it
      // needs to also alter visual properties. If so the newer surface would
      // denote the first visual update of the rotation. So its activation time
      // is correct.
      //
      // Otherwise there may be two rotations in close proximity, and one takes
      // too long to present. When this occurs the initial rotation does not
      // display. This newer surface will be the first displayed. Use its
      // activation time for the rotation, as the user would have been blocked
      // on visual updates for that long.
      //
      // We want to know of these long tail rotation times.
      if (activated_local_surface_id.IsSameOrNewerThan(
              rotation_target.second)) {
        // The duration for a rotation encompasses two separate spans of time,
        // depending on whether or not we were `is_showing_` at the start of
        // rotation.
        //
        // For a visible rotation `rotation_target.first` denotes the start of
        // the rotation event handled in BeginRotationBatching.
        //
        // For a hidden rotation we ignore this initial event, as the Renderer
        // can continue to be hidden for a long time. In these cases the
        // `rotation_target.first` denotes when ShowInternal is called.
        //
        // From these, until `activation_time`, we can determine the length of
        // time that the Renderer is visible, until the post rotation surface is
        // first displayed.
        auto duration = activation_time - rotation_target.first;
        TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
            "viz", "RenderWidgetHostViewAndroid::RotationEmbed",
            TRACE_ID_LOCAL(rotation_target.second.hash()), activation_time,
            "duration(ms)", duration.InMillisecondsF());
        rotation_metrics_.pop_front();
      } else {
        // The embedded surface may have updated the
        // LocalSurfaceId::child_sequence_number while we were updating the
        // parent_sequence_number for `rotation_target`. For example starting
        // from (6, 2) the child advances to (6, 3), and the parent advances to
        // (7, 2). viz::LocalSurfaceId::IsNewerThan will return false in these
        // mixed sequence advancements.
        //
        // Subsequently we would merge the two into (7, 3) which will become the
        // actually submitted surface to Viz.
        //
        // As such we have now received a surface that is not for our target, so
        // we break here and await the next frame from the child.
        break;
      }
    }
    if (rotation_metrics_.empty())
      in_rotation_ = false;
  }
  if (ime_adapter_android_) {
    // We need to first wait for Blink's viewport size to change such that we
    // can correctly scroll to the currently focused input.
    // On Clank, only visible viewport size changes and device viewport size or
    // viewport_size_in_pixels do not change according to the window/view size
    /// change. Only scrollable viewport size changes both for Chrome and
    // WebView.
    ime_adapter_android_->OnRenderFrameMetadataChangedAfterActivation(
        metadata.scrollable_viewport_size);
  }
}

void RenderWidgetHostViewAndroid::OnRootScrollOffsetChanged(
    const gfx::PointF& root_scroll_offset) {
  if (!gesture_listener_manager_)
    return;
  gfx::PointF root_scroll_offset_dip = root_scroll_offset;
  root_scroll_offset_dip.Scale(1 / view_.GetDipScale());
  gesture_listener_manager_->OnRootScrollOffsetChanged(root_scroll_offset_dip);
}

void RenderWidgetHostViewAndroid::Focus() {
  if (view_.HasFocus())
    GotFocus();
  else
    view_.RequestFocus();
}

void RenderWidgetHostViewAndroid::OnFocusInternal() {
  if (overscroll_controller_)
    overscroll_controller_->Enable();
}

void RenderWidgetHostViewAndroid::LostFocusInternal() {
  if (overscroll_controller_)
    overscroll_controller_->Disable();
}

bool RenderWidgetHostViewAndroid::HasFocus() {
  return view_.HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() {
  return !using_browser_compositor_ ||
         (delegated_frame_host_ &&
          delegated_frame_host_->CanCopyFromCompositingSurface());
}

void RenderWidgetHostViewAndroid::ShowWithVisibility(
    PageVisibilityState page_visibility) {
  // We can transition from `PageVisibilityState::kHiddenButPainting` to
  // `PageVisibilityState::kVisible` while `is_showing_`. We only want to
  // support updating visibility requests for this transition.
  if (page_visibility_ == page_visibility) {
    return;
  }

  page_visibility_ = page_visibility;
  is_showing_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_showing_)
    return;

  page_visibility_ = PageVisibilityState::kHidden;
  is_showing_ = false;
  HideInternal();
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  // |view_.parent()| being NULL means that it is not attached
  // to the View system yet, so we treat this RWHVA as hidden.
  return is_showing_ && view_.parent();
}

void RenderWidgetHostViewAndroid::SelectAroundCaretAck(
    int startOffset,
    int endOffset,
    int surroundingTextLength,
    blink::mojom::SelectAroundCaretResultPtr result) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->OnSelectAroundCaretAck(
      startOffset, endOffset, surroundingTextLength, std::move(result));
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() {
  if (!view_.parent())
    return default_bounds_;

  gfx::Size size(view_.GetSize());

  return gfx::Rect(size);
}

gfx::Size RenderWidgetHostViewAndroid::GetVisibleViewportSize() {
  int pinned_bottom_adjust_dps =
      std::max(0, (int)(view_.GetViewportInsetBottom() / view_.GetDipScale()));
  gfx::Rect requested_rect(GetRequestedRendererSize());
  requested_rect.Inset(gfx::Insets::TLBR(0, 0, pinned_bottom_adjust_dps, 0));
  return requested_rect.size();
}

void RenderWidgetHostViewAndroid::SetInsets(const gfx::Insets& insets) {
  NOTREACHED_IN_MIGRATION();
}

gfx::Size RenderWidgetHostViewAndroid::GetCompositorViewportPixelSize() {
  if (!view_.parent()) {
    if (default_bounds_.IsEmpty()) return gfx::Size();

    float scale_factor = view_.GetDipScale();
    return gfx::Size(default_bounds_.right() * scale_factor,
                     default_bounds_.bottom() * scale_factor);
  }

  return view_.GetPhysicalBackingSize();
}

int RenderWidgetHostViewAndroid::GetMouseWheelMinimumGranularity() const {
  auto* window = view_.GetWindowAndroid();
  if (!window)
    return 0;

  // On Android, mouse wheel MotionEvents specify the number of ticks and how
  // many pixels each tick scrolls. This multiplier is specified by device
  // metrics (See WindowAndroid.getMouseWheelScrollFactor) so the minimum
  // granularity will be the size of this tick multiplier.
  return window->mouse_wheel_scroll_factor() / view_.GetDipScale();
}

void RenderWidgetHostViewAndroid::UpdateCursor(const ui::Cursor& cursor) {
  view_.OnCursorChanged(cursor);
}

void RenderWidgetHostViewAndroid::SetIsLoading(bool is_loading) {
  // Do nothing. The UI notification is handled through ContentViewClient which
  // is TabContentsDelegate.
}

// -----------------------------------------------------------------------------
// TextInputManager::Observer implementations.
void RenderWidgetHostViewAndroid::OnUpdateTextInputStateCalled(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view,
    bool did_change_state) {
  if (!ime_adapter_android_)
    return;

  DCHECK_EQ(text_input_manager_, text_input_manager);
  if (GetTextInputManager()->GetActiveWidget()) {
    ime_adapter_android_->UpdateState(
        *GetTextInputManager()->GetTextInputState());
  } else {
    // If there are no active widgets, the TextInputState.type should be
    // reported as none.
    ime_adapter_android_->UpdateState(ui::mojom::TextInputState());
  }
}

void RenderWidgetHostViewAndroid::OnImeCompositionRangeChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view,
    bool character_bounds_changed,
    const std::optional<std::vector<gfx::Rect>>& line_bounds) {
  DCHECK_EQ(text_input_manager_, text_input_manager);
  // Don't pass data to Java if using the new pipeline.
  if (!ime_adapter_android_ ||
      base::FeatureList::IsEnabled(
          blink::features::kCursorAnchorInfoMojoPipe)) {
    return;
  }

  if (character_bounds_changed) {
    const TextInputManager::CompositionRangeInfo* info =
        text_input_manager_->GetCompositionRangeInfo();
    ime_adapter_android_->SetBounds(
        info ? info->character_bounds : std::vector<gfx::Rect>(),
        character_bounds_changed, line_bounds);
    return;
  }

  ime_adapter_android_->SetBounds(std::vector<gfx::Rect>(), false, line_bounds);
}

void RenderWidgetHostViewAndroid::OnImeCancelComposition(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(text_input_manager_, text_input_manager);
  if (ime_adapter_android_)
    ime_adapter_android_->CancelComposition();
}

void RenderWidgetHostViewAndroid::OnTextSelectionChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(text_input_manager_, text_input_manager);

  if (!selection_popup_controller_)
    return;

  RenderWidgetHostImpl* focused_widget = GetFocusedWidget();
  if (!focused_widget || !focused_widget->GetView())
    return;

  const TextInputManager::TextSelection& selection =
      *text_input_manager_->GetTextSelection(focused_widget->GetView());

  selection_popup_controller_->OnSelectionChanged(
      base::UTF16ToUTF8(selection.selected_text()));
}

viz::FrameSinkId RenderWidgetHostViewAndroid::GetRootFrameSinkId() {
  if (sync_compositor_)
    return sync_compositor_->GetFrameSinkId();
  if (view_.GetWindowAndroid() && view_.GetWindowAndroid()->GetCompositor())
    return view_.GetWindowAndroid()->GetCompositor()->GetFrameSinkId();
  return viz::FrameSinkId();
}

viz::SurfaceId RenderWidgetHostViewAndroid::GetCurrentSurfaceId() const {
  if (sync_compositor_)
    return sync_compositor_->GetSurfaceId();
  return delegated_frame_host_ ? delegated_frame_host_->SurfaceId()
                               : viz::SurfaceId();
}

bool RenderWidgetHostViewAndroid::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  auto frame_sink_id = GetFrameSinkId();
  if (!frame_sink_id.is_valid()) {
    return false;
  }

  // In TransformPointToLocalCoordSpace() there is a Point-to-Pixel conversion,
  // but it is not necessary here because the final target view is responsible
  // for converting before computing the final transform.
  return target_view->TransformPointToLocalCoordSpace(point, frame_sink_id,
                                                      transformed_point);
}

void RenderWidgetHostViewAndroid::SetGestureListenerManager(
    GestureListenerManager* manager) {
  gesture_listener_manager_ = manager;
  UpdateRootScrollOffsetUpdateFrequency();
}

void RenderWidgetHostViewAndroid::UpdateRootScrollOffsetUpdateFrequency() {
  if (!host())
    return;

  host()
      ->render_frame_metadata_provider()
      ->UpdateRootScrollOffsetUpdateFrequency(
          RootScrollOffsetUpdateFrequency());
}

base::WeakPtr<RenderWidgetHostViewAndroid>
RenderWidgetHostViewAndroid::GetWeakPtrAndroid() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool RenderWidgetHostViewAndroid::OnGestureEvent(
    const ui::GestureEventAndroid& event) {
  std::unique_ptr<blink::WebGestureEvent> web_event;
  if (event.scale() < 0.f) {
    // Negative scale indicates zoom reset.
    float delta = min_page_scale_ / page_scale_;
    web_event = ui::CreateWebGestureEventFromGestureEventAndroid(
        ui::GestureEventAndroid(event.type(), event.location(),
                                event.screen_location(), event.time(), delta, 0,
                                0, 0, 0, /*target_viewport*/ false,
                                /*synthetic_scroll*/ false,
                                /*prevent_boosting*/ false));
  } else {
    web_event = ui::CreateWebGestureEventFromGestureEventAndroid(event);
  }
  if (!web_event)
    return false;
  SendGestureEvent(*web_event);
  return true;
}

bool RenderWidgetHostViewAndroid::OnTouchEvent(
    const ui::MotionEventAndroid& event) {
  RecordToolTypeForActionDown(event);

  if (event.GetAction() == ui::MotionEventAndroid::Action::DOWN) {
    if (base::FeatureList::IsEnabled(
            features::kFocusRenderWidgetHostViewAndroidOnActionDown) &&
        !HasFocus()) {
      // On Android, |this| class should always be focused even when a
      // ChildFrame is handling touch.
      // TODO(b/340824076): Adding Focus call on ActionDown is a workaround to
      // this problem. This line should be removed after this bug is fixed.
      Focus();
    }
    if (ime_adapter_android_)
      ime_adapter_android_->UpdateOnTouchDown();
  }

  if (event.for_touch_handle())
    return OnTouchHandleEvent(event);

  if (!host() || !host()->delegate())
    return false;

  ComputeEventLatencyOSTouchHistograms(event);

  // Receiving any other touch event before the double-tap timeout expires
  // cancels opening the spellcheck menu.
  if (text_suggestion_host_)
    text_suggestion_host_->StopSuggestionMenuTimer();

  // If a browser-based widget consumes the touch event, it's critical that
  // touch event interception be disabled. This avoids issues with
  // double-handling for embedder-detected gestures like side swipe.
  if (OnTouchHandleEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  if (stylus_text_selector_.OnTouchEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(event);
  if (!result.succeeded)
    return false;

  blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
      event, result.moved_beyond_slop_region /* may_cause_scrolling */,
      false /* hovering */);
  if (web_event.GetType() == blink::WebInputEvent::Type::kUndefined)
    return false;

  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteTouchEvent(this, &web_event,
                                                               latency_info);
  } else {
    host()->GetRenderInputRouter()->ForwardTouchEventWithLatencyInfo(
        web_event, latency_info);
  }

  // Send a proactive BeginFrame for this vsync to reduce scroll latency for
  // scroll-inducing touch events. Note that Android's Choreographer ensures
  // that BeginFrame requests made during Action::MOVE dispatch will be honored
  // in the same vsync phase.
  if (observing_root_window_ && result.moved_beyond_slop_region) {
    if (sync_compositor_)
      sync_compositor_->RequestOneBeginFrame();
  }
  return true;
}

bool RenderWidgetHostViewAndroid::OnTouchHandleEvent(
    const ui::MotionEvent& event) {
  return touch_selection_controller_ &&
         touch_selection_controller_->WillHandleTouchEvent(event);
}

int RenderWidgetHostViewAndroid::GetTouchHandleHeight() {
  if (!touch_selection_controller_)
    return 0;
  return static_cast<int>(touch_selection_controller_->GetTouchHandleHeight());
}

void RenderWidgetHostViewAndroid::ResetGestureDetection() {
  const ui::MotionEvent* current_down_event =
      gesture_provider_.GetCurrentDownEvent();
  if (!current_down_event) {
    // A hard reset ensures prevention of any timer-based events that might fire
    // after a touch sequence has ended.
    gesture_provider_.ResetDetection();
    return;
  }

  std::unique_ptr<ui::MotionEvent> cancel_event = current_down_event->Cancel();
  if (gesture_provider_.OnTouchEvent(*cancel_event).succeeded) {
    bool causes_scrolling = false;
    ui::LatencyInfo latency_info;
    latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
    blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
        *cancel_event, causes_scrolling /* may_cause_scrolling */,
        false /* hovering */);
    if (ShouldRouteEvents()) {
      host()->delegate()->GetInputEventRouter()->RouteTouchEvent(
          this, &web_event, latency_info);
    } else {
      host()->GetRenderInputRouter()->ForwardTouchEventWithLatencyInfo(
          web_event, latency_info);
    }
  }
}

void RenderWidgetHostViewAndroid::OnOldViewDidNavigatePreCommit() {
  if (delegated_frame_host_) {
    delegated_frame_host_->DidNavigateMainFramePreCommit();
  }
}

void RenderWidgetHostViewAndroid::OnNewViewDidNavigatePostCommit() {
  // Move to front only if we are the primary page (we don't want to receive
  // events in the Prerender). GetMainRenderFrameHost() may be null in tests.
  if (view_.parent() &&
      RenderViewHostImpl::From(host())->GetMainRenderFrameHost() &&
      RenderViewHostImpl::From(host())
              ->GetMainRenderFrameHost()
              ->GetLifecycleState() ==
          RenderFrameHost::LifecycleState::kActive) {
    view_.parent()->MoveToFront(&view_);
  }
  ResetGestureDetection();
}

void RenderWidgetHostViewAndroid::DidEnterBackForwardCache() {
  local_surface_id_allocator_.GenerateId();
  delegated_frame_host_->DidEnterBackForwardCache();
  // If we have the fallback content timer running, force it to stop. Else, when
  // the page is restored the timer could also fire, setting whatever
  // `DelegatedFrameHostAndroid::first_local_surface_id_after_navigation_`
  // as the fallback to our Surfacelayer.
  //
  // This is safe for BFCache restore because we will supply specific fallback
  // surfaces for BFCache.
  //
  // We do not want to call this in `RWHImpl::WasHidden()` because in the case
  // of `Visibility::OCCLUDED` we still want to keep the timer running.
  //
  // Called after to prevent prematurely evict the BFCached surface.
  host()->ForceFirstFrameAfterNavigationTimeout();
}

void RenderWidgetHostViewAndroid::SetDoubleTapSupportEnabled(bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(enabled);
}

void RenderWidgetHostViewAndroid::SetMultiTouchZoomSupportEnabled(
    bool enabled) {
  gesture_provider_.SetMultiTouchZoomSupportEnabled(enabled);
}

void RenderWidgetHostViewAndroid::FocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_screen) {
  if (ime_adapter_android_)
    ime_adapter_android_->FocusedNodeChanged(is_editable_node,
                                             node_bounds_in_screen);
}

bool RenderWidgetHostViewAndroid::ShouldInitiateStylusWriting() {
  return ime_adapter_android_ &&
         ime_adapter_android_->ShouldInitiateStylusWriting();
}

void RenderWidgetHostViewAndroid::NotifyHoverActionStylusWritable(
    bool stylus_writable) {
  view_.NotifyHoverActionStylusWritable(stylus_writable);
}

void RenderWidgetHostViewAndroid::OnStartStylusWriting() {
  if (host()) {
    host()->UpdateElementFocusForStylusWriting();
  }
}

void RenderWidgetHostViewAndroid::OnEditElementFocusedForStylusWriting(
    const gfx::Rect& focused_edit_bounds,
    const gfx::Rect& caret_bounds) {
  if (ime_adapter_android_) {
    ime_adapter_android_->OnEditElementFocusedForStylusWriting(
        focused_edit_bounds, caret_bounds);
  }
}

void RenderWidgetHostViewAndroid::OnEditElementFocusClearedForStylusWriting() {
  // ImeAdapterAndroid expects empty bounds when focus could not be set.
  OnEditElementFocusedForStylusWriting(gfx::Rect(), gfx::Rect());
}

void RenderWidgetHostViewAndroid::RenderProcessGone() {
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  host()->render_frame_metadata_provider()->RemoveObserver(this);
  host()->ViewDestroyed();
  UpdateNativeViewTree(/*parent_native_view=*/nullptr,
                       /*parent_layer=*/nullptr);
  delegated_frame_host_.reset();
  delegated_frame_host_client_.reset();

  if (GetTextInputManager() && GetTextInputManager()->HasObserver(this))
    GetTextInputManager()->RemoveObserver(this);

  for (auto& observer : destruction_observers_)
    observer.RenderWidgetHostViewDestroyed(this);
  destruction_observers_.Clear();
  // Call this before the derived class is destroyed so that virtual function
  // calls back into `this` still work.
  NotifyObserversAboutShutdown();
  RenderWidgetHostViewBase::Destroy();
  delete this;
}

void RenderWidgetHostViewAndroid::UpdateTooltipUnderCursor(
    const std::u16string& tooltip_text) {
  // Tooltips don't make sense on Android.
}

void RenderWidgetHostViewAndroid::UpdateTooltipFromKeyboard(
    const std::u16string& tooltip_text,
    const gfx::Rect& bounds) {
  // Tooltips don't make sense on Android.
}

void RenderWidgetHostViewAndroid::ClearKeyboardTriggeredTooltip() {
  // Tooltips don't make sense on Android.
}

void RenderWidgetHostViewAndroid::UpdateFrameSinkIdRegistration() {
  RenderWidgetHostViewBase::UpdateFrameSinkIdRegistration();

  delegated_frame_host_->SetIsFrameSinkIdOwner(is_frame_sink_id_owner());
}

void RenderWidgetHostViewAndroid::UpdateBackgroundColor() {
  DCHECK(RenderWidgetHostViewBase::GetBackgroundColor());

  SkColor color = *RenderWidgetHostViewBase::GetBackgroundColor();
  view_.OnBackgroundColorChanged(color);
}

bool RenderWidgetHostViewAndroid::HasFallbackSurface() const {
  return delegated_frame_host_ && delegated_frame_host_->HasFallbackSurface();
}

void RenderWidgetHostViewAndroid::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  TRACE_EVENT0("cc", "RenderWidgetHostViewAndroid::CopyFromSurface");
  if (!IsSurfaceAvailableForCopy()) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  if (!using_browser_compositor_) {
    SynchronousCopyContents(src_subrect, output_size, std::move(callback));
    return;
  }

  DCHECK(delegated_frame_host_);
  delegated_frame_host_->CopyFromCompositingSurface(
      src_subrect, output_size,
      base::BindOnce(
          [](base::OnceCallback<void(const SkBitmap&)> callback,
             const SkBitmap& bitmap) {
            TRACE_EVENT0(
                "cc", "RenderWidgetHostViewAndroid::CopyFromSurface finished");
            std::move(callback).Run(bitmap);
          },
          std::move(callback)),
      /*capture_exact_surface_id=*/false);
}

void RenderWidgetHostViewAndroid::CopyFromExactSurface(
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  CHECK(IsSurfaceAvailableForCopy())
      << "To copy the exact surface, it must be available for copy (embedded "
         "via the browser).";
  CHECK(using_browser_compositor_);
  CHECK(delegated_frame_host_);

  delegated_frame_host_->CopyFromCompositingSurface(
      src_rect, output_size,
      base::BindOnce(
          [](base::OnceCallback<void(const SkBitmap&)> callback,
             const SkBitmap& bitmap) { std::move(callback).Run(bitmap); },
          std::move(callback)),
      /*capture_exact_surface_id=*/true);
}

void RenderWidgetHostViewAndroid::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseInfiniteDeadline(),
                              std::nullopt);
}

uint32_t RenderWidgetHostViewAndroid::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

bool RenderWidgetHostViewAndroid::CanSynchronizeVisualProperties() {
  // When a rotation begins, the new visual properties are not all notified to
  // RenderWidgetHostViewAndroid at the same time. The process begins when
  // OnSynchronizedDisplayPropertiesChanged is called, and ends with
  // OnPhysicalBackingSizeChanged.
  //
  // During this time there can be upwards of three calls to
  // SynchronizeVisualProperties. Sending each of these separately to the
  // Renderer causes three full re-layouts of the page to occur.
  //
  // We should instead wait for the full set of new visual properties to be
  // available, and deliver them to the Renderer in one single update.
  if (in_rotation_) {
    return false;
  }

  return screen_state_change_handler_.CanSynchronizeVisualProperties();
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAndroid::CreateSyntheticGestureTarget() {
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetAndroid(host(), &view_));
}

bool RenderWidgetHostViewAndroid::ShouldRouteEvents() const {
  DCHECK(host());
  return host()->delegate() && host()->delegate()->GetInputEventRouter();
}

void RenderWidgetHostViewAndroid::UpdateWebViewBackgroundColorIfNecessary() {
  // Android WebView had a bug the BG color was always set to black when
  // fullscreen (see https://crbug.com/961223#c5). As applications came to rely
  // on this behavior, preserve it here.
  if (!using_browser_compositor_ && host()->delegate()->IsFullscreen()) {
    SetContentBackgroundColor(SK_ColorBLACK);
  }
}

void RenderWidgetHostViewAndroid::ClearFallbackSurfaceForCommitPending() {
  delegated_frame_host_->ClearFallbackSurfaceForCommitPending();
  EvictInternal();
}

void RenderWidgetHostViewAndroid::ResetFallbackToFirstNavigationSurface() {
  if (delegated_frame_host_)
    delegated_frame_host_->ResetFallbackToFirstNavigationSurface();
}

bool RenderWidgetHostViewAndroid::RequestRepaintForTesting() {
  return SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                     std::nullopt);
}

void RenderWidgetHostViewAndroid::SetSynchronousCompositorClient(
      SynchronousCompositorClient* client) {
  synchronous_compositor_client_ = client;
  MaybeCreateSynchronousCompositor();
}

void RenderWidgetHostViewAndroid::MaybeCreateSynchronousCompositor() {
  if (!sync_compositor_ && synchronous_compositor_client_) {
    sync_compositor_ = SynchronousCompositorHost::Create(
        this, host()->GetFrameSinkId(), GetHostFrameSinkManager());
    view_.SetCopyOutputCallback(sync_compositor_->GetCopyViewCallback());
    if (renderer_widget_created_)
      sync_compositor_->InitMojo();
  }
}

void RenderWidgetHostViewAndroid::ResetSynchronousCompositor() {
  if (sync_compositor_) {
    view_.SetCopyOutputCallback(ui::ViewAndroid::CopyViewCallback());
    sync_compositor_.reset();
  }
}

void RenderWidgetHostViewAndroid::OnOverscrollRefreshHandlerAvailable() {
  DCHECK(!overscroll_controller_);
  CreateOverscrollControllerIfPossible();
}

bool RenderWidgetHostViewAndroid::SupportsAnimation() const {
  // The synchronous (WebView) compositor does not have a proper browser
  // compositor with which to drive animations.
  return using_browser_compositor_;
}

void RenderWidgetHostViewAndroid::SetNeedsAnimate() {
  DCHECK(view_.GetWindowAndroid());
  DCHECK(using_browser_compositor_);
  view_.GetWindowAndroid()->SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::PointF& position) {
  MoveCaret(gfx::Point(position.x(), position.y()));
}

void RenderWidgetHostViewAndroid::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->MoveRangeSelectionExtent(extent);
}

void RenderWidgetHostViewAndroid::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->SelectBetweenCoordinates(base, extent);
}

void RenderWidgetHostViewAndroid::OnSelectionEvent(
    ui::SelectionEventType event) {
  if (!selection_popup_controller_)
    return;
  DCHECK(touch_selection_controller_);
  // If a selection drag has started, it has taken over the active touch
  // sequence. Immediately cancel gesture detection and any downstream touch
  // listeners (e.g., web content) to communicate this transfer.
  if (event == ui::SELECTION_HANDLES_SHOWN &&
      gesture_provider_.GetCurrentDownEvent()) {
    ResetGestureDetection();
  }
  selection_popup_controller_->OnSelectionEvent(
      event, GetSelectionRect(*touch_selection_controller_));
}

void RenderWidgetHostViewAndroid::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->OnDragUpdate(type, position);
}

ui::TouchSelectionControllerClient*
RenderWidgetHostViewAndroid::GetSelectionControllerClientManagerForTesting() {
  return touch_selection_controller_client_manager_.get();
}

void RenderWidgetHostViewAndroid::SetSelectionControllerClientForTesting(
    std::unique_ptr<ui::TouchSelectionControllerClient> client) {
  touch_selection_controller_client_for_test_.swap(client);

  touch_selection_controller_ = CreateSelectionController(
      touch_selection_controller_client_for_test_.get(), !!view_.parent());
}

std::unique_ptr<ui::TouchHandleDrawable>
RenderWidgetHostViewAndroid::CreateDrawable() {
  if (!using_browser_compositor_) {
    if (!sync_compositor_)
      return nullptr;
    return std::unique_ptr<ui::TouchHandleDrawable>(
        sync_compositor_->client()->CreateDrawable());
  }
  if (!selection_popup_controller_)
    return nullptr;
  return selection_popup_controller_->CreateTouchHandleDrawable(
      view_.parent(), view_.GetLayer()->parent());
}

void RenderWidgetHostViewAndroid::DidScroll() {}

void RenderWidgetHostViewAndroid::ShowTouchSelectionContextMenu(
    const gfx::Point& location) {
  host()->ShowContextMenuAtPoint(location, ui::MENU_SOURCE_TOUCH_HANDLE);
}

void RenderWidgetHostViewAndroid::SynchronousCopyContents(
    const gfx::Rect& src_subrect_dip,
    const gfx::Size& dst_size_in_pixel,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  // Note: When |src_subrect| is empty, a conversion from the view size must
  // be made instead of using |current_frame_size_|. The latter sometimes also
  // includes extra height for the toolbar UI, which is not intended for
  // capture.
  gfx::Rect valid_src_subrect_in_dips = src_subrect_dip;
  if (valid_src_subrect_in_dips.IsEmpty())
    valid_src_subrect_in_dips = gfx::Rect(GetVisibleViewportSize());
  const gfx::Rect src_subrect_in_pixel = gfx::ToEnclosingRect(
      gfx::ConvertRectToPixels(valid_src_subrect_in_dips, view_.GetDipScale()));

  // TODO(crbug.com/41305903): [BUG] Current implementation does not support
  // read-back of regions that do not originate at (0,0).
  const gfx::Size& input_size_in_pixel = src_subrect_in_pixel.size();
  DCHECK(!input_size_in_pixel.IsEmpty());

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = input_size_in_pixel;
  else
    output_size_in_pixel = dst_size_in_pixel;
  int output_width = output_size_in_pixel.width();
  int output_height = output_size_in_pixel.height();

  if (!sync_compositor_) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(output_width, output_height));
  SkCanvas canvas(bitmap);
  canvas.scale(
      (float)output_width / (float)input_size_in_pixel.width(),
      (float)output_height / (float)input_size_in_pixel.height());
  sync_compositor_->DemandDrawSw(&canvas, /*software_canvas=*/true);
  std::move(callback).Run(bitmap);
}

WebContentsAccessibilityAndroid*
RenderWidgetHostViewAndroid::GetWebContentsAccessibilityAndroid() const {
  return web_contents_accessibility_;
}

void RenderWidgetHostViewAndroid::UpdateTouchSelectionController(
    const viz::Selection<gfx::SelectionBound>& selection,
    float page_scale_factor,
    float top_controls_height,
    float top_controls_shown_ratio,
    const gfx::SizeF& scrollable_viewport_size_dip) {
  if (!touch_selection_controller_)
    return;

  DCHECK(touch_selection_controller_client_manager_);
  touch_selection_controller_client_manager_->UpdateClientSelectionBounds(
      selection.start, selection.end, this, nullptr);
  OnUpdateScopedSelectionHandles();

  // Set parameters for adaptive handle orientation.
  gfx::SizeF viewport_size(scrollable_viewport_size_dip);
  viewport_size.Scale(page_scale_factor);
  gfx::RectF viewport_rect(0.0f, top_controls_height * top_controls_shown_ratio,
                           viewport_size.width(), viewport_size.height());
  touch_selection_controller_->OnViewportChanged(viewport_rect);
}

bool RenderWidgetHostViewAndroid::UpdateControls(
    float dip_scale,
    float top_controls_height,
    float top_controls_shown_ratio,
    float top_controls_min_height_offset,
    float bottom_controls_height,
    float bottom_controls_shown_ratio,
    float bottom_controls_min_height_offset) {
  float top_controls_pix = top_controls_height;
  // |top_content_offset| is in physical pixels if --use-zoom-for-dsf is
  // enabled. Otherwise, it is in DIPs.
  // Note that the height of browser control is not affected by page scale
  // factor. Thus, |top_content_offset| in CSS pixels is also in DIPs.
  float top_content_offset = top_controls_height * top_controls_shown_ratio;
  float top_shown_pix = top_content_offset;
  float top_translate = top_shown_pix - top_controls_pix;
  bool top_changed =
      !cc::MathUtil::IsFloatNearlyTheSame(top_shown_pix, prev_top_shown_pix_);

  float top_min_height_offset_pix = top_controls_min_height_offset;
  top_changed |= !cc::MathUtil::IsFloatNearlyTheSame(
      top_min_height_offset_pix, prev_top_controls_min_height_offset_pix_);

  top_changed |= !cc::MathUtil::IsFloatNearlyTheSame(top_controls_pix,
                                                     prev_top_controls_pix_);

  prev_top_shown_pix_ = top_shown_pix;
  prev_top_controls_pix_ = top_controls_pix;
  prev_top_controls_translate_ = top_translate;
  prev_top_controls_min_height_offset_pix_ = top_min_height_offset_pix;

  float bottom_controls_pix = bottom_controls_height;
  float bottom_shown_pix = bottom_controls_pix * bottom_controls_shown_ratio;
  bool bottom_changed = !cc::MathUtil::IsFloatNearlyTheSame(
      bottom_shown_pix, prev_bottom_shown_pix_);
  float bottom_translate = bottom_controls_pix - bottom_shown_pix;

  float bottom_min_height_offset_pix = bottom_controls_min_height_offset;
  bottom_changed |= !cc::MathUtil::IsFloatNearlyTheSame(
      bottom_min_height_offset_pix,
      prev_bottom_controls_min_height_offset_pix_);

  if (top_changed || bottom_changed || !controls_initialized_) {
    view_.OnControlsChanged(top_translate, top_shown_pix,
                            top_min_height_offset_pix, bottom_translate,
                            bottom_min_height_offset_pix);
  }
  prev_bottom_shown_pix_ = bottom_shown_pix;
  prev_bottom_controls_translate_ = bottom_translate;
  prev_bottom_controls_min_height_offset_pix_ = bottom_min_height_offset_pix;
  controls_initialized_ = true;
  return top_changed || bottom_changed;
}

void RenderWidgetHostViewAndroid::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
    // Eviction and rotation handling has been updated, and is no longer tied to
    // child update. No more need to unthrottle here.
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                metadata.local_surface_id);

  if (using_browser_compositor_) {
    ui::WindowAndroid* window = view_.GetWindowAndroid();
    if (!window) {
      return;
    }
    ui::WindowAndroidCompositor* compositor = window->GetCompositor();
    if (!compositor) {
      return;
    }
    static_cast<CompositorImpl*>(compositor)->MaybeCompositeNow();
  }
}

void RenderWidgetHostViewAndroid::OnFinishGetContentBitmap(
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& callback,
    const std::string& path,
    const SkBitmap& bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!bitmap.drawsNothing()) {
    auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    task_runner->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&CompressAndSaveBitmap, path, bitmap),
        base::BindOnce(
            &base::android::RunStringCallbackAndroid,
            base::android::ScopedJavaGlobalRef<jobject>(env, callback.obj())));
    return;
  }
  // If readback failed, call empty callback
  base::android::RunStringCallbackAndroid(callback, std::string());
}

void RenderWidgetHostViewAndroid::ShowInternal() {
  bool show = is_showing_ && is_window_activity_started_ && is_window_visible_;
  if (!show)
    return;

  OnShowWithPageVisibility(page_visibility_);
}

void RenderWidgetHostViewAndroid::HideInternal() {
  DCHECK(!is_showing_ || !is_window_activity_started_ || !is_window_visible_)
      << "Hide called when the widget should be shown.";

  // As we stop visual observations, we clear the current fullscreen state. Once
  // ShowInternal() is invoked the most up to date visual properties will be
  // used.
  fullscreen_rotation_ = false;

  // If a RWHVA gets hidden and swapped out then gets swapped back in and shown,
  // the last known controls offsets may be the same as the latest values we get
  // from the renderer. In this case, we would skip pushing the offset to
  // `ViewAndroid` assuming there was no change. To prevent this, we should
  // reset `controls_initialized_` to make sure the offsets are pushed once the
  // RWHVA is shown again.
  controls_initialized_ = false;

  // Only preserve the frontbuffer if the activity was stopped while the
  // window is still visible. This avoids visual artifacts when transitioning
  // between activities.
  bool hide_frontbuffer = is_window_activity_started_ || !is_window_visible_;

  // Only stop observing the root window if the widget has been explicitly
  // hidden and the frontbuffer is being cleared. This allows window visibility
  // notifications to eventually clear the frontbuffer.
  bool stop_observing_root_window = !is_showing_ && hide_frontbuffer;

  if (hide_frontbuffer) {
    view_.GetLayer()->SetHideLayerAndSubtree(true);
    if (delegated_frame_host_)
      delegated_frame_host_->WasHidden();
  }

  if (stop_observing_root_window) {
    DCHECK(!is_showing_);
    StopObservingRootWindow();
  }

  if (!host() || host()->is_hidden())
    return;

  if (overscroll_controller_)
    overscroll_controller_->Disable();

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host()->WasHidden();
}

void RenderWidgetHostViewAndroid::StartObservingRootWindow() {
  DCHECK(view_.parent());
  DCHECK(view_.GetWindowAndroid());
  DCHECK(is_showing_);
  if (observing_root_window_)
    return;

  observing_root_window_ = true;
  view_.GetWindowAndroid()->AddObserver(this);

  ui::WindowAndroidCompositor* compositor =
      view_.GetWindowAndroid()->GetCompositor();
  if (compositor) {
    delegated_frame_host_->AttachToCompositor(compositor);
  }

  OnUpdateScopedSelectionHandles();
  ObserveDevicePosturePlatformProvider();
}

void RenderWidgetHostViewAndroid::StopObservingRootWindow() {
  if (!(view_.GetWindowAndroid())) {
    DCHECK(!observing_root_window_);
    return;
  }

  if (!observing_root_window_)
    return;

  // Reset window state variables to their defaults.
  is_window_activity_started_ = true;
  is_window_visible_ = true;
  observing_root_window_ = false;
  OnUpdateScopedSelectionHandles();
  view_.GetWindowAndroid()->RemoveObserver(this);
  // If the DFH has already been destroyed, it will have cleaned itself up.
  // This happens in some WebView cases.
  if (delegated_frame_host_)
    delegated_frame_host_->DetachFromCompositor();
}

bool RenderWidgetHostViewAndroid::Animate(base::TimeTicks frame_time) {
  bool needs_animate = false;
  if (overscroll_controller_) {
    needs_animate |=
        overscroll_controller_->Animate(frame_time, view_.GetLayer()->parent());
  }
  // TODO(wjmaclean): Investigate how animation here does or doesn't affect
  // an OOPIF client.
  if (touch_selection_controller_)
    needs_animate |= touch_selection_controller_->Animate(frame_time);
  return needs_animate;
}

void RenderWidgetHostViewAndroid::RequestDisallowInterceptTouchEvent() {
  if (view_.parent())
    view_.RequestDisallowInterceptTouchEvent();
}

void RenderWidgetHostViewAndroid::TransformPointToRootSurface(
    gfx::PointF* point) {
  if (!host()->delegate())
    return;
  RenderViewHostDelegateView* rvh_delegate_view =
      host()->delegate()->GetDelegateView();
  if (rvh_delegate_view->DoBrowserControlsShrinkRendererSize())
    *point += gfx::Vector2d(0, rvh_delegate_view->GetTopControlsHeight());
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

const viz::LocalSurfaceId&
RenderWidgetHostViewAndroid::IncrementSurfaceIdForNavigation() {
  local_surface_id_allocator_.GenerateId();

  if (delegated_frame_host_) {
    delegated_frame_host_->EmbedSurface(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        GetCompositorViewportPixelSize(),
        cc::DeadlinePolicy::UseDefaultDeadline(),
        host()->delegate()->IsFullscreen());
  }

  return local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const input::TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAndroid::ProcessAckedTouchEvent");
  const bool event_consumed =
      ack_result == blink::mojom::InputEventResultState::kConsumed;
  // |is_source_touch_event_set_non_blocking| defines a blocking behaviour of
  // the future inputs.
  const bool is_source_touch_event_set_non_blocking =
      InputEventResultStateIsSetBlocking(ack_result);
  // |was_touch_blocked| indicates whether the current event was dispatched
  // blocking to the Renderer.
  const bool was_touch_blocked =
      ui::WebInputEventTraits::ShouldBlockEventStream(touch.event);
  gesture_provider_.OnTouchEventAck(
      touch.event.unique_touch_event_id, event_consumed,
      is_source_touch_event_set_non_blocking,
      was_touch_blocked
          ? std::make_optional(touch.event.GetEventLatencyMetadata())
          : std::nullopt);
  if (touch.event.touch_start_or_first_touch_move && event_consumed &&
      host()->delegate() && host()->delegate()->GetInputEventRouter()) {
    host()
        ->delegate()
        ->GetInputEventRouter()
        ->OnHandledTouchStartOrFirstTouchMove(
            touch.event.unique_touch_event_id);
  }
}

void RenderWidgetHostViewAndroid::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  if (overscroll_controller_)
    overscroll_controller_->OnGestureEventAck(event, ack_result);
  mouse_wheel_phase_handler_.GestureEventAck(event, ack_result);

  ForwardTouchpadZoomEventIfNecessary(event, ack_result);

  // Stop flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  StopFlingingIfNecessary(event, ack_result);

  HandleSwipeToMoveCursorGestureAck(event);
}

blink::mojom::InputEventResultState
RenderWidgetHostViewAndroid::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  if (overscroll_controller_ &&
      blink::WebInputEvent::IsGestureEventType(input_event.GetType())) {
    blink::WebGestureEvent gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    if (overscroll_controller_->WillHandleGestureEvent(gesture_event)) {
      // Terminate an active fling when a GSU generated from the fling progress
      // (GSU with inertial state) is consumed by the overscroll_controller_ and
      // overscrolling mode is not |OVERSCROLL_NONE|. The early fling
      // termination generates a GSE which completes the overscroll action.
      if (gesture_event.GetType() ==
              blink::WebInputEvent::Type::kGestureScrollUpdate &&
          gesture_event.data.scroll_update.inertial_phase ==
              blink::WebGestureEvent::InertialPhaseState::kMomentum) {
        host_->StopFling();
      }

      return blink::mojom::InputEventResultState::kConsumed;
    }
  }

  if (gesture_listener_manager_ &&
      gesture_listener_manager_->FilterInputEvent(input_event)) {
    return blink::mojom::InputEventResultState::kConsumed;
  }

  if (!host())
    return blink::mojom::InputEventResultState::kNotConsumed;

  if (input_event.GetType() == blink::WebInputEvent::Type::kTouchStart) {
    GpuProcessHost::CallOnUI(FROM_HERE, GPU_PROCESS_KIND_SANDBOXED,
                             false /* force_create */,
                             base::BindOnce(&WakeUpGpu));
  }

  return blink::mojom::InputEventResultState::kNotConsumed;
}

blink::mojom::PointerLockResult RenderWidgetHostViewAndroid::LockPointer(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnsupportedOptions;
}

blink::mojom::PointerLockResult RenderWidgetHostViewAndroid::ChangePointerLock(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnsupportedOptions;
}

void RenderWidgetHostViewAndroid::UnlockPointer() {
  NOTIMPLEMENTED();
}

// Methods called from the host to the render

void RenderWidgetHostViewAndroid::SendKeyEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (!host())
    return;

  RenderWidgetHostImpl* target_host = host();

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (host()->delegate())
    target_host = host()->delegate()->GetFocusedRenderWidgetHost(host());
  if (!target_host)
    return;

  // Receiving a key event before the double-tap timeout expires cancels opening
  // the spellcheck menu. If the suggestion menu is open, we close the menu.
  if (text_suggestion_host_)
    text_suggestion_host_->OnKeyEvent();

  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  target_host->ForwardKeyboardEventWithLatencyInfo(event, latency_info);
}

void RenderWidgetHostViewAndroid::SendMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& info) {
  if (!host() || !host()->delegate())
    return;

  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteMouseEvent(this, &event,
                                                               info);
  } else {
    host()->ForwardMouseEventWithLatencyInfo(event, info);
  }
}

void RenderWidgetHostViewAndroid::UpdateMouseState(int action_button,
                                                   float mousedown_x,
                                                   float mousedown_y) {
  if (action_button != ui::MotionEventAndroid::BUTTON_PRIMARY) {
    // Reset state if middle or right button was pressed.
    left_click_count_ = 0;
    prev_mousedown_timestamp_ = base::TimeTicks();
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  const base::TimeDelta time_delay = current_time - prev_mousedown_timestamp_;
  const gfx::Point mousedown_point(mousedown_x, mousedown_y);
  const float distance_squared =
      (mousedown_point - prev_mousedown_point_).LengthSquared();
  if (left_click_count_ > 2 || time_delay > kClickCountInterval ||
      distance_squared > kClickCountRadiusSquaredDIP) {
    left_click_count_ = 0;
  }
  left_click_count_++;
  prev_mousedown_timestamp_ = current_time;
  prev_mousedown_point_ = mousedown_point;
}

void RenderWidgetHostViewAndroid::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  if (!host() || !host()->delegate())
    return;

  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  blink::WebMouseWheelEvent wheel_event(event);
  bool should_route_events = ShouldRouteEvents();
  mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
      wheel_event, should_route_events);

  if (should_route_events) {
    host()->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
        this, &wheel_event, latency_info);
  } else {
    host()->ForwardWheelEventWithLatencyInfo(wheel_event, latency_info);
  }
}

void RenderWidgetHostViewAndroid::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  // Sending a gesture that may trigger overscroll should resume the effect.
  if (overscroll_controller_)
    overscroll_controller_->Enable();

  if (!host() || !host()->delegate() ||
      event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    return;
  }

  // We let the touch selection controller see gesture events here, since they
  // may be routed and not make it to FilterInputEvent().
  if (touch_selection_controller_ &&
      event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
    switch (event.GetType()) {
      case blink::WebInputEvent::Type::kGestureLongPress:
        touch_selection_controller_->HandleLongPressEvent(
            event.TimeStamp(), event.PositionInWidget());
        break;

      case blink::WebInputEvent::Type::kGestureTapDown:
        if (event.data.tap_down.tap_down_count == 2) {
          touch_selection_controller_->HandleDoublePressEvent(
              event.TimeStamp(), event.PositionInWidget());
        }
        break;

      case blink::WebInputEvent::Type::kGestureTap:
        touch_selection_controller_->HandleTapEvent(event.PositionInWidget(),
                                                    event.data.tap.tap_count);
        break;

      case blink::WebInputEvent::Type::kGestureScrollBegin:
        touch_selection_controller_->OnScrollBeginEvent();
        break;

      default:
        break;
    }
  }

  ui::LatencyInfo latency_info;
  if (event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
      // If there is a current scroll going on and a new scroll that isn't
      // wheel based, send a synthetic wheel event with kPhaseEnded to cancel
      // the current scroll.
      mouse_wheel_phase_handler_.DispatchPendingWheelEndEvent();
    } else if (event.GetType() ==
               blink::WebInputEvent::Type::kGestureScrollEnd) {
      // Make sure that the next wheel event will have phase = |kPhaseBegan|.
      // This is for maintaining the correct phase info when some of the wheel
      // events get ignored while a touchscreen scroll is going on.
      mouse_wheel_phase_handler_.IgnorePendingWheelEndEvent();
    }

  } else if (event.GetType() ==
                 blink::WebInputEvent::Type::kGestureFlingStart &&
             event.SourceDevice() == blink::WebGestureDevice::kTouchpad) {
    // Ignore the pending wheel end event to avoid sending a wheel event with
    // kPhaseEnded before a GFS.
    mouse_wheel_phase_handler_.IgnorePendingWheelEndEvent();
  }
  if (ShouldRouteEvents()) {
    blink::WebGestureEvent gesture_event(event);
    host()->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, &gesture_event, latency_info);
  } else {
    host()->GetRenderInputRouter()->ForwardGestureEventWithLatencyInfo(
        event, latency_info);
  }
}

bool RenderWidgetHostViewAndroid::ShowSelectionMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {
  if (!selection_popup_controller_) {
    return false;
  }

  return selection_popup_controller_->ShowSelectionMenu(
      render_frame_host, params, GetTouchHandleHeight());
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::Point& point) {
  if (host() && host()->delegate())
    host()->delegate()->MoveCaret(point);
}

void RenderWidgetHostViewAndroid::DismissTextHandles() {
  if (touch_selection_controller_)
    touch_selection_controller_->HideAndDisallowShowingAutomatically();
}

void RenderWidgetHostViewAndroid::SetTextHandlesTemporarilyHidden(
    bool hide_handles) {
  if (!touch_selection_controller_ ||
      handles_hidden_by_selection_ui_ == hide_handles)
    return;
  handles_hidden_by_selection_ui_ = hide_handles;
  SetTextHandlesHiddenInternal();
}

std::optional<SkColor> RenderWidgetHostViewAndroid::GetCachedBackgroundColor() {
  return RenderWidgetHostViewBase::GetBackgroundColor();
}

void RenderWidgetHostViewAndroid::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  if (sync_compositor_)
    sync_compositor_->DidOverscroll(params);

  if (!view_.parent() || !is_showing_)
    return;

  if (overscroll_controller_)
    overscroll_controller_->OnOverscrolled(params);
}

const viz::FrameSinkId& RenderWidgetHostViewAndroid::GetFrameSinkId() const {
  if (!delegated_frame_host_)
    return viz::FrameSinkIdAllocator::InvalidFrameSinkId();

  return delegated_frame_host_->GetFrameSinkId();
}

void RenderWidgetHostViewAndroid::UpdateNativeViewTree(
    gfx::NativeView parent_native_view,
    cc::slim::Layer* parent_layer) {
  // `parent_native_view` and `parent_layer` must be null or non-null at the
  // same time.
  CHECK(!(!!parent_native_view ^ !!parent_layer));

  bool will_build_tree = parent_native_view != nullptr;
  bool has_view_tree = view_.parent() != nullptr;

  // Allows same parent view to be set again.
  DCHECK(!will_build_tree || !has_view_tree ||
         parent_native_view == view_.parent());

  StopObservingRootWindow();

  bool resize = false;
  if (will_build_tree != has_view_tree) {
    touch_selection_controller_.reset();
    if (has_view_tree) {
      view_.RemoveObserver(this);
      view_.RemoveFromParent();
      view_.GetLayer()->RemoveFromParent();
    }
    if (will_build_tree) {
      view_.AddObserver(this);
      parent_native_view->AddChild(&view_);
      parent_layer->AddChild(view_.GetLayer());
    }

    // TODO(yusufo) : Get rid of the below conditions and have a better handling
    // for resizing after crbug.com/628302 is handled.
    bool is_size_initialized = !will_build_tree ||
                               view_.GetSize().width() != 0 ||
                               view_.GetSize().height() != 0;
    if (has_view_tree || is_size_initialized)
      resize = true;
    has_view_tree = will_build_tree;
  }

  if (!has_view_tree) {
    ResetSynchronousCompositor();
    return;
  }
  // Parent native view can become null and then later non-null again, if
  // WebContents swaps away from this, and then later back to it. Need to
  // ensure SynchronousCompositor is recreated in this case.
  MaybeCreateSynchronousCompositor();

  // Force an initial update of screen infos so the default RWHVBase value
  // is not used.
  // TODO(enne): figure out a more straightforward init path for screen infos.
  UpdateScreenInfo();

  if (is_showing_ && view_.GetWindowAndroid())
    StartObservingRootWindow();

  if (resize) {
    SynchronizeVisualProperties(
        cc::DeadlinePolicy::UseSpecifiedDeadline(
            ui::DelegatedFrameHostAndroid::ResizeTimeoutFrames()),
        std::nullopt);
  }

  if (!touch_selection_controller_) {
    ui::TouchSelectionControllerClient* client =
        touch_selection_controller_client_manager_.get();
    if (touch_selection_controller_client_for_test_)
      client = touch_selection_controller_client_for_test_.get();

    touch_selection_controller_ = CreateSelectionController(client, true);
  }

  CreateOverscrollControllerIfPossible();
}

cc::mojom::RootScrollOffsetUpdateFrequency
RenderWidgetHostViewAndroid::RootScrollOffsetUpdateFrequency() {
  // In order to provide support for onScrollOffsetOrExtentChanged()
  // GestureListenerManager needs root-scroll-offsets. The frequency of the
  // updates depends on the needs of the `GestureStateListenerWithScroll`s, if
  // any.
  if (web_contents_accessibility_ != nullptr) {
    return cc::mojom::RootScrollOffsetUpdateFrequency::kAllUpdates;
  }
  return gesture_listener_manager_
             ? gesture_listener_manager_->root_scroll_offset_update_frequency()
             : cc::mojom::RootScrollOffsetUpdateFrequency::kNone;
}

MouseWheelPhaseHandler*
RenderWidgetHostViewAndroid::GetMouseWheelPhaseHandler() {
  return &mouse_wheel_phase_handler_;
}

TouchSelectionControllerClientManager*
RenderWidgetHostViewAndroid::GetTouchSelectionControllerClientManager() {
  return touch_selection_controller_client_manager_.get();
}

const viz::LocalSurfaceId& RenderWidgetHostViewAndroid::GetLocalSurfaceId()
    const {
  return local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

void RenderWidgetHostViewAndroid::OnRendererWidgetCreated() {
  renderer_widget_created_ = true;
  if (sync_compositor_)
    sync_compositor_->InitMojo();
}

bool RenderWidgetHostViewAndroid::OnMouseEvent(
    const ui::MotionEventAndroid& event) {
  RecordToolTypeForActionDown(event);

  blink::WebInputEvent::Type webMouseEventType =
      ui::ToWebMouseEventType(event.GetAction());

  if (webMouseEventType == blink::WebInputEvent::Type::kUndefined) {
    return false;
  }

  int action_button = event.GetActionButton();
  if (webMouseEventType == blink::WebInputEvent::Type::kMouseDown) {
    UpdateMouseState(action_button, event.GetX(0), event.GetY(0));
  }

  int click_count = 0;

  if (webMouseEventType == blink::WebInputEvent::Type::kMouseDown ||
      webMouseEventType == blink::WebInputEvent::Type::kMouseUp) {
    click_count = (action_button == ui::MotionEventAndroid::BUTTON_PRIMARY)
                      ? left_click_count_
                      : 1;
  }

  SendMouseEvent(input::WebMouseEventBuilder::Build(event, webMouseEventType,
                                                    click_count, action_button),
                 ui::LatencyInfo());

  return true;
}

bool RenderWidgetHostViewAndroid::OnMouseWheelEvent(
    const ui::MotionEventAndroid& event) {
  SendMouseWheelEvent(input::WebMouseWheelEventBuilder::Build(event));
  return true;
}

void RenderWidgetHostViewAndroid::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  if ((gesture.type() == ui::EventType::kGesturePinchBegin ||
       gesture.type() == ui::EventType::kGesturePinchUpdate ||
       gesture.type() == ui::EventType::kGesturePinchEnd) &&
      !IsPinchToZoomEnabled()) {
    return;
  }

  blink::WebGestureEvent web_gesture =
      ui::CreateWebGestureEventFromGestureEventData(gesture);
  // TODO(jdduke): Remove this workaround after Android fixes UiAutomator to
  // stop providing shift meta values to synthetic MotionEvents. This prevents
  // unintended shift+click interpretation of all accessibility clicks.
  // See crbug.com/443247.
  if (web_gesture.GetType() == blink::WebInputEvent::Type::kGestureTap &&
      web_gesture.GetModifiers() == blink::WebInputEvent::kShiftKey) {
    web_gesture.SetModifiers(blink::WebInputEvent::kNoModifiers);
  }
  SendGestureEvent(web_gesture);
}

bool RenderWidgetHostViewAndroid::RequiresDoubleTapGestureEvents() const {
  return true;
}

void RenderWidgetHostViewAndroid::OnSizeChanged() {
  screen_state_change_handler_.OnVisibleViewportSizeChanged(view_.GetSize());
  // The display feature depends on the view size so we need to recompute it.
  ComputeDisplayFeature();
}

void RenderWidgetHostViewAndroid::OnPhysicalBackingSizeChanged(
    std::optional<base::TimeDelta> deadline_override) {
  // We may need to update the background color to match pre-surface-sync
  // behavior of EvictFrameIfNecessary.
  UpdateWebViewBackgroundColorIfNecessary();
  int64_t deadline_in_frames =
      deadline_override ? ui::DelegatedFrameHostAndroid::TimeDeltaToFrames(
                              deadline_override.value())
                        : ui::DelegatedFrameHostAndroid::ResizeTimeoutFrames();

    if (screen_state_change_handler_.OnPhysicalBackingSizeChanged(
            view_.GetPhysicalBackingSize(), deadline_in_frames)) {
      return;
    }

    SynchronizeVisualProperties(
        cc::DeadlinePolicy::UseSpecifiedDeadline(deadline_in_frames),
        std::nullopt);
}

void RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged(bool visible) {
  TRACE_EVENT1("browser",
               "RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged",
               "visible", visible);
  DCHECK(observing_root_window_);

  // Don't early out if visibility hasn't changed and visible. This is necessary
  // as OnDetachedFromWindow() sets |is_window_visible_| to true, so that this
  // may be called when ShowInternal() needs to be called.
  if (is_window_visible_ == visible && !visible)
    return;

  is_window_visible_ = visible;

  if (visible)
    ShowInternal();
  else
    HideInternal();
}

void RenderWidgetHostViewAndroid::OnAttachedToWindow() {
  if (!view_.parent())
    return;

  UpdateScreenInfo();
  if (is_showing_)
    StartObservingRootWindow();
  DCHECK(view_.GetWindowAndroid());
  if (view_.GetWindowAndroid()->GetCompositor())
    OnAttachCompositor();
}

void RenderWidgetHostViewAndroid::OnDetachedFromWindow() {
  StopObservingRootWindow();
  OnDetachCompositor();
}

void RenderWidgetHostViewAndroid::OnAttachCompositor() {
  DCHECK(view_.parent());
  CreateOverscrollControllerIfPossible();
  if (observing_root_window_ && using_browser_compositor_) {
    ui::WindowAndroidCompositor* compositor =
        view_.GetWindowAndroid()->GetCompositor();
    delegated_frame_host_->AttachToCompositor(compositor);
  }
}

void RenderWidgetHostViewAndroid::OnDetachCompositor() {
  DCHECK(view_.parent());
  overscroll_controller_.reset();
  if (using_browser_compositor_)
    delegated_frame_host_->DetachFromCompositor();
}

void RenderWidgetHostViewAndroid::OnAnimate(base::TimeTicks begin_frame_time) {
  if (Animate(begin_frame_time))
    SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::OnUnfoldStarted(
    base::TimeTicks unfold_begin_time) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnUnfoldStarted");
  host()->RequestSuccessfulPresentationTimeForNextFrame(
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          unfold_begin_time, /*destination_is_loaded=*/false,
          /*show_reason_tab_switching=*/false,
          /*show_reason_bfcache_restore=*/false,
          /*show_reason_unfolding=*/true));
}

void RenderWidgetHostViewAndroid::OnActivityStopped() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStopped");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = false;
  HideInternal();
}

void RenderWidgetHostViewAndroid::OnActivityStarted() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStarted");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::SetTextHandlesHiddenForDropdownMenu(
    bool hide_handles) {
  if (!touch_selection_controller_ ||
      handles_hidden_by_dropdown_menu_ == hide_handles) {
    return;
  }
  handles_hidden_by_dropdown_menu_ = hide_handles;
  SetTextHandlesHiddenInternal();
}

void RenderWidgetHostViewAndroid::SetTextHandlesHiddenForStylus(
    bool hide_handles) {
  if (!touch_selection_controller_ || handles_hidden_by_stylus_ == hide_handles)
    return;
  handles_hidden_by_stylus_ = hide_handles;
  SetTextHandlesHiddenInternal();
}

void RenderWidgetHostViewAndroid::SetTextHandlesHiddenInternal() {
  if (!touch_selection_controller_)
    return;
  touch_selection_controller_->SetTemporarilyHidden(
      handles_hidden_by_dropdown_menu_ || handles_hidden_by_stylus_ ||
      handles_hidden_by_selection_ui_);
}

void RenderWidgetHostViewAndroid::OnStylusSelectBegin(float x0,
                                                      float y0,
                                                      float x1,
                                                      float y1) {
  SetTextHandlesHiddenForStylus(true);
  // TODO(ajith.v) Refactor the event names as this is not really handle drag,
  // but currently we use same for long press drag selection as well.
  OnSelectionEvent(ui::SELECTION_HANDLE_DRAG_STARTED);
  SelectBetweenCoordinates(gfx::PointF(x0, y0), gfx::PointF(x1, y1));
}

void RenderWidgetHostViewAndroid::OnStylusSelectUpdate(float x, float y) {
  MoveRangeSelectionExtent(gfx::PointF(x, y));
}

void RenderWidgetHostViewAndroid::OnStylusSelectEnd(float x, float y) {
  SetTextHandlesHiddenForStylus(false);
  // TODO(ajith.v) Refactor the event names as this is not really handle drag,
  // but currently we use same for long press drag selection as well.
  OnSelectionEvent(ui::SELECTION_HANDLE_DRAG_STOPPED);
}

void RenderWidgetHostViewAndroid::OnStylusSelectTap(base::TimeTicks time,
                                                    float x,
                                                    float y) {
  // Treat the stylus tap as a long press, activating either a word selection or
  // context menu depending on the targetted content.
  blink::WebGestureEvent long_press = input::WebGestureEventBuilder::Build(
      blink::WebInputEvent::Type::kGestureLongPress, time, x, y);
  SendGestureEvent(long_press);
}

void RenderWidgetHostViewAndroid::ComputeEventLatencyOSTouchHistograms(
      const ui::MotionEvent& event) {
  base::TimeTicks event_time = event.GetEventTime();
  base::TimeTicks current_time = base::TimeTicks::Now();
  ui::EventType event_type;
  switch (event.GetAction()) {
    case ui::MotionEvent::Action::DOWN:
    case ui::MotionEvent::Action::POINTER_DOWN:
      event_type = ui::EventType::kTouchPressed;
      break;
    case ui::MotionEvent::Action::MOVE:
      event_type = ui::EventType::kTouchMoved;
      break;
    case ui::MotionEvent::Action::UP:
    case ui::MotionEvent::Action::POINTER_UP:
      event_type = ui::EventType::kTouchReleased;
      break;
    default:
      return;
  }
  ui::ComputeEventLatencyOS(event_type, event_time, current_time);
}

void RenderWidgetHostViewAndroid::CreateOverscrollControllerIfPossible() {
  // an OverscrollController is already set
  if (overscroll_controller_)
    return;

  RenderWidgetHostDelegate* delegate = host()->delegate();
  if (!delegate)
    return;

  RenderViewHostDelegateView* delegate_view = delegate->GetDelegateView();
  // render_widget_host_unittest.cc uses an object called
  // MockRenderWidgetHostDelegate that does not have a DelegateView
  if (!delegate_view)
    return;

  ui::OverscrollRefreshHandler* overscroll_refresh_handler =
      delegate_view->GetOverscrollRefreshHandler();
  if (!overscroll_refresh_handler)
    return;

  if (!view_.parent())
    return;

  // If window_android is null here, this is bad because we don't listen for it
  // being set, so we won't be able to construct the OverscrollController at the
  // proper time.
  ui::WindowAndroid* window_android = view_.GetWindowAndroid();
  if (!window_android)
    return;

  ui::WindowAndroidCompositor* compositor = window_android->GetCompositor();
  if (!compositor)
    return;

  overscroll_controller_ = std::make_unique<OverscrollControllerAndroid>(
      overscroll_refresh_handler, compositor, view_.GetDipScale());
}

void RenderWidgetHostViewAndroid::SetOverscrollControllerForTesting(
    ui::OverscrollRefreshHandler* overscroll_refresh_handler) {
  overscroll_controller_ = std::make_unique<OverscrollControllerAndroid>(
      overscroll_refresh_handler, view_.GetWindowAndroid()->GetCompositor(),
      view_.GetDipScale());
}

void RenderWidgetHostViewAndroid::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  DCHECK(!static_cast<RenderWidgetHostViewBase*>(view)
              ->IsRenderWidgetHostViewChildFrame());
  CopyBackgroundColorIfPresentFrom(*view);

  RenderWidgetHostViewAndroid* view_android =
      static_cast<RenderWidgetHostViewAndroid*>(view);
  if (!delegated_frame_host_ || !view_android->delegated_frame_host_)
    return;
  delegated_frame_host_->TakeFallbackContentFrom(
      view_android->delegated_frame_host_.get());
}

void RenderWidgetHostViewAndroid::OnSynchronizedDisplayPropertiesChanged(
    bool rotation) {
    if (screen_state_change_handler_.OnScreenInfoChanged(GetScreenInfo()))
      return;
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                std::nullopt);
}

std::optional<SkColor> RenderWidgetHostViewAndroid::GetBackgroundColor() {
  return default_background_color_;
}

void RenderWidgetHostViewAndroid::DidNavigate() {
  if (!delegated_frame_host_) {
    RenderWidgetHostViewBase::DidNavigate();
    return;
  }
  if (!is_showing_) {
    // Navigating while hidden should not allocate a new LocalSurfaceID. Once
    // sizes are ready, or we begin to Show, we can then allocate the new
    // LocalSurfaceId.
    EvictInternal();
    navigation_while_hidden_ = true;
  } else {
    // TODO(jonross): This was a legacy optimization to not perform too many
    // Surface Synchronization iterations for the first navigation. However we
    // currently are performing 5 full synchornizations before navigation
    // completes anyways. So we need to re-do RWHVA setup.
    // (https://crbug.com/1245652)
    //
    // In the interim we will not allocate a new Surface as long as the Renderer
    // has yet to produce any content. If we have existing content always
    // allocate a new surface, as the content will be from a pre-navigation
    // source.
    if (!pre_navigation_content_) {
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  std::nullopt,
                                  /*reuse_current_local_surface_id=*/true);
    } else {
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  std::nullopt);
    }
    // Only notify of navigation once a surface has been embedded.
    delegated_frame_host_->DidNavigate();
  }
  pre_navigation_content_ = true;
}

WebContentsAccessibility*
RenderWidgetHostViewAndroid::GetWebContentsAccessibility() {
  return web_contents_accessibility_;
}

viz::ScopedSurfaceIdAllocator
RenderWidgetHostViewAndroid::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      &RenderWidgetHostViewAndroid::OnDidUpdateVisualPropertiesComplete,
      weak_ptr_factory_.GetWeakPtr(), metadata);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

display::ScreenInfo RenderWidgetHostViewAndroid::GetScreenInfo() const {
  bool use_window_wide_color_gamut =
      GetContentClient()->browser()->GetWideColorGamutHeuristic() ==
      ContentBrowserClient::WideColorGamutHeuristic::kUseWindow;
  auto* window = view_.GetWindowAndroid();
  if (!window || !use_window_wide_color_gamut) {
    return RenderWidgetHostViewBase::GetScreenInfo();
  }
  display::ScreenInfo screen_info;
  display::DisplayUtil::DisplayToScreenInfo(
      &screen_info, window->GetDisplayWithWindowColorSpace());
  return screen_info;
}

void RenderWidgetHostViewAndroid::ObserveDevicePosturePlatformProvider() {
  if (device_posture_observation_.IsObserving()) {
    return;
  }

  DevicePosturePlatformProvider* platform_provider =
      GetDevicePosturePlatformProvider();
  if (!platform_provider) {
    return;
  }

  device_posture_observation_.Observe(platform_provider);
  OnDisplayFeatureBoundsChanged(platform_provider->GetDisplayFeatureBounds());
}

void RenderWidgetHostViewAndroid::OnDisplayFeatureBoundsChanged(
    const gfx::Rect& display_feature_bounds) {
  if (display_feature_overridden_for_testing_) {
    return;
  }

  display_feature_ = std::nullopt;
  display_feature_bounds_ = gfx::Rect();
  // On some devices like the Galaxy Fold the display feature has a size of
  // 0 (width or height depending on the orientation). IsEmpty() will fail here.
  if (display_feature_bounds.size().IsZero()) {
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                std::nullopt);
    return;
  }
  display_feature_bounds_ = display_feature_bounds;
  ComputeDisplayFeature();
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              std::nullopt);
}

void RenderWidgetHostViewAndroid::SetDisplayFeatureBoundsForTesting(
    const gfx::Rect& bounds) {
  display_feature_bounds_ = bounds;
  ComputeDisplayFeature();
}

void RenderWidgetHostViewAndroid::ComputeDisplayFeature() {
  if (display_feature_overridden_for_testing_) {
    return;
  }

  display_feature_ = std::nullopt;
  gfx::Size view_size(view_.GetSize());
  // On some devices like the Galaxy Fold the display feature has a size of
  // 0 (width or height depending on the orientation). IsEmpty() will fail here.
  if (display_feature_bounds_.size().IsZero() || view_size.IsEmpty()) {
    return;
  }

  // On Android, the display feature is exposed as a rectangle as a generic
  // concept. Here in the content layer, we translate that to a more
  // constrained concept, see content::DisplayFeature.
  // The display feature and view location are both provided in device pixels,
  // relative to the window. Convert this to DIP and view relative coordinates,
  // first by applying the scale, converting the display feature to view
  // relative coordinates, then intersect with the view bounds rect.
  // the convert to view-relative coordinates.
  float dip_scale = 1 / view_.GetDipScale();
  gfx::Point view_location = view_.GetLocationOfContainerViewInWindow();
  view_location = gfx::ScaleToRoundedPoint(view_location, dip_scale);
  gfx::Rect transformed_display_feature =
      gfx::ScaleToRoundedRect(display_feature_bounds_, dip_scale);

  transformed_display_feature.Offset(-view_location.x(), -view_location.y());
  transformed_display_feature.InclusiveIntersect(gfx::Rect(view_size));

  if (transformed_display_feature.x() == 0 &&
      transformed_display_feature.width() == view_size.width()) {
    // A horizontal display feature covers the view's width and starts at
    // an x-offset of 0.
    display_feature_ = {DisplayFeature::Orientation::kHorizontal,
                        transformed_display_feature.y(),
                        transformed_display_feature.height()};
  } else if (transformed_display_feature.y() == 0 &&
             transformed_display_feature.height() == view_size.height()) {
    // A vertical display feature covers the view's height and starts at
    // a y-offset of 0.
    display_feature_ = {DisplayFeature::Orientation::kVertical,
                        transformed_display_feature.x(),
                        transformed_display_feature.width()};
  }
}

std::optional<DisplayFeature> RenderWidgetHostViewAndroid::GetDisplayFeature() {
  return display_feature_;
}

void RenderWidgetHostViewAndroid::SetDisplayFeatureForTesting(
    const DisplayFeature* display_feature) {
  if (display_feature) {
    display_feature_ = *display_feature;
  } else {
    display_feature_ = std::nullopt;
  }
  display_feature_overridden_for_testing_ = true;
}

void RenderWidgetHostViewAndroid::NotifyHostAndDelegateOnWasShown(
    blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request) {
  // Whether evicted or not, we stop batching for rotation in order to get
  // content ready for the new orientation.
  bool rotation_override = in_rotation_;
  in_rotation_ = false;

  view_.GetLayer()->SetHideLayerAndSubtree(false);

  if (overscroll_controller_)
    overscroll_controller_->Enable();

  bool was_evicted = false;
  if ((delegated_frame_host_ &&
       delegated_frame_host_->IsPrimarySurfaceEvicted()) ||
      !local_surface_id_allocator_.HasValidLocalSurfaceId()) {
    was_evicted = true;
    ui::WindowAndroidCompositor* compositor =
        view_.GetWindowAndroid() ? view_.GetWindowAndroid()->GetCompositor()
                                 : nullptr;
    SynchronizeVisualProperties(
        compositor && compositor->IsDrawingFirstVisibleFrame()
            ? cc::DeadlinePolicy::UseSpecifiedDeadline(
                  ui::DelegatedFrameHostAndroid::FirstFrameTimeoutFrames())
            : cc::DeadlinePolicy::UseDefaultDeadline(),
        std::nullopt);
    // If we navigated while hidden, we need to update the fallback surface only
    // after we've completed navigation, and embedded the new surface. The
    // |delegated_frame_host_| is always valid when |navigation_while_hidden_|
    // is set to true.
    if (navigation_while_hidden_) {
      navigation_while_hidden_ = false;
      delegated_frame_host_->DidNavigate();
    }
  } else if (rotation_override) {
    // If a rotation occurred while this was not visible, we need to allocate a
    // new viz::LocalSurfaceId and send the current visual properties to the
    // Renderer. Otherwise there will be no content at all to display.
    //
    // The rotation process will complete after this first surface is displayed.
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                std::nullopt);
  }

  // Whenever the page is restored, via back-forward cache, or tab changes,
  // record content to visible time.
  bool show_reason_bfcache_restore =
      visible_time_request ? visible_time_request->show_reason_bfcache_restore
                           : false;
  bool has_saved_frame = delegated_frame_host_->HasSavedFrame();
  if (show_reason_bfcache_restore) {
    host()->WasShown(visible_time_request.Clone());
  } else {
    host()->WasShown(has_saved_frame
                         ? blink::mojom::RecordContentToVisibleTimeRequestPtr()
                         : visible_time_request.Clone());
  }

  if (delegated_frame_host_) {
    delegated_frame_host_->WasShown(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        GetCompositorViewportPixelSize(), host()->delegate()->IsFullscreen(),
        has_saved_frame ? std::move(visible_time_request)
                        : blink::mojom::RecordContentToVisibleTimeRequestPtr());
  }

  if (view_.parent() && view_.GetWindowAndroid()) {
    StartObservingRootWindow();
    if (sync_compositor_)
      sync_compositor_->RequestOneBeginFrame();
  }

  if (rotation_override) {
    // It's possible that several rotations were all enqueued while this view
    // has hidden. We skip those and update to just the final state.
    size_t skipped_rotations = rotation_metrics_.size() - 1;
    if (skipped_rotations) {
      rotation_metrics_.erase(rotation_metrics_.begin(),
                              rotation_metrics_.begin() + skipped_rotations);
    }
    // If a rotation occurred while we were hidden, we do not want to include
    // all of that idle time in the rotation metrics. However we do want to have
    // the "RotationBegin" tracing event. So end the tracing event, before
    // setting the starting time of the rotation.
    EndRotationBatching();
    rotation_metrics_.begin()->first = base::TimeTicks::Now();
    BeginRotationEmbed();
  } else if (!rotation_metrics_.empty()) {
    // If we have enqueued `rotation_metrics` but are not completing a rotation,
    // then a timeout fired while we were hidden. As no synchronizing has
    // previously occurred, set now to be the start of the rotation time.
    rotation_metrics_.begin()->first = base::TimeTicks::Now();
  }

  // TODO(crbug.com/40879074): Ideally we would do no synchronizing at all when
  // hidden. We should just amass all the new blink::VisualProperties and send
  // them once when becoming visible. However the refactor would be difficult
  // right now. We will revisit this once we are satisfied with the rollout of
  // content::kSurfaceSyncFullscreenKillswitch.
  if (was_evicted)
    screen_state_change_handler_.WasShownAfterEviction();
}

void RenderWidgetHostViewAndroid::
    RequestSuccessfulPresentationTimeFromHostOrDelegate(
        blink::mojom::RecordContentToVisibleTimeRequestPtr
            visible_time_request) {
  bool has_saved_frame = delegated_frame_host_->HasSavedFrame();
  // No need to check for saved frames for the case of bfcache restore.
  if (visible_time_request->show_reason_bfcache_restore || !has_saved_frame) {
    host()->RequestSuccessfulPresentationTimeForNextFrame(
        visible_time_request.Clone());
  }

  // If the frame for the renderer is already available, then the
  // tab-switching time is the presentation time for the browser-compositor.
  if (has_saved_frame) {
    delegated_frame_host_->RequestSuccessfulPresentationTimeForNextFrame(
        std::move(visible_time_request));
  }
}

void RenderWidgetHostViewAndroid::
    CancelSuccessfulPresentationTimeRequestForHostAndDelegate() {
  host()->CancelSuccessfulPresentationTimeRequest();
  delegated_frame_host_->CancelSuccessfulPresentationTimeRequest();
}

void RenderWidgetHostViewAndroid::EnterFullscreenMode(
    const blink::mojom::FullscreenOptions& options) {
  screen_state_change_handler_.EnterFullscreenMode();
}

void RenderWidgetHostViewAndroid::ExitFullscreenMode() {
  screen_state_change_handler_.ExitFullscreenMode();
}

void RenderWidgetHostViewAndroid::LockOrientation(
    device::mojom::ScreenOrientationLockType orientation) {
  screen_state_change_handler_.LockOrientation(orientation);
}

void RenderWidgetHostViewAndroid::UnlockOrientation() {
  screen_state_change_handler_.UnlockOrientation();
}

void RenderWidgetHostViewAndroid::SetHasPersistentVideo(
    bool has_persistent_video) {
  screen_state_change_handler_.SetHasPersistentVideo(has_persistent_video);
}

void RenderWidgetHostViewAndroid::InvalidateLocalSurfaceIdAndAllocationGroup() {
  local_surface_id_allocator_.Invalidate(
      /*also_invalidate_allocation_group=*/true);
}

void RenderWidgetHostViewAndroid::HandleSwipeToMoveCursorGestureAck(
    const blink::WebGestureEvent& event) {
  if (!touch_selection_controller_ || !selection_popup_controller_) {
    swipe_to_move_cursor_activated_ = false;
    return;
  }

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin: {
      if (!event.data.scroll_begin.cursor_control)
        break;
      swipe_to_move_cursor_activated_ = true;
      touch_selection_controller_->OnSwipeToMoveCursorBegin();
      OnSelectionEvent(ui::INSERTION_HANDLE_DRAG_STARTED);
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollUpdate: {
      if (!swipe_to_move_cursor_activated_)
        break;
      gfx::RectF rect = touch_selection_controller_->GetRectBetweenBounds();
      // Suppress this when the input is not focused, in which case rect will be
      // 0x0.
      if (rect.width() != 0.f || rect.height() != 0.f) {
        selection_popup_controller_->OnDragUpdate(
            ui::TouchSelectionDraggable::Type::kNone,
            gfx::PointF(event.PositionInWidget().x(), rect.right_center().y()));
      }
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollEnd: {
      if (!swipe_to_move_cursor_activated_)
        break;
      swipe_to_move_cursor_activated_ = false;
      touch_selection_controller_->OnSwipeToMoveCursorEnd();
      OnSelectionEvent(ui::INSERTION_HANDLE_DRAG_STOPPED);
      break;
    }
    default:
      break;
  }
}

void RenderWidgetHostViewAndroid::WasEvicted() {
  // Eviction can occur when the CompositorFrameSink has changed. This can
  // occur either from a lost connection, as well as from the initial conneciton
  // upon creating RenderWidgetHostViewAndroid. When this occurs while visible
  // a new LocalSurfaceId should be generated. If eviction occurs while not
  // visible, then the new LocalSurfaceId can be allocated upon the next Show.
  if (is_showing_) {
    local_surface_id_allocator_.GenerateId();
    // Guarantee that the new LocalSurfaceId is propagated. Rather than relying
    // upon calls to Show() and OnDidUpdateVisualPropertiesComplete(). As there
    // is no guarantee that they will occur after the eviction.
    SynchronizeVisualProperties(
        cc::DeadlinePolicy::UseExistingDeadline(),
        local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  } else {
    EvictInternal();
  }
  if (sync_compositor_) {
    sync_compositor_->WasEvicted();
  }
}

void RenderWidgetHostViewAndroid::OnUpdateScopedSelectionHandles() {
  if (!observing_root_window_ ||
      !touch_selection_controller_client_manager_->has_active_selection()) {
    scoped_selection_handles_.reset();
    return;
  }

  if (!scoped_selection_handles_) {
    scoped_selection_handles_ =
        std::make_unique<ui::WindowAndroid::ScopedSelectionHandles>(
            view_.GetWindowAndroid());
  }
}

void RenderWidgetHostViewAndroid::SetWebContentsAccessibility(
    WebContentsAccessibilityAndroid* web_contents_accessibility) {
  web_contents_accessibility_ = web_contents_accessibility;
  UpdateRootScrollOffsetUpdateFrequency();
}

void RenderWidgetHostViewAndroid::SetNeedsBeginFrameForFlingProgress() {
  if (sync_compositor_)
    sync_compositor_->RequestOneBeginFrame();
}

const cc::slim::SurfaceLayer* RenderWidgetHostViewAndroid::GetSurfaceLayer()
    const {
  if (!delegated_frame_host_) {
    return nullptr;
  }
  return delegated_frame_host_->content_layer();
}

void RenderWidgetHostViewAndroid::RegisterOffsetTags(
    const cc::BrowserControlsOffsetTagsInfo& tags_info) {
  if (delegated_frame_host_) {
    delegated_frame_host_->RegisterOffsetTags(tags_info);
  }
}

void RenderWidgetHostViewAndroid::UnregisterOffsetTags(
    const cc::BrowserControlsOffsetTagsInfo& tags_info) {
  if (delegated_frame_host_) {
    delegated_frame_host_->UnregisterOffsetTags(tags_info);
  }
}

void RenderWidgetHostViewAndroid::PassImeRenderWidgetHost(
    mojo::PendingRemote<blink::mojom::ImeRenderWidgetHost> pending_remote) {
  host()->PassImeRenderWidgetHost(std::move(pending_remote));
}

void RenderWidgetHostViewAndroid::BeginRotationBatching() {
  in_rotation_ = true;
  rotation_metrics_.emplace_back(
      std::make_pair(base::TimeTicks::Now(), viz::LocalSurfaceId()));
  // When a rotation begins, a series of calls update different aspects of
  // visual properties. Completing in EndRotationBatching, where the full new
  // set of properties is known. Trace the duration of that.
  const auto delta = rotation_metrics_.back().first - base::TimeTicks();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "viz", "RenderWidgetHostViewAndroid::RotationBegin",
      TRACE_ID_LOCAL(delta.InNanoseconds()), "visible", is_showing_);

  if (rotation_timeout_.IsRunning())
    rotation_timeout_.Stop();
  rotation_timeout_.Start(
      FROM_HERE, kThrottleTimeout,
      base::BindOnce(
          &RenderWidgetHostViewAndroid::EndRotationAndSyncIfNecessary,
          base::Unretained(this)));
}

void RenderWidgetHostViewAndroid::EndRotationBatching() {
  in_rotation_ = false;
  // Always clear when ending batching. As WebView can trigger multiple
  // OnPhysicalBackingSizeChanged which would re-trigger rotation if we were
  // still tracking `fullscreen_rotation_`. crbug.com/1302964
  fullscreen_rotation_ = false;
  DCHECK(!rotation_metrics_.empty());
  const auto delta = rotation_metrics_.back().first - base::TimeTicks();
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "viz", "RenderWidgetHostViewAndroid::RotationBegin",
      TRACE_ID_LOCAL(delta.InNanoseconds()), "local_surface_id",
      local_surface_id_allocator_.GetCurrentLocalSurfaceId().ToString());

  if (rotation_timeout_.IsRunning())
    rotation_timeout_.Stop();
}

void RenderWidgetHostViewAndroid::BeginRotationEmbed() {
  DCHECK(!rotation_metrics_.empty());
  rotation_metrics_.back().second =
      local_surface_id_allocator_.GetCurrentLocalSurfaceId();

  // The full set of visual properties for a rotation is now known. This
  // tracks the time it takes until the Renderer successfully submits a frame
  // embedding the new viz::LocalSurfaceId. Tracking how long until a user
  // sees the complete rotation and layout of the page. This completes in
  // OnRenderFrameMetadataChangedAfterActivation.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "viz", "RenderWidgetHostViewAndroid::RotationEmbed",
      TRACE_ID_LOCAL(
          local_surface_id_allocator_.GetCurrentLocalSurfaceId().hash()),
      base::TimeTicks::Now(), "LocalSurfaceId",
      local_surface_id_allocator_.GetCurrentLocalSurfaceId().ToString());
}

void RenderWidgetHostViewAndroid::EndRotationAndSyncIfNecessary() {
  if (!in_rotation_)
    return;
  EndRotationBatching();

  if (is_showing_) {
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                std::nullopt,
                                /*reuse_current_local_surface_id=*/false,
                                /*ignore_ack=*/true);
  } else {
    // If hidden, generate a new viz::LocalSurfaceId to represent the new set of
    // blink::VisualProperties. However do not synchronize them to perform
    // layout. The subsequent Show will lead to embedding (crbug.com/1383446)
    local_surface_id_allocator_.GenerateId();
  }
  BeginRotationEmbed();
}

void RenderWidgetHostViewAndroid::EvictInternal() {
  screen_state_change_handler_.WasEvicted();
  local_surface_id_allocator_.Invalidate();
}

}  // namespace content
