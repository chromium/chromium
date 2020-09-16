// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_PROVIDER_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class XRFrameTransport;
class XRSession;
class XRSystem;
class XRWebGLLayer;

// This class manages requesting and dispatching frame updates, which includes
// pose information for a given XRDevice.
class XRFrameProvider final : public GarbageCollected<XRFrameProvider> {
 public:
  using ImmersiveSessionStartCallback = base::OnceClosure;

  explicit XRFrameProvider(XRSystem*);

  XRSession* immersive_session() const { return immersive_session_; }

  void OnSessionStarted(XRSession* session,
                        device::mojom::blink::XRSessionPtr session_ptr);

  // The FrameProvider needs to be notified before the page does that the
  // session has been ended so that requesting a new session is possible.
  // However, the non-immersive frame loop shouldn't start until after the page
  // has been notified.
  void OnSessionEnded(XRSession* session);
  void RestartNonImmersiveFrameLoop();

  void RequestFrame(XRSession*);

  void OnNonImmersiveVSync(double high_res_now_ms);

  void SubmitWebGLLayer(XRWebGLLayer*, bool was_changed);
  void UpdateWebGLLayerViewports(XRWebGLLayer*);

  void Dispose();
  void OnFocusChanged();

  device::mojom::blink::XRFrameDataProvider* GetImmersiveDataProvider() {
    return immersive_data_provider_.get();
  }

  void AddImmersiveSessionStartCallback(ImmersiveSessionStartCallback);

  virtual void Trace(Visitor*) const;

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

  // Called before dispatching a frame to an inline session. This method ensures
  // that inline session frame calls can be scheduled and that they are neither
  // served nor dropped if an immersive session is started while the inline
  // session was waiting to be served.
  void OnPreDispatchInlineFrame(
      XRSession* session,
      double timestamp,
      const base::Optional<gpu::MailboxHolder>& output_mailbox_holder,
      const base::Optional<gpu::MailboxHolder>& camera_image_mailbox_holder);

  const Member<XRSystem> xr_;

  // Immersive session state
  Member<XRSession> immersive_session_;
  Member<XRFrameTransport> frame_transport_;
  HeapMojoRemote<device::mojom::blink::XRFrameDataProvider,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      immersive_data_provider_;
  HeapMojoRemote<device::mojom::blink::XRPresentationProvider,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      immersive_presentation_provider_;
  device::mojom::blink::VRPosePtr immersive_frame_pose_;
  bool is_immersive_frame_position_emulated_ = false;

  // Time the first immersive frame has arrived - used to align the monotonic
  // clock the devices use with the base::TimeTicks.
  base::Optional<base::TimeTicks> first_immersive_frame_time_;
  // The time_delta value of the first immersive frame that has arrived.
  base::Optional<base::TimeDelta> first_immersive_frame_time_delta_;

  // Non-immersive session state
  HeapHashMap<Member<XRSession>,
              Member<DisallowNewWrapper<HeapMojoRemote<
                  device::mojom::blink::XRFrameDataProvider,
                  HeapMojoWrapperMode::kWithoutContextObserver>>>>
      non_immersive_data_providers_;
  HeapHashMap<Member<XRSession>, device::mojom::blink::XRFrameDataPtr>
      requesting_sessions_;

  Vector<ImmersiveSessionStartCallback> immersive_session_start_callbacks_;

  // This frame ID is XR-specific and is used to track when frames arrive at the
  // XR compositor so that it knows which poses to use, when to apply bounds
  // updates, etc.
  int16_t frame_id_ = -1;
  bool pending_immersive_vsync_ = false;
  bool pending_non_immersive_vsync_ = false;

  base::Optional<gpu::MailboxHolder> buffer_mailbox_holder_;
  base::Optional<gpu::MailboxHolder> camera_image_mailbox_holder_;
  bool last_has_focus_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_PROVIDER_H_
