// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_AUDIO_FIFO_SINK_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_AUDIO_FIFO_SINK_ADAPTER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/protocol/audio_sample_info.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace remoting {
class FifoBufferWriter;

namespace protocol {

// An adapter that receives decoded PCM audio frames from WebRTC on WebRTC's
// internal audio thread and writes them directly (single-copy) into an SPSC
// FifoBufferWriter.
//
// It supports dynamic playout format hot-swapping via an asynchronous handshake
// callback to prevent playing corrupt frames.
//
// Note: All methods of this class except for OnData() must be run on the
// same sequence (the construction sequence).
class WebrtcAudioFifoSinkAdapter : public webrtc::AudioTrackSinkInterface {
 public:
  // Callback triggered when the incoming playout format changes.
  // The adapter will pause SPSC writing and drop frames until the callback
  // invokes the acknowledgment closure.
  using FormatChangedCallback =
      base::RepeatingCallback<void(const AudioSampleInfo& info,
                                   base::OnceClosure acknowledgment_callback)>;

  // `format_changed_cb` is guaranteed to be invoked on the sequence where
  // this adapter was constructed.
  WebrtcAudioFifoSinkAdapter(std::unique_ptr<FifoBufferWriter> audio_writer,
                             FormatChangedCallback format_changed_cb);

  WebrtcAudioFifoSinkAdapter(const WebrtcAudioFifoSinkAdapter&) = delete;
  WebrtcAudioFifoSinkAdapter& operator=(const WebrtcAudioFifoSinkAdapter&) =
      delete;

  ~WebrtcAudioFifoSinkAdapter() override;

  // Dynamically binds or unbinds the active WebRTC audio track on the fly
  // without interrupting the SPSC Mojo buffer connection.
  // Pass nullptr to unbind. Must be called on the construction sequence.
  void SetTrack(webrtc::scoped_refptr<webrtc::AudioTrackInterface> track);

  // webrtc::AudioTrackSinkInterface implementation.
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override;

  bool FormatHandshakeCompleteForTesting() const;

 private:
  // Resumes SPSC writing once the new format is acknowledged by the consumer.
  void OnFormatAcknowledged(uint32_t sequence);

  // Intermediate format changed notifier running on the main thread to avoid
  // thread-safety issues with weak pointers on WebRTC's audio thread.
  void NotifyFormatChanged(const AudioSampleInfo& info, uint32_t sequence);

  // Task runner on which all public methods of this class except for OnData()
  // should be called.
  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;

  std::unique_ptr<FifoBufferWriter> audio_writer_;
  FormatChangedCallback format_changed_cb_;
  base::RepeatingCallback<void(const AudioSampleInfo&, uint32_t)>
      notify_format_changed_cb_;

  webrtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;

  // Cached current format.
  std::optional<AudioSampleInfo> current_format_;

  // Atomic sequence numbers to track format change handshakes deterministically
  // under rapid format oscillations.
  std::atomic<uint32_t> latest_posted_sequence_{0};
  std::atomic<uint32_t> latest_acknowledged_sequence_{0};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebrtcAudioFifoSinkAdapter> weak_factory_{this};
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_AUDIO_FIFO_SINK_ADAPTER_H_
