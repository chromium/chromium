// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_AUDIO_STREAM_H_
#define REMOTING_PROTOCOL_WEBRTC_AUDIO_STREAM_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "remoting/protocol/audio_stream.h"
#include "third_party/webrtc/api/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class PeerConnectionInterface;
}  // namespace webrtc

namespace remoting::protocol {

class AudioSource;
class WebrtcAudioSourceAdapter;
class WebrtcTransport;

class WebrtcAudioStream : public AudioStream {
 public:
  WebrtcAudioStream();

  WebrtcAudioStream(const WebrtcAudioStream&) = delete;
  WebrtcAudioStream& operator=(const WebrtcAudioStream&) = delete;

  ~WebrtcAudioStream() override;

  void Start(scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
             std::unique_ptr<AudioSource> audio_source,
             WebrtcTransport* webrtc_transport);

  // AudioStream interface.
  void Pause(bool pause) override;

 private:
  scoped_refptr<WebrtcAudioSourceAdapter> source_adapter_;

  scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_AUDIO_STREAM_H_
