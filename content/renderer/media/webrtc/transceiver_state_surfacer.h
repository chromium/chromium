// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_TRANSCEIVER_STATE_SURFACER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_TRANSCEIVER_STATE_SURFACER_H_

#include "content/renderer/media/webrtc/rtc_rtp_transceiver.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "third_party/webrtc/api/rtptransceiverinterface.h"
#include "third_party/webrtc/rtc_base/refcount.h"
#include "third_party/webrtc/rtc_base/refcountedobject.h"

namespace content {

// Takes care of creating and initializing transceiver states (including sender
// and receiver states). This is usable for both blocking and non-blocking calls
// to the webrtc signaling thread.
//
// The surfacer is initialized on the signaling thread and states are obtained
// on the main thread. It is designed to be initialized and handed off; it is
// not thread safe for concurrent thread usage.
class CONTENT_EXPORT TransceiverStateSurfacer {
 public:
  TransceiverStateSurfacer(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner);
  TransceiverStateSurfacer(TransceiverStateSurfacer&&);
  TransceiverStateSurfacer(const TransceiverStateSurfacer&) = delete;
  ~TransceiverStateSurfacer();

  // This is intended to be used for moving the object from the signaling thread
  // to the main thread and as such has no thread checks. Once moved to the main
  // this should only be invoked on the main thread.
  TransceiverStateSurfacer& operator=(TransceiverStateSurfacer&&);
  TransceiverStateSurfacer& operator=(const TransceiverStateSurfacer&) = delete;

  bool is_initialized() const { return is_initialized_; }

  // Must be invoked on the signaling thread.
  void Initialize(
      scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
      std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
          transceivers);

  // Must be invoked on the main thread.
  std::vector<RtpTransceiverState> ObtainStates();

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner_;
  bool is_initialized_;
  bool states_obtained_;
  std::vector<RtpTransceiverState> transceiver_states_;
};

// A dummy implementation of a transceiver used to surface sender state
// information only. It is not thread safe, only designed to be passed on to
// TransceiverStateSurfacer::Initialize().
class CONTENT_EXPORT SurfaceSenderStateOnly
    : public rtc::RefCountedObject<webrtc::RtpTransceiverInterface> {
 public:
  SurfaceSenderStateOnly(rtc::scoped_refptr<webrtc::RtpSenderInterface> sender);
  ~SurfaceSenderStateOnly() override;

  cricket::MediaType media_type() const override;
  absl::optional<std::string> mid() const override;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender() const override;
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver() const override;
  bool stopped() const override;
  webrtc::RtpTransceiverDirection direction() const override;
  void SetDirection(webrtc::RtpTransceiverDirection new_direction) override;
  absl::optional<webrtc::RtpTransceiverDirection> current_direction()
      const override;
  void Stop() override;

 private:
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender_;
};

// A dummy implementation of a transceiver used to surface receiver state
// information only. It is not thread safe, only designed to be passed on to
// TransceiverStateSurfacer::Initialize().
class CONTENT_EXPORT SurfaceReceiverStateOnly
    : public rtc::RefCountedObject<webrtc::RtpTransceiverInterface> {
 public:
  SurfaceReceiverStateOnly(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver);
  ~SurfaceReceiverStateOnly() override;

  cricket::MediaType media_type() const override;
  absl::optional<std::string> mid() const override;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender() const override;
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver() const override;
  bool stopped() const override;
  webrtc::RtpTransceiverDirection direction() const override;
  void SetDirection(webrtc::RtpTransceiverDirection new_direction) override;
  absl::optional<webrtc::RtpTransceiverDirection> current_direction()
      const override;
  void Stop() override;

 private:
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_TRANSCEIVER_STATE_SURFACER_H_
