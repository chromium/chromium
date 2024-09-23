// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_NETWORK_SPEECH_RECOGNITION_ENGINE_IMPL_H_
#define CONTENT_BROWSER_SPEECH_NETWORK_SPEECH_RECOGNITION_ENGINE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/speech/audio_encoder.h"
#include "components/speech/chunked_byte_buffer.h"
#include "components/speech/downstream_loader.h"
#include "components/speech/downstream_loader_client.h"
#include "components/speech/upstream_loader.h"
#include "components/speech/upstream_loader_client.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/common/content_export.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "media/mojo/mojom/speech_recognition_grammar.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

class AudioChunk;

namespace base {
class TimeDelta;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

// This is the network implementation for `SpeechRecognitionEngine`, which is
// supporting continuous recognition by means of interaction with the Google
// streaming speech recognition webservice.
//
// This class establishes two HTTPS connections with the webservice for each
// session, herein called "upstream" and "downstream". Audio chunks are sent on
// the upstream by means of a chunked HTTP POST upload. Recognition results are
// retrieved in a full-duplex fashion (i.e. while pushing audio on the upstream)
// on the downstream by means of a chunked HTTP GET request. Pairing between the
// two stream is handled through a randomly generated key, unique for each
// request, which is passed in the &pair= arg to both stream request URLs. In
// the case of a regular session, the upstream is closed when the audio capture
// ends (notified through a |AudioChunksEnded| call) and the downstream waits
// for a corresponding server closure (eventually some late results can come
// after closing the upstream). Both streams are guaranteed to be closed when
// |EndRecognition| call is issued.

class CONTENT_EXPORT NetworkSpeechRecognitionEngineImpl
    : public SpeechRecognitionEngine,
      public speech::UpstreamLoaderClient,
      public speech::DownstreamLoaderClient {
 public:
  // Network engine configuration.
  struct CONTENT_EXPORT Config {
    Config();
    ~Config();

    std::string language;
    std::vector<media::mojom::SpeechRecognitionGrammar> grammars;
    bool filter_profanities = false;
    bool continuous = true;
    bool interim_results = true;
    uint32_t max_hypotheses;
    std::string origin_url;
    int audio_sample_rate;
    int audio_num_bits_per_sample;
    std::string auth_token;
    std::string auth_scope;
    scoped_refptr<SpeechRecognitionSessionPreamble> preamble;
  };

  // Duration of each audio packet.
  static const int kAudioPacketIntervalMs;

  // |accept_language| is the default Accept-Language header.
  NetworkSpeechRecognitionEngineImpl(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      const std::string& accept_language);

  NetworkSpeechRecognitionEngineImpl(
      const NetworkSpeechRecognitionEngineImpl&) = delete;
  NetworkSpeechRecognitionEngineImpl& operator=(
      const NetworkSpeechRecognitionEngineImpl&) = delete;

  ~NetworkSpeechRecognitionEngineImpl() override;

  // Sets the URL requests are sent to for tests.
  static void set_web_service_base_url_for_tests(
      const char* base_url_for_tests);

  void SetConfig(const Config& config);
  bool IsRecognitionPending() const;

  // content::SpeechRecognitionEngine:
  void StartRecognition() override;
  void EndRecognition() override;
  void TakeAudioChunk(const AudioChunk& data) override;
  void AudioChunksEnded() override;
  int GetDesiredAudioChunkDurationMs() const override;

 private:
  friend class speech::UpstreamLoaderClient;
  friend class speech::DownstreamLoader;

  // Response status codes from the speech recognition webservice.
  static const int kWebserviceStatusNoError;
  static const int kWebserviceStatusErrorNoMatch;

  // Frame type for framed POST data. Do NOT change these. They must match
  // values the server expects.
  enum FrameType { FRAME_PREAMBLE_AUDIO = 0, FRAME_RECOGNITION_AUDIO = 1 };

  // Data types for the internal Finite State Machine (FSM).
  enum FSMState {
    STATE_IDLE = 0,
    STATE_BOTH_STREAMS_CONNECTED,
    STATE_WAITING_DOWNSTREAM_RESULTS,
    STATE_MAX_VALUE = STATE_WAITING_DOWNSTREAM_RESULTS
  };

  enum FSMEvent {
    EVENT_END_RECOGNITION = 0,
    EVENT_START_RECOGNITION,
    EVENT_AUDIO_CHUNK,
    EVENT_AUDIO_CHUNKS_ENDED,
    EVENT_UPSTREAM_ERROR,
    EVENT_DOWNSTREAM_ERROR,
    EVENT_DOWNSTREAM_RESPONSE,
    EVENT_DOWNSTREAM_CLOSED,
    EVENT_MAX_VALUE = EVENT_DOWNSTREAM_CLOSED
  };

  struct FSMEventArgs {
    explicit FSMEventArgs(FSMEvent event_value);

    FSMEventArgs(const FSMEventArgs&) = delete;
    FSMEventArgs& operator=(const FSMEventArgs&) = delete;

    ~FSMEventArgs();

    FSMEvent event;

    // In case of EVENT_AUDIO_CHUNK, holds the chunk pushed by |TakeAudioChunk|.
    scoped_refptr<const AudioChunk> audio_data;

    // In case of EVENT_DOWNSTREAM_RESPONSE, hold the current chunk bytes.
    std::unique_ptr<std::vector<uint8_t>> response;
  };

  // speech::UpstreamLoaderClient
  void OnUpstreamDataComplete(bool success, int response_code) override;

  // speech::DownstreamLoaderClient
  void OnDownstreamDataReceived(std::string_view new_response_data) override;
  void OnDownstreamDataComplete(bool success, int response_code) override;

  // Entry point for pushing any new external event into the recognizer FSM.
  void DispatchEvent(const FSMEventArgs& event_args);

  // Defines the behavior of the recognizer FSM, selecting the appropriate
  // transition according to the current state and event.
  FSMState ExecuteTransitionAndGetNextState(const FSMEventArgs& event_args);

  // The methods below handle transitions of the recognizer FSM.
  FSMState ConnectBothStreams(const FSMEventArgs& event_args);
  FSMState TransmitAudioUpstream(const FSMEventArgs& event_args);
  FSMState ProcessDownstreamResponse(const FSMEventArgs& event_args);
  FSMState RaiseNoMatchErrorIfGotNoResults(const FSMEventArgs& event_args);
  FSMState CloseUpstreamAndWaitForResults(const FSMEventArgs& event_args);
  FSMState CloseDownstream(const FSMEventArgs& event_args);
  FSMState AbortSilently(const FSMEventArgs& event_args);
  FSMState AbortWithError(const FSMEventArgs& event_args);
  FSMState Abort(media::mojom::SpeechRecognitionErrorCode error);
  FSMState DoNothing(const FSMEventArgs& event_args);
  FSMState NotFeasible(const FSMEventArgs& event_args);

  std::string GetAcceptedLanguages() const;
  std::string GenerateRequestKey() const;

  // Upload a single chunk of audio data. Handles both unframed and framed
  // upload formats, and uses the appropriate one.
  void UploadAudioChunk(const std::string& data, FrameType type, bool is_final);

  // The total audio duration of the upstream request.
  base::TimeDelta upstream_audio_duration_;

  Config config_;
  std::unique_ptr<speech::UpstreamLoader> upstream_loader_;
  std::unique_ptr<speech::DownstreamLoader> downstream_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  const std::string accept_language_;
  std::unique_ptr<AudioEncoder> encoder_;
  std::unique_ptr<AudioEncoder> preamble_encoder_;
  speech::ChunkedByteBuffer chunked_byte_buffer_;
  bool got_last_definitive_result_ = false;
  bool is_dispatching_event_ = false;
  bool use_framed_post_data_ = false;
  FSMState state_ = FSMState::STATE_IDLE;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_NETWORK_SPEECH_RECOGNITION_ENGINE_IMPL_H_
