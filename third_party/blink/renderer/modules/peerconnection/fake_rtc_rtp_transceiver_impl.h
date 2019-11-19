// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_FAKE_RTC_RTP_TRANSCEIVER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_FAKE_RTC_RTP_TRANSCEIVER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_rtc_rtp_receiver.h"
#include "third_party/blink/public/platform/web_rtc_rtp_source.h"
#include "third_party/blink/public/platform/web_rtc_rtp_transceiver.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_dtmf_sender_handler.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"

namespace blink {

// TODO(https://crbug.com/868868): Similar methods to this exist in many blink
// unittests. Move to a separate file and reuse it in all of them.
blink::WebMediaStreamTrack CreateWebMediaStreamTrack(
    const std::string& id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

class FakeRTCRtpSenderImpl : public blink::RTCRtpSenderPlatform {
 public:
  FakeRTCRtpSenderImpl(base::Optional<std::string> track_id,
                       std::vector<std::string> stream_ids,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  FakeRTCRtpSenderImpl(const FakeRTCRtpSenderImpl&);
  ~FakeRTCRtpSenderImpl() override;
  FakeRTCRtpSenderImpl& operator=(const FakeRTCRtpSenderImpl&);

  std::unique_ptr<blink::RTCRtpSenderPlatform> ShallowCopy() const override;
  uintptr_t Id() const override;
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> DtlsTransport() override;
  webrtc::DtlsTransportInformation DtlsTransportInformation() override;
  blink::WebMediaStreamTrack Track() const override;
  blink::WebVector<blink::WebString> StreamIds() const override;
  void ReplaceTrack(blink::WebMediaStreamTrack with_track,
                    blink::RTCVoidRequest* request) override;
  std::unique_ptr<blink::RtcDtmfSenderHandler> GetDtmfSender() const override;
  std::unique_ptr<webrtc::RtpParameters> GetParameters() const override;
  void SetParameters(blink::WebVector<webrtc::RtpEncodingParameters>,
                     webrtc::DegradationPreference,
                     blink::RTCVoidRequest*) override;
  void GetStats(blink::WebRTCStatsReportCallback,
                const blink::WebVector<webrtc::NonStandardGroupId>&) override;
  void SetStreams(
      const blink::WebVector<blink::WebString>& stream_ids) override;

 private:
  base::Optional<std::string> track_id_;
  std::vector<std::string> stream_ids_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class FakeRTCRtpReceiverImpl : public blink::WebRTCRtpReceiver {
 public:
  FakeRTCRtpReceiverImpl(
      const std::string& track_id,
      std::vector<std::string> stream_ids,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  FakeRTCRtpReceiverImpl(const FakeRTCRtpReceiverImpl&);
  ~FakeRTCRtpReceiverImpl() override;
  FakeRTCRtpReceiverImpl& operator=(const FakeRTCRtpReceiverImpl&);

  std::unique_ptr<blink::WebRTCRtpReceiver> ShallowCopy() const override;
  uintptr_t Id() const override;
  rtc::scoped_refptr<webrtc::DtlsTransportInterface> DtlsTransport() override;
  webrtc::DtlsTransportInformation DtlsTransportInformation() override;
  const blink::WebMediaStreamTrack& Track() const override;
  blink::WebVector<blink::WebString> StreamIds() const override;
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpSource>> GetSources()
      override;
  void GetStats(blink::WebRTCStatsReportCallback,
                const blink::WebVector<webrtc::NonStandardGroupId>&) override;
  std::unique_ptr<webrtc::RtpParameters> GetParameters() const override;
  void SetJitterBufferMinimumDelay(
      base::Optional<double> delay_seconds) override;

 private:
  blink::WebMediaStreamTrack track_;
  std::vector<std::string> stream_ids_;
};

class FakeRTCRtpTransceiverImpl : public blink::WebRTCRtpTransceiver {
 public:
  FakeRTCRtpTransceiverImpl(
      base::Optional<std::string> mid,
      FakeRTCRtpSenderImpl sender,
      FakeRTCRtpReceiverImpl receiver,
      bool stopped,
      webrtc::RtpTransceiverDirection direction,
      base::Optional<webrtc::RtpTransceiverDirection> current_direction);
  ~FakeRTCRtpTransceiverImpl() override;

  blink::WebRTCRtpTransceiverImplementationType ImplementationType()
      const override;
  uintptr_t Id() const override;
  blink::WebString Mid() const override;
  std::unique_ptr<blink::RTCRtpSenderPlatform> Sender() const override;
  std::unique_ptr<blink::WebRTCRtpReceiver> Receiver() const override;
  bool Stopped() const override;
  webrtc::RtpTransceiverDirection Direction() const override;
  void SetDirection(webrtc::RtpTransceiverDirection direction) override;
  base::Optional<webrtc::RtpTransceiverDirection> CurrentDirection()
      const override;
  base::Optional<webrtc::RtpTransceiverDirection> FiredDirection()
      const override;

 private:
  base::Optional<std::string> mid_;
  FakeRTCRtpSenderImpl sender_;
  FakeRTCRtpReceiverImpl receiver_;
  bool stopped_;
  webrtc::RtpTransceiverDirection direction_;
  base::Optional<webrtc::RtpTransceiverDirection> current_direction_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_FAKE_RTC_RTP_TRANSCEIVER_IMPL_H_
