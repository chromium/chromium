// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_AUDIO_SENDER_H_
#define MEDIA_CAST_SENDER_AUDIO_SENDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_callbacks.h"
#include "media/cast/cast_config.h"
#include "media/cast/sender/frame_sender.h"

namespace openscreen::cast {
class Sender;
}

namespace media::cast {

class AudioEncoder;

// Not thread safe. Only called from the main cast thread.
// This class owns all objects related to sending audio, objects that create RTP
// packets, congestion control, audio encoder, parsing and sending of
// RTCP packets.
// Additionally it posts a bunch of delayed tasks to the main thread for various
// timeouts.
class AudioSender : public FrameSender::Client {
 public:

  // New way of instantiating using an openscreen::cast::Sender. Since the
  // |Sender| instance is destroyed when renegotiation is complete, |this|
  // is also invalid and should be immediately torn down.
  AudioSender(scoped_refptr<CastEnvironment> cast_environment,
              const FrameSenderConfig& audio_config,
              StatusChangeOnceCallback status_change_cb,
              std::unique_ptr<openscreen::cast::Sender> sender);

  AudioSender(const AudioSender&) = delete;
  AudioSender& operator=(const AudioSender&) = delete;

  ~AudioSender() override;

  // Note: It is not guaranteed that |audio_frame| will actually be encoded and
  // sent, if AudioSender detects too many frames in flight.  Therefore, clients
  // should be careful about the rate at which this method is called.
  virtual void InsertAudio(std::unique_ptr<AudioBus> audio_bus,
                           base::TimeTicks recorded_time);

  void SetTargetPlayoutDelay(base::TimeDelta new_target_playout_delay);
  base::TimeDelta GetTargetPlayoutDelay() const;
  int GetEncoderBitrate() const;

  base::WeakPtr<AudioSender> AsWeakPtr();

 protected:
  // For mocking in unit tests.
  AudioSender();

  // FrameSender::Client overrides.
  int GetNumberOfFramesInEncoder() const final;
  base::TimeDelta GetEncoderBacklogDuration() const final;

 private:
  AudioSender(scoped_refptr<CastEnvironment> cast_environment,
              const FrameSenderConfig& audio_config,
              StatusChangeOnceCallback status_change_cb,
              std::unique_ptr<FrameSender> sender);

  // Called by the |audio_encoder_| with the next EncodedFrame to send.
  void OnEncodedAudioFrame(std::unique_ptr<SenderEncodedFrame> encoded_frame,
                           int samples_skipped);

  scoped_refptr<CastEnvironment> cast_environment_;

  // The number of RTP units advanced per second;
  const int rtp_timebase_ = 0;

  // The backing frame sender implementation.
  std::unique_ptr<FrameSender> frame_sender_;

  // Encodes AudioBuses into EncodedFrames.
  std::unique_ptr<AudioEncoder> audio_encoder_;

  // The number of audio samples enqueued in |audio_encoder_|.
  int samples_in_encoder_ = 0;

  // Used to calculate the percentage of lost frames. We currently report this
  // metric as the number of frames dropped in the entire session.
  int number_of_frames_inserted_ = 0;
  int number_of_frames_dropped_ = 0;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<AudioSender> weak_factory_{this};
};

}  // namespace media::cast

#endif  // MEDIA_CAST_SENDER_AUDIO_SENDER_H_
