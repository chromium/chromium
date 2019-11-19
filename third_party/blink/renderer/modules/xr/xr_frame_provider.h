// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_PROVIDER_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class XR;
class XRSession;
class XRFrameTransport;
class XRWebGLLayer;

// This class manages requesting and dispatching frame updates, which includes
// pose information for a given XRDevice.
class XRFrameProvider final : public GarbageCollected<XRFrameProvider> {
 public:
  explicit XRFrameProvider(XR*);

  XRSession* immersive_session() const { return immersive_session_; }

  void OnSessionStarted(XRSession* session,
                        device::mojom::blink::XRSessionPtr session_ptr);
  void OnSessionEnded(XRSession* session);

  void RequestFrame(XRSession*);

  void OnNonImmersiveVSync(double high_res_now_ms);

  void SubmitWebGLLayer(XRWebGLLayer*, bool was_changed);
  void UpdateWebGLLayerViewports(XRWebGLLayer*);

  void Dispose();
  void OnFocusChanged();

  device::mojom::blink::XRFrameDataProvider* GetImmersiveDataProvider() {
    return immersive_data_provider_.get();
  }

  virtual void Trace(blink::Visitor*);

 private:
  void OnImmersiveFrameData(device::mojom::blink::XRFrameDataPtr data);
  void OnNonImmersiveFrameData(XRSession* session,
                               device::mojom::blink::XRFrameDataPtr data);

  // Posts a request to the |XRFrameDataProvider| for the given session for
  // frame data. If the given session has no provider, it will be given null
  // frame data.
  void RequestNonImmersiveFrameData(XRSession* session);

  // TODO(https://crbug.com/955819): options should be removed from those
  // methods as they'll no longer be passed on a per-frame basis.
  void ScheduleImmersiveFrame(
      device::mojom::blink::XRFrameDataRequestOptionsPtr options);

  // Schedules an animation frame to service all non-immersive requesting
  // sessions. This will be postponed if there is a currently running immmersive
  // session.
  void ScheduleNonImmersiveFrame(
      device::mojom::blink::XRFrameDataRequestOptionsPtr options);

  void OnProviderConnectionError(XRSession* session);
  void ProcessScheduledFrame(device::mojom::blink::XRFrameDataPtr frame_data,
                             double high_res_now_ms);

  const Member<XR> xr_;

  // Immersive session state
  Member<XRSession> immersive_session_;
  Member<XRFrameTransport> frame_transport_;
  mojo::Remote<device::mojom::blink::XRFrameDataProvider>
      immersive_data_provider_;
  mojo::Remote<device::mojom::blink::XRPresentationProvider>
      immersive_presentation_provider_;
  device::mojom::blink::VRPosePtr immersive_frame_pose_;
  bool is_immersive_frame_position_emulated_ = false;

  // Non-immersive session state
  HeapHashMap<Member<XRSession>,
              mojo::Remote<device::mojom::blink::XRFrameDataProvider>>
      non_immersive_data_providers_;
  HeapHashMap<Member<XRSession>, device::mojom::blink::VRPosePtr>
      requesting_sessions_;

  // This frame ID is XR-specific and is used to track when frames arrive at the
  // XR compositor so that it knows which poses to use, when to apply bounds
  // updates, etc.
  int16_t frame_id_ = -1;
  bool pending_immersive_vsync_ = false;
  bool pending_non_immersive_vsync_ = false;

  base::Optional<gpu::MailboxHolder> buffer_mailbox_holder_;
  bool last_has_focus_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_PROVIDER_H_
