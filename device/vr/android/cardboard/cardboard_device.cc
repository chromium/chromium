// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_device.h"

#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "device/vr/android/cardboard/cardboard_device_params.h"
#include "device/vr/android/cardboard/cardboard_image_transport.h"
#include "device/vr/android/cardboard/cardboard_render_loop.h"
#include "device/vr/android/xr_activity_state_handler.h"

namespace device {

namespace {
const std::vector<mojom::XRSessionFeature>& GetSupportedFeatures() {
  static base::NoDestructor<std::vector<mojom::XRSessionFeature>>
      kSupportedFeatures{{
          mojom::XRSessionFeature::REF_SPACE_VIEWER,
          mojom::XRSessionFeature::REF_SPACE_LOCAL,
          mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
      }};

  return *kSupportedFeatures;
}
}  // namespace

CardboardDevice::CardboardDevice(
    std::unique_ptr<CardboardSdk> cardboard_sdk,
    std::unique_ptr<MailboxToSurfaceBridgeFactory>
        mailbox_to_surface_bridge_factory,
    std::unique_ptr<XrJavaCoordinator> xr_java_coordinator,
    std::unique_ptr<CompositorDelegateProvider> compositor_delegate_provider,
    std::unique_ptr<XrActivityStateHandlerFactory>
        activity_state_handler_factory)
    : VRDeviceBase(mojom::XRDeviceId::CARDBOARD_DEVICE_ID),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      cardboard_sdk_(std::move(cardboard_sdk)),
      mailbox_to_surface_bridge_factory_(
          std::move(mailbox_to_surface_bridge_factory)),
      xr_java_coordinator_(std::move(xr_java_coordinator)),
      compositor_delegate_provider_(std::move(compositor_delegate_provider)),
      activity_state_handler_factory_(
          std::move(activity_state_handler_factory)) {
  SetSupportedFeatures(GetSupportedFeatures());
}

CardboardDevice::~CardboardDevice() {
  // Notify any outstanding session requests that they have failed.
  OnCreateSessionResult(nullptr);

  // Ensure that any active sessions are terminated.
  OnSessionEnded();
}

void CardboardDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  // We can only have one exclusive session or serve one pending request session
  // request at a time.
  if (HasExclusiveSession() || pending_session_request_callback_) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Store these off since we'll potentially need to use them in the future
  // (after we've std::move'd the options object) as well as now.
  int render_process_id = options->render_process_id;
  int render_frame_id = options->render_frame_id;

  base::android::ScopedJavaLocalRef<jobject> application_context =
      xr_java_coordinator_->GetActivityFrom(render_process_id, render_frame_id);
  if (!application_context.obj()) {
    DLOG(ERROR) << "Unable to retrieve the Java context/activity!";
    std::move(callback).Run(nullptr);
    return;
  }

  cardboard_sdk_->Initialize(application_context.obj());

  // It's an error to close a mojo pipe with an outstanding callback. Since we
  // are not sure if we will be continued immediately or potentially get
  // destroyed instead of a Resume event, store the callback now, so that it
  // can be cleaned up properly during destruction.
  pending_session_request_callback_ = std::move(callback);

  base::OnceClosure continue_callback =
      base::BindOnce(&CardboardDevice::OnCardboardParametersAcquired,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options),
                     render_process_id, render_frame_id);

  auto params = CardboardDeviceParams::GetSavedDeviceParams();
  if (params.IsValid()) {
    std::move(continue_callback).Run();
    return;
  }

  // This will suspend us and will trigger the XrActivityStateHandler on Resume.
  std::unique_ptr<XrActivityStateHandler> activity_state_handler =
      activity_state_handler_factory_->Create(render_process_id,
                                              render_frame_id);
  cardboard_sdk_->ScanQrCodeAndSaveDeviceParams(
      std::move(activity_state_handler), std::move(continue_callback));
}

void CardboardDevice::OnCardboardParametersAcquired(
    mojom::XRRuntimeSessionOptionsPtr options,
    int render_process_id,
    int render_frame_id) {
  // Set HasExclusiveSession status to true. This lasts until OnSessionEnded.
  OnStartPresenting();

  render_loop_ = std::make_unique<CardboardRenderLoop>(
      std::make_unique<CardboardImageTransportFactory>(),
      mailbox_to_surface_bridge_factory_->Create());

  // Start the render loop now. Any tasks that we post to it won't run until it
  // finishes starting.
  render_loop_->Start();

  auto ready_callback = base::BindRepeating(
      &CardboardDevice::OnDrawingSurfaceReady, weak_ptr_factory_.GetWeakPtr());
  auto touch_callback = base::BindRepeating(
      &CardboardDevice::OnDrawingSurfaceTouch, weak_ptr_factory_.GetWeakPtr());
  auto destroyed_callback =
      base::BindOnce(&CardboardDevice::OnDrawingSurfaceDestroyed,
                     weak_ptr_factory_.GetWeakPtr());
  auto xr_session_button_callback =
      base::BindOnce(&CardboardDevice::OnXrSessionButtonTouched,
                     weak_ptr_factory_.GetWeakPtr());

  // While options_ is only used in OnDrawingSurfaceReady, stashing it as a
  // member allows us to control its lifetime relative to the callback which can
  // prevent some hard-to-debug issues.
  options_ = std::move(options);

  xr_java_coordinator_->RequestVrSession(
      render_process_id, render_frame_id, *compositor_delegate_provider_.get(),
      std::move(ready_callback), std::move(touch_callback),
      std::move(destroyed_callback), std::move(xr_session_button_callback));
}

