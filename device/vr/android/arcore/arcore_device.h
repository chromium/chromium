// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ARCORE_DEVICE_H_
#define DEVICE_VR_ANDROID_ARCORE_ARCORE_DEVICE_H_

#include <jni.h>

#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "device/vr/android/arcore/arcore_gl.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_base.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace device {

class ArImageTransportFactory;
class ArCoreFactory;
class ArCoreGlThread;
class XrJavaCoordinator;
class CompositorDelegateProvider;
class MailboxToSurfaceBridge;
class MailboxToSurfaceBridgeFactory;

class COMPONENT_EXPORT(VR_ARCORE) ArCoreDevice : public VRDeviceBase {
 public:
  ArCoreDevice(
      std::unique_ptr<ArCoreFactory> arcore_factory,
      std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory,
      std::unique_ptr<MailboxToSurfaceBridgeFactory>
          mailbox_to_surface_bridge_factory,
      std::unique_ptr<XrJavaCoordinator> xr_java_coordinator,
      std::unique_ptr<CompositorDelegateProvider> compositor_delegate_provider,
      XrFrameSinkClientFactory xr_frame_sink_client_factory);

  ArCoreDevice(const ArCoreDevice&) = delete;
  ArCoreDevice& operator=(const ArCoreDevice&) = delete;

  ~ArCoreDevice() override;

  // VRDeviceBase implementation.
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;

  void ShutdownSession(mojom::XRRuntime::ShutdownSessionCallback) override;

  base::WeakPtr<ArCoreDevice> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnDrawingSurfaceReady(gfx::AcceleratedWidget window,
                             gpu::SurfaceHandle surface_handle,
                             ui::WindowAndroid* root_window,
                             display::Display::Rotation rotation,
                             const gfx::Size& frame_size);
  void OnDrawingSurfaceTouch(bool is_primary,
                             bool touching,
                             int32_t pointer_id,
                             const gfx::PointF& location);
  void OnDrawingSurfaceDestroyed();
  void OnSessionEnded();

  void PostTaskToGlThread(base::OnceClosure task);

  bool IsOnMainThread();

  // Called once the GL thread is started. At this point, it doesn't
  // have a valid GL context yet.
  void OnGlThreadReady(int render_process_id,
                       int render_frame_id,
                       bool use_overlay);

  // Replies to the pending mojo RequestSession request.
  void CallDeferredRequestSessionCallback(
      ArCoreGlInitializeStatus arcore_initialization_result);

  // Tells the GL thread to initialize a GL context and other resources,
  // using the supplied window as a drawing surface.
  void RequestArCoreGlInitialization(gfx::AcceleratedWidget window,
                                     gpu::SurfaceHandle surface_handle,
                                     ui::WindowAndroid* root_window,
                                     int rotation,
                                     const gfx::Size& size);

  // Called when the GL thread's GL context initialization completes.
  void OnArCoreGlInitializationComplete(
      ArCoreGlInitializeStatus arcore_initialization_result);

  void OnCreateSessionCallback(
      mojom::XRRuntime::RequestSessionCallback deferred_callback,
      ArCoreGlInitializeResult initialize_result,
      ArCoreGlCreateSessionResult create_session_result);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<ArCoreFactory> arcore_factory_;
  std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory_;
  std::unique_ptr<MailboxToSurfaceBridgeFactory> mailbox_bridge_factory_;
  std::unique_ptr<XrJavaCoordinator> xr_java_coordinator_;
  std::unique_ptr<CompositorDelegateProvider> compositor_delegate_provider_;
  XrFrameSinkClientFactory xr_frame_sink_client_factory_;

  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;

  // Because the FrameSinkClient needs to have a teardown method called on it
  // before it is destructed, we need to own it rather than the ArCoreGl. Note
  // that this *also* means that we need to guarantee that it outlives the Gl
  // thread.
  std::unique_ptr<XrFrameSinkClient> frame_sink_client_;

  // Encapsulates data with session lifetime.
  struct SessionState {
    SessionState();
    ~SessionState();

    std::unique_ptr<ArCoreGlThread> arcore_gl_thread_;
    bool is_arcore_gl_initialized_ = false;

    base::OnceClosure start_immersive_activity_callback_;

    // The initial requestSession triggers the initialization sequence, store
    // the callback for replying once that initialization completes. Only one
    // concurrent session is supported, other requests are rejected.
    mojom::XRRuntime::RequestSessionCallback pending_request_session_callback_;

    // Collections of features that were requested on the session.
    std::unordered_set<device::mojom::XRSessionFeature> required_features_;
    std::unordered_set<device::mojom::XRSessionFeature> optional_features_;

    device::mojom::XRDepthOptionsPtr depth_options_;

    // Collection of features that have been enabled on the session. Initially
    // empty, will be set only once the ArCoreGl has been initialized.
    std::unordered_set<device::mojom::XRSessionFeature> enabled_features_;

    std::optional<device::mojom::XRDepthConfig> depth_configuration_;

    std::vector<device::mojom::XRTrackedImagePtr> tracked_images_;

    viz::FrameSinkId frame_sink_id_;

    // Trace ID of the requestSession() call that resulted in creating this
    // session state.
    uint64_t request_session_trace_id_;

    // In case of driver bugs that need workarounds (see
    // ArImageTransport::OnSurfaceBridgeReady), allow a one-time
    // retry of session creation. This needs a copy of the original
    // session creation options.
    bool allow_retry_ = true;
    mojom::XRRuntimeSessionOptionsPtr options_clone_for_retry_;
    bool initiate_retry_ = false;
  };

  // This object is reset to initial values when ending a session. This helps
  // ensure that each session has consistent per-session state.
  std::unique_ptr<SessionState> session_state_;

  base::OnceCallback<void(bool)>
      on_request_arcore_install_or_update_result_callback_;
  base::OnceCallback<void(bool)> on_request_ar_module_result_callback_;

  // Must be last.
  base::WeakPtrFactory<ArCoreDevice> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ARCORE_DEVICE_H_
