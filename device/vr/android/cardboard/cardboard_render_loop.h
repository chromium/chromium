// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_RENDER_LOOP_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_RENDER_LOOP_H_

#include <memory>
#include "base/android/java_handler_thread.h"
#include "base/memory/scoped_refptr.h"
#include "device/vr/android/cardboard/scoped_cardboard_objects.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/display/display.h"
#include "ui/gfx/native_widget_types.h"

namespace gl {
class GLSurface;
class GLContext;
}  // namespace gl

namespace device {

class CardboardImageTransport;
class CardboardImageTransportFactory;
class CardboardSdk;

using CardboardRequestSessionCallback =
    base::OnceCallback<void(mojom::XRRuntimeSessionResultPtr)>;

class CardboardRenderLoop : public base::android::JavaHandlerThread,
                            public device::mojom::XRFrameDataProvider,
                            public device::mojom::XRSessionController,
                            public device::mojom::XRPresentationProvider {
 public:
  CardboardRenderLoop(std::unique_ptr<CardboardImageTransportFactory>
                          cardboard_image_transport_factory,
                      std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge);
  ~CardboardRenderLoop() override;

  CardboardRenderLoop(const CardboardRenderLoop&) = delete;
  CardboardRenderLoop& operator=(const CardboardRenderLoop&) = delete;

  void CreateSession(CardboardRequestSessionCallback session_request_callback,
                     base::OnceClosure session_shutdown_callback,
                     CardboardSdk* cardboard_sdk,
                     gfx::AcceleratedWidget drawing_widget,
                     const gfx::Size& frame_size,
                     display::Display::Rotation display_rotation,
                     mojom::XRRuntimeSessionOptionsPtr options);

  // mojom::XRFrameDataProvider
  void GetFrameData(mojom::XRFrameDataRequestOptionsPtr options,
                    GetFrameDataCallback callback) override;

  void GetEnvironmentIntegrationProvider(
      mojo::PendingAssociatedReceiver<
          device::mojom::XREnvironmentIntegrationProvider> environment_provider)
      override;

  // XRPresentationProvider
  void UpdateLayerBounds(int16_t frame_index,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;
  void SubmitFrameMissing(int16_t frame_index, const gpu::SyncToken&) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox,
                   base::TimeDelta time_waited) override;
  void SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                   const gpu::SyncToken&,
                                   base::TimeDelta time_waited) override;

  // mojom::XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  void OnTriggerEvent(bool pressed);
  device::mojom::XRInputSourceStatePtr GetInputSourceState();

  base::WeakPtr<CardboardRenderLoop> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  void CleanUp() override;

 private:
  void OnCardboardImageTransportReady(bool success);
  bool InitializeGl(gfx::AcceleratedWidget drawing_widget);

  void OnBindingDisconnect();
  void CloseBindingsIfOpen();

  bool CanStartNewAnimatingFrame();

  bool IsSubmitFrameExpected(int16_t frame_index);

  void ProcessFrameFromMailbox(int16_t frame_index,
                               const gpu::MailboxHolder& mailbox);
  void ProcessFrameDrawnIntoTexture(const gpu::SyncToken& sync_token);
  void OnWebXrTokenSignaled(std::unique_ptr<gfx::GpuFence> gpu_fence);

  void TransitionProcessingFrameToRendering();
  void ClearRenderingFrame(WebXrFrame* frame);
  void RenderFrame(const gfx::Transform& uv_transform);
  void FinishFrame(int16_t frame_index);
  void FinishRenderingFrame(WebXrFrame* frame = nullptr);

  void Pause();
  void Resume();

  // These are only alive until CreateSession is called
  std::unique_ptr<CardboardImageTransportFactory>
      cardboard_image_transport_factory_;
  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;

  CardboardRequestSessionCallback session_request_callback_;
  base::OnceClosure session_shutdown_callback_;

  // Rendering Parameters
  gfx::Size texture_size_ = {0, 0};
  std::unique_ptr<CardboardImageTransport> cardboard_image_transport_;
  std::unique_ptr<WebXrPresentationState> webxr_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  gfx::RectF left_bounds_;
  gfx::RectF right_bounds_;

  // Input Parameters
  bool trigger_pressed_ = false;
  bool trigger_clicked_ = false;

  // Owned by our parent (cardboard_device)
  raw_ptr<CardboardSdk> cardboard_sdk_;

  // Session Controllers
  mojo::Receiver<mojom::XRFrameDataProvider> frame_data_receiver_{this};
  mojo::Receiver<mojom::XRSessionController> session_controller_receiver_{this};
  mojo::Receiver<device::mojom::XRPresentationProvider> presentation_receiver_{
      this};
  mojo::Remote<device::mojom::XRPresentationClient> submit_client_;

  // Stored Mojo data
  mojom::XRViewPtr left_eye_;
  mojom::XRViewPtr right_eye_;
  std::unordered_set<device::mojom::XRSessionFeature> enabled_features_;

  // Session State
  bool pending_shutdown_ = false;
  bool restrict_frame_data_ = false;
  bool is_paused_ = false;

  internal::ScopedCardboardObject<CardboardHeadTracker*> head_tracker_;

  // This closure saves arguments for the next GetFrameData call, including a
  // mojo callback. Must remain owned by CardboardRenderLoop, don't pass it off
  // to the task runner directly. Storing the mojo getframedata callback in a
  // closure owned by the task runner would lead to inconsistent state on
  // session shutdown. See https://crbug.com/1065572.
  base::OnceClosure pending_getframedata_;

  // Must be last.
  base::WeakPtrFactory<CardboardRenderLoop> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_RENDER_LOOP_H_
