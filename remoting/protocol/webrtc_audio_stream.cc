// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_audio_stream.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/audio_source.h"
#include "remoting/protocol/webrtc_audio_source_adapter.h"
#include "remoting/protocol/webrtc_transport.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/rtc_base/ref_count.h"

namespace remoting {
namespace protocol {

const char kAudioStreamLabel[] = "audio_stream";
const char kAudioTrackLabel[] = "system_audio";

WebrtcAudioStream::WebrtcAudioStream() = default;
WebrtcAudioStream::~WebrtcAudioStream() = default;

void WebrtcAudioStream::Start(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    std::unique_ptr<AudioSource> audio_source,
    WebrtcTransport* webrtc_transport) {
  DCHECK(webrtc_transport);

  source_adapter_ =
      new rtc::RefCountedObject<WebrtcAudioSourceAdapter>(audio_task_runner);
  source_adapter_->Start(std::move(audio_source));

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory(
      webrtc_transport->peer_connection_factory());
  peer_connection_ = webrtc_transport->peer_connection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track =
      peer_connection_factory->CreateAudioTrack(kAudioTrackLabel,
                                                source_adapter_.get());

  webrtc::RtpTransceiverInit init;
  init.stream_ids = {kAudioStreamLabel};

  // value() DCHECKs if AddTransceiver() fails, which only happens if a track
  // was already added with the stream label.
  auto transceiver =
      peer_connection_->AddTransceiver(audio_track, init).value();

  webrtc_transport->OnAudioTransceiverCreated(transceiver);
}

void WebrtcAudioStream::Pause(bool pause) {
  source_adapter_->Pause(pause);
}

}  // namespace protocol
}  // namespace remoting