void CardboardDevice::OnXrSessionButtonTouched() {
  // The ScanQrCodeAndSaveDeviceParams() method calls the
  // CardboardQrCode_scanQrCodeAndSaveDeviceParams() Cardboard API entry which
  // in turn launches a new QR code scanner activity in order to scan a QR code
  // with the parameters of a new Cardboard viewer. The way said activity works
  // is the following:
  // - Uses the Camera to scan a Cardboard QR code.
  // - Gets the device parameters from the URL from the scanned QR code.
  // - Once scanned, saves the obtained device parameters in the scoped
  //   storage.
  // - In case the scan is skipped, the current device parameter are left
  //   untouched.
  // - The activity finishes. See
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/cardboard/src/sdk/qrcode/android/java/com/google/cardboard/sdk/QrCodeCaptureActivity.java;l=270
  //
  // Next, the activity that invoked the QR code scanner is resumed and, as
  // part the resume process it will have to obtain the newly saved device
  // parameter and recreate the distortion meshes. See
  // https://source.chromium.org/chromium/chromium/src/+/main:device/vr/android/cardboard/cardboard_image_transport.cc;l=64
  cardboard_sdk_->ScanQrCodeAndSaveDeviceParams();
}

void CardboardDevice::OnDrawingSurfaceReady(gfx::AcceleratedWidget window,
                                            gpu::SurfaceHandle surface_handle,
                                            ui::WindowAndroid* root_window,
                                            display::Display::Rotation rotation,
                                            const gfx::Size& frame_size) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__ << ": size=" << frame_size.width() << "x"
           << frame_size.height() << " rotation=" << static_cast<int>(rotation);
  auto session_shutdown_callback = base::BindPostTask(
      main_thread_task_runner_,
      base::BindOnce(&CardboardDevice::OnDrawingSurfaceDestroyed,
                     weak_ptr_factory_.GetWeakPtr()));
  auto session_result_callback =
      base::BindPostTask(main_thread_task_runner_,
                         base::BindOnce(&CardboardDevice::OnCreateSessionResult,
                                        weak_ptr_factory_.GetWeakPtr()));

  PostTaskToRenderThread(base::BindOnce(
      &CardboardRenderLoop::CreateSession, render_loop_->GetWeakPtr(),
      std::move(session_result_callback), std::move(session_shutdown_callback),
      cardboard_sdk_.get(), window, frame_size, rotation, std::move(options_)));
}

void CardboardDevice::OnDrawingSurfaceTouch(bool is_primary,
                                            bool touching,
                                            int32_t pointer_id,
                                            const gfx::PointF& location) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DVLOG(3) << __func__ << ": pointer_id=" << pointer_id
           << " is_primary=" << is_primary << " touching=" << touching;

  // Cardboard doesn't care about anything but the primary pointer.
  if (!is_primary) {
    return;
  }

  // It's possible that we could get some touch events trail in after we've
  // decided to shutdown the render loop due to scheduling conflicts.
  if (!render_loop_) {
    return;
  }

  // Cardboard touch events don't make use of any of the pointer information,
  // so we only need to notify that an touch has happened.
  PostTaskToRenderThread(base::BindOnce(&CardboardRenderLoop::OnTriggerEvent,
                                        render_loop_->GetWeakPtr(), touching));
}

void CardboardDevice::OnDrawingSurfaceDestroyed() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  // This is really the only mechanism that the java code has to signal to us
  // that session creation failed. So in the event that there's still a pending
  // session request, we need to notify our caller that it failed.
  OnCreateSessionResult(nullptr);

  OnSessionEnded();
}

void CardboardDevice::OnCreateSessionResult(
    mojom::XRRuntimeSessionResultPtr result) {
  if (pending_session_request_callback_) {
    std::move(pending_session_request_callback_).Run(std::move(result));
  }
}

void CardboardDevice::ShutdownSession(
    mojom::XRRuntime::ShutdownSessionCallback on_completed) {
  DVLOG(1) << __func__;
  OnDrawingSurfaceDestroyed();
  std::move(on_completed).Run();
}

void CardboardDevice::OnSessionEnded() {
  DVLOG(1) << __func__;

  if (!HasExclusiveSession()) {
    return;
  }

  // The render loop destructor stops itself, so we don't need to stop it here.
  render_loop_.reset();

  // This may be a no-op in case session end was initiated from the Java side.
  xr_java_coordinator_->EndSession();

  // This sets HasExclusiveSession status to false.
  OnExitPresent();
}

void CardboardDevice::PostTaskToRenderThread(base::OnceClosure task) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  render_loop_->task_runner()->PostTask(FROM_HERE, std::move(task));
}

}  // namespace device
