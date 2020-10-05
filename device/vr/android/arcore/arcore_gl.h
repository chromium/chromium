// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_GL_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_GL_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/util/fps_meter.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gl {
class GLContext;
class GLSurface;
}  // namespace gl

namespace vr {
class ArCoreSessionUtils;
class WebXrPresentationState;
}  // namespace vr

namespace device {

class ArCore;
class ArCoreFactory;
class ArImageTransport;

using ArCoreGlCreateSessionCallback = base::OnceCallback<void(
    mojo::PendingRemote<mojom::XRFrameDataProvider> frame_data_provider,
    mojom::VRDisplayInfoPtr display_info,
    mojo::PendingRemote<mojom::XRSessionController> session_controller,
    mojom::XRPresentationConnectionPtr presentation_connection)>;

// All of this class's methods must be called on the same valid GL thread with
// the exception of GetGlThreadTaskRunner() and GetWeakPtr().
class ArCoreGl : public mojom::XRFrameDataProvider,
                 public mojom::XRPresentationProvider,
                 public mojom::XREnvironmentIntegrationProvider,
                 public mojom::XRSessionController {
 public:
  explicit ArCoreGl(std::unique_ptr<ArImageTransport> ar_image_transport);
  ~ArCoreGl() override;

  void Initialize(
      vr::ArCoreSessionUtils* session_utils,
      ArCoreFactory* arcore_factory,
      gfx::AcceleratedWidget drawing_widget,
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation,
      const std::vector<device::mojom::XRSessionFeature>& enabled_features,
      base::OnceCallback<void(bool)> callback);

  void CreateSession(mojom::VRDisplayInfoPtr display_info,
                     ArCoreGlCreateSessionCallback create_callback,
                     base::OnceClosure shutdown_callback);

  const scoped_refptr<base::SingleThreadTaskRunner>& GetGlThreadTaskRunner() {
    return gl_thread_task_runner_;
  }

  // mojom::XRFrameDataProvider
  void GetFrameData(mojom::XRFrameDataRequestOptionsPtr options,
                    GetFrameDataCallback callback) override;

  void GetEnvironmentIntegrationProvider(
      mojo::PendingAssociatedReceiver<mojom::XREnvironmentIntegrationProvider>
          environment_provider) override;
  void SetInputSourceButtonListener(
      mojo::PendingAssociatedRemote<device::mojom::XRInputSourceButtonListener>)
      override;

  // XRPresentationProvider
  void SubmitFrameMissing(int16_t frame_index, const gpu::SyncToken&) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox,
                   base::TimeDelta time_waited) override;
  void SubmitFrameWithTextureHandle(
      int16_t frame_index,
      mojo::PlatformHandle texture_handle) override;
  void SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                   const gpu::SyncToken&,
                                   base::TimeDelta time_waited) override;
  void UpdateLayerBounds(int16_t frame_index,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;

  // XREnvironmentIntegrationProvider
  void SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray,
      mojom::XREnvironmentIntegrationProvider::SubscribeToHitTestCallback
          callback) override;
  void SubscribeToHitTestForTransientInput(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray,
      mojom::XREnvironmentIntegrationProvider::
          SubscribeToHitTestForTransientInputCallback callback) override;

  void UnsubscribeFromHitTest(uint64_t subscription_id) override;

  void CreateAnchor(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const device::Pose& native_origin_from_anchor,
      CreateAnchorCallback callback) override;
  void CreatePlaneAnchor(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const device::Pose& native_origin_from_anchor,
      uint64_t plane_id,
      CreatePlaneAnchorCallback callback) override;

  void DetachAnchor(uint64_t anchor_id) override;

  // mojom::XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  void ProcessFrameFromMailbox(int16_t frame_index,
                               const gpu::MailboxHolder& mailbox);
  void ProcessFrameDrawnIntoTexture(int16_t frame_index,
                                    const gpu::SyncToken& sync_token);
  void OnWebXrTokenSignaled(int16_t frame_index,
                            std::unique_ptr<gfx::GpuFence> gpu_fence);

  // Notifies that the screen was touched at |touch_point| using a pointer.
  // |touching| will be set to true if the screen is still touched. |is_primary|
  // signifies that the used pointer is considered primary.
  void OnScreenTouch(bool is_primary,
                     bool touching,
                     int32_t pointer_id,
                     const gfx::PointF& touch_point);
  std::vector<mojom::XRInputSourceStatePtr> GetInputSourceStates();

  base::WeakPtr<ArCoreGl> GetWeakPtr();

 private:
  void Pause();
  void Resume();

  void FinishFrame(int16_t frame_index);
  bool IsSubmitFrameExpected(int16_t frame_index);
  void ProcessFrame(mojom::XRFrameDataRequestOptionsPtr options,
                    mojom::XRFrameDataPtr frame_data,
                    mojom::XRFrameDataProvider::GetFrameDataCallback callback);

  bool InitializeGl(gfx::AcceleratedWidget drawing_widget);
  void OnArImageTransportReady(base::OnceCallback<void(bool)> callback);
  bool IsOnGlThread() const;
  void CopyCameraImageToFramebuffer();
  void OnTransportFrameAvailable(const gfx::Transform& uv_transform);

  // Use a helper method to avoid storing the mojo getframedata callback
  // in a closure owned by the task runner, that would lead to inconsistent
  // state on session shutdown. See https://crbug.com/1065572.
  void RunNextGetFrameData();

  bool IsFeatureEnabled(mojom::XRSessionFeature feature);

  // Set of features enabled on this session. Required to correctly configure
  // the session and only send out necessary data related to reference spaces to
  // blink. Valid after the call to |Initialize()| method.
  std::unordered_set<device::mojom::XRSessionFeature> enabled_features_;

  base::OnceClosure session_shutdown_callback_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  // Created on GL thread and should only be accessed on that thread.
  std::unique_ptr<ArCore> arcore_;
  std::unique_ptr<ArImageTransport> ar_image_transport_;

  // This class uses the same overall presentation state logic
  // as GvrGraphicsDelegate, with some difference due to drawing
  // camera images even on frames with no pose and therefore
  // no blink-generated rendered image.
  //
  // Rough sequence is:
  //
  // SubmitFrame N                 N animating->processing
  //   draw camera N
  //   waitForToken
  // GetFrameData N+1              N+1 start animating
  //   update ARCore N to N+1
  // OnToken N                     N processing->rendering
  //   draw rendered N
  //   swap                        N rendering done
  // SubmitFrame N+1               N+1 animating->processing
  //   draw camera N+1
  //   waitForToken
  std::unique_ptr<vr::WebXrPresentationState> webxr_;

  // Default dummy values to ensure consistent behaviour.

  // Transfer size is the size of the WebGL framebuffer, this may be
  // smaller than the camera image if framebufferScaleFactor is < 1.0.
  gfx::Size transfer_size_ = gfx::Size(0, 0);

  // The camera image size stays locked to the screen size even if
  // framebufferScaleFactor changes.
  gfx::Size camera_image_size_ = gfx::Size(0, 0);
  display::Display::Rotation display_rotation_ = display::Display::ROTATE_0;
  bool should_update_display_geometry_ = true;

  // UV transform for drawing the camera texture, this is supplied by ARCore
  // and can include 90 degree rotations or other nontrivial transforms.
  gfx::Transform uv_transform_;

  // UV transform for drawing received WebGL content from a shared buffer's
  // texture, this is simply an identity.
  gfx::Transform shared_buffer_transform_;

  gfx::Transform projection_;
  gfx::Transform inverse_projection_;
  // The first run of ProduceFrame should set uv_transform_ and projection_
  // using the default settings in ArCore.
  bool should_recalculate_uvs_ = true;
  bool have_camera_image_ = false;

  bool is_initialized_ = false;
  bool is_paused_ = true;

  bool restrict_frame_data_ = false;

  base::TimeTicks arcore_update_next_expected_;
  base::TimeDelta arcore_last_frame_timestamp_;
  base::TimeDelta arcore_frame_interval_;
  FPSMeter fps_meter_;

  mojo::Receiver<mojom::XRFrameDataProvider> frame_data_receiver_{this};
  mojo::Receiver<mojom::XRSessionController> session_controller_receiver_{this};
  mojo::AssociatedReceiver<mojom::XREnvironmentIntegrationProvider>
      environment_receiver_{this};

  void OnBindingDisconnect();
  void CloseBindingsIfOpen();

  mojo::Receiver<device::mojom::XRPresentationProvider> presentation_receiver_{
      this};
  mojo::Remote<device::mojom::XRPresentationClient> submit_client_;

  // This closure saves arguments for the next GetFrameData call, including a
  // mojo callback. Must remain owned by ArCoreGl, don't pass it off to the task
  // runner directly. See RunNextGetFrameData() comments.
  base::OnceClosure pending_getframedata_;

  mojom::VRDisplayInfoPtr display_info_;
  bool display_info_changed_ = false;

  // Currently estimated floor height.
  base::Optional<float> floor_height_estimate_;

  // Touch-related data.
  // Android will report touch events via MotionEvent - see ArImmersiveOverlay
  // for details.
  struct ScreenTouchEvent {
    gfx::PointF screen_last_touch;

    // Screen touch start/end events get reported asynchronously. We want to
    // report at least one "clicked" event even if start and end happen within a
    // single frame. The "active" state corresponds to the current state and is
    // updated asynchronously. The "pending" state is set to true whenever the
    // screen is touched, but only gets cleared by the input source handler.
    //
    //    active   pending    event
    //         0         0
    //         1         1
    //         1         1    pressed=true (selectstart)
    //         1         1    pressed=true
    //         0         1->0 pressed=false clicked=true (selectend, click)
    //
    //         0         0
    //         1         1
    //         0         1
    //         0         1->0 pressed=false clicked=true (selectend, click)
    float screen_touch_pending = false;
    float screen_touch_active = false;

    // ID of the pointer that raised this event.
    int32_t pointer_id;
    bool is_primary;
  };

  // Map from input source ID to its latest information.
  std::unordered_map<uint32_t, ScreenTouchEvent> screen_touch_events_;
  // Map from pointer ID to input source ID currently assigned to that pointer.
  std::unordered_map<int32_t, uint32_t> pointer_id_to_input_source_id_;

  uint32_t next_input_source_id_ = 1;

  // Must be last.
  base::WeakPtrFactory<ArCoreGl> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ArCoreGl);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_GL_H_
