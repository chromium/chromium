// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_TEST_CAST_STREAMING_TEST_SENDER_H_
#define FUCHSIA_CAST_STREAMING_TEST_CAST_STREAMING_TEST_SENDER_H_

#include "base/callback.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "components/cast/message_port/message_port.h"
#include "components/openscreen_platform/task_runner.h"
#include "fuchsia/cast_streaming/test/cast_message_port_sender_impl.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/data_buffer.h"
#include "media/base/video_decoder_config.h"
#include "net/base/ip_address.h"
#include "third_party/openscreen/src/cast/streaming/sender_session.h"

namespace cast_streaming {

// Cast Streaming Sender implementation for testing. This class provides basic
// functionality for starting a Cast Streaming Sender and sending audio and
// video frames to a Cast Streaming Receiver. Example usage:
//
// std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port;
// std::unique_ptr<cast_api_bindings::MessagePort> receiver_message_port;
// cast_api_bindings::MessagePort::CreatePair(&sender_message_port,
//                                            &receiver_message_port);
//
// // Send |receiver_message_port| to a Receiver and start it.
//
// CastStreamingTestSender sender;
// if !(sender.Start(
//     std::move(sender_message_port), net::IPAddress::IPv6Localhost(),
//     audio_config, video_config)) {
//   return;
// }
// sender.RunUntilStarted();
// sender.SendAudioBuffer(audio_buffer);
// sender.SendVideoBuffer(video_buffer);
// sender.Stop();
// sender.RunUntilStopped();
class CastStreamingTestSender : public openscreen::cast::SenderSession::Client {
 public:
  CastStreamingTestSender();
  ~CastStreamingTestSender() final;

  CastStreamingTestSender(const CastStreamingTestSender&) = delete;
  CastStreamingTestSender& operator=(const CastStreamingTestSender&) = delete;

  // Uses |message_port| as the Sender-end of a Cast Streaming MessagePort to
  // start a Cast Streaming Session with a Cast Streaming Receiver at
  // |receiver_address|. At least one of |audio_config| or |video_config| must
  // be set. Returns true on success.
  bool Start(std::unique_ptr<cast_api_bindings::MessagePort> message_port,
             net::IPAddress receiver_address,
             base::Optional<media::AudioDecoderConfig> audio_config,
             base::Optional<media::VideoDecoderConfig> video_config);

  // Ends the Cast Streaming Session.
  void Stop();

  // Sends |audio_buffer| or |video_buffer| to the Receiver. These can only be
  // called when is_active() returns true.
  void SendAudioBuffer(scoped_refptr<media::DataBuffer> audio_buffer);
  void SendVideoBuffer(scoped_refptr<media::DataBuffer> video_buffer,
                       bool is_key_frame);

  // After a successful call to Start(), will run until the Cast Streaming
  // Sender Session is active. After this call, is_active() will return true
  // and at least one of audio_decoder_config() or video_decoder_config() will
  // be set.
  void RunUntilStarted();

  // Runs until is_active() returns false.
  void RunUntilStopped();

  bool is_active() const { return is_active_; }
  const base::Optional<media::AudioDecoderConfig>& audio_decoder_config()
      const {
    return audio_decoder_config_;
  }
  const base::Optional<media::VideoDecoderConfig>& video_decoder_config()
      const {
    return video_decoder_config_;
  }

 private:
  void OnCastChannelClosed();

  // openscreen::cast::SenderSession::Client implementation.
  void OnNegotiated(const openscreen::cast::SenderSession* session,
                    openscreen::cast::SenderSession::ConfiguredSenders senders,
                    openscreen::cast::capture_recommendations::Recommendations
                        capture_recommendations) final;
  void OnError(const openscreen::cast::SenderSession* session,
               openscreen::Error error) final;

  openscreen_platform::TaskRunner task_runner_;
  openscreen::cast::Environment environment_;

  std::unique_ptr<openscreen::cast::SenderSession> sender_session_;
  std::unique_ptr<CastMessagePortSenderImpl> message_port_;

  openscreen::cast::Sender* audio_sender_ = nullptr;
  openscreen::cast::Sender* video_sender_ = nullptr;
  openscreen::cast::FrameId last_audio_reference_frame_id_;
  openscreen::cast::FrameId last_video_reference_frame_id_;

  bool is_active_ = false;
  base::Optional<media::AudioDecoderConfig> audio_decoder_config_;
  base::Optional<media::VideoDecoderConfig> video_decoder_config_;

  // Used to implement RunUntilStarted() and RunUntilStopped().
  base::OnceClosure sender_started_closure_;
  base::OnceClosure sender_stopped_closure_;
};

}  // namespace cast_streaming

#endif  // FUCHSIA_CAST_STREAMING_TEST_CAST_STREAMING_TEST_SENDER_H_
