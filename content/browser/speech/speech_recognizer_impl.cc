// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/speech/speech_recognizer_impl.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/speech/audio_buffer.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_audio_forwarder_config.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "services/audio/public/cpp/device_factory.h"

#if BUILDFLAG(IS_WIN)
#include "media/audio/win/core_audio_util_win.h"
#endif

using media::AudioBus;
using media::AudioConverter;
using media::AudioGlitchInfo;
using media::AudioParameters;
using media::ChannelLayout;

namespace content {

// Private class which encapsulates the audio converter and the
// AudioConverter::InputCallback. It handles resampling, buffering and
// channel mixing between input and output parameters.
class SpeechRecognizerImpl::OnDataConverter
    : public media::AudioConverter::InputCallback {
 public:
  OnDataConverter(const AudioParameters& input_params,
                  const AudioParameters& output_params);

  OnDataConverter(const OnDataConverter&) = delete;
  OnDataConverter& operator=(const OnDataConverter&) = delete;

  ~OnDataConverter() override;

  // Converts input audio |data| bus into an AudioChunk where the input format
  // is given by |input_parameters_| and the output format by
  // |output_parameters_|.
  scoped_refptr<AudioChunk> Convert(const AudioBus* data);

  bool data_was_converted() const { return data_was_converted_; }

 private:
  // media::AudioConverter::InputCallback implementation.
  double ProvideInput(AudioBus* dest,
                      uint32_t frames_delayed,
                      const AudioGlitchInfo& glitch_info) override;

  // Handles resampling, buffering, and channel mixing between input and output
  // parameters.
  AudioConverter audio_converter_;

  std::unique_ptr<AudioBus> input_bus_;
  std::unique_ptr<AudioBus> output_bus_;
  const AudioParameters input_parameters_;
  const AudioParameters output_parameters_;
  bool data_was_converted_;
};

namespace {

// The following constants are related to the volume level indicator shown in
// the UI for recorded audio.
// Multiplier used when new volume is greater than previous level.
const float kUpSmoothingFactor = 1.0f;
// Multiplier used when new volume is lesser than previous level.
const float kDownSmoothingFactor = 0.7f;
// RMS dB value of a maximum (unclipped) sine wave for int16_t samples.
const float kAudioMeterMaxDb = 90.31f;
// This value corresponds to RMS dB for int16_t with 6 most-significant-bits =
// 0.
// Values lower than this will display as empty level-meter.
const float kAudioMeterMinDb = 30.0f;
const float kAudioMeterDbRange = kAudioMeterMaxDb - kAudioMeterMinDb;

// Maximum level to draw to display unclipped meter. (1.0f displays clipping.)
const float kAudioMeterRangeMaxUnclipped = 47.0f / 48.0f;

// Returns true if more than 5% of the samples are at min or max value.
bool DetectClipping(const AudioChunk& chunk) {
  const int num_samples = chunk.NumSamples();
  const int16_t* samples = chunk.SamplesData16();
  const int kThreshold = num_samples / 20;
  int clipping_samples = 0;

  for (int i = 0; i < num_samples; ++i) {
    if (samples[i] <= -32767 || samples[i] >= 32767) {
      if (++clipping_samples > kThreshold)
        return true;
    }
  }
  return false;
}

}  // namespace

media::AudioSystem* SpeechRecognizerImpl::audio_system_for_tests_ = nullptr;
media::AudioCapturerSource*
    SpeechRecognizerImpl::audio_capturer_source_for_tests_ = nullptr;

// SpeechRecognizerImpl::OnDataConverter implementation

SpeechRecognizerImpl::OnDataConverter::OnDataConverter(
    const AudioParameters& input_params,
    const AudioParameters& output_params)
    : audio_converter_(input_params, output_params, false),
      input_bus_(AudioBus::Create(input_params)),
      output_bus_(AudioBus::Create(output_params)),
      input_parameters_(input_params),
      output_parameters_(output_params),
      data_was_converted_(false) {
  audio_converter_.AddInput(this);
  audio_converter_.PrimeWithSilence();
}

SpeechRecognizerImpl::OnDataConverter::~OnDataConverter() {
  // It should now be safe to unregister the converter since no more OnData()
  // callbacks are outstanding at this point.
  audio_converter_.RemoveInput(this);
}

scoped_refptr<AudioChunk> SpeechRecognizerImpl::OnDataConverter::Convert(
    const AudioBus* data) {
  CHECK_EQ(data->frames(), input_parameters_.frames_per_buffer());
  data_was_converted_ = false;
  // Copy recorded audio to the |input_bus_| for later use in ProvideInput().
  data->CopyTo(input_bus_.get());
  // Convert the audio and place the result in |output_bus_|. This call will
  // result in a ProvideInput() callback where the actual input is provided.
  // However, it can happen that the converter contains enough cached data
  // to return a result without calling ProvideInput(). The caller of this
  // method should check the state of data_was_converted_() and make an
  // additional call if it is set to false at return.
  // See http://crbug.com/506051 for details.
  audio_converter_.Convert(output_bus_.get());
  // Create an audio chunk based on the converted result.
  scoped_refptr<AudioChunk> chunk(new AudioChunk(
      output_parameters_.GetBytesPerBuffer(media::kSampleFormatS16),
      kNumBitsPerAudioSample / 8));

  static_assert(SpeechRecognizerImpl::kNumBitsPerAudioSample == 16,
                "kNumBitsPerAudioSample must match interleaving type.");
  output_bus_->ToInterleaved<media::SignedInt16SampleTypeTraits>(
      output_bus_->frames(),
      reinterpret_cast<int16_t*>(chunk->writable_data()));
  return chunk;
}

double SpeechRecognizerImpl::OnDataConverter::ProvideInput(
    AudioBus* dest,
    uint32_t frames_delayed,
    const AudioGlitchInfo& glitch_info) {
  // Read from the input bus to feed the converter.
  input_bus_->CopyTo(dest);
  // Indicate that the recorded audio has in fact been used by the converter.
  data_was_converted_ = true;
  return 1;
}

// SpeechRecognizerImpl implementation

SpeechRecognizerImpl::SpeechRecognizerImpl(
    SpeechRecognitionEventListener* listener,
    media::AudioSystem* audio_system,
    int session_id,
    bool continuous,
    bool provisional_results,
    std::unique_ptr<SpeechRecognitionEngine> engine,
    std::optional<SpeechRecognitionAudioForwarderConfig> audio_forwarder_config)
    : SpeechRecognizer(listener, session_id),
      audio_system_(audio_system),
      recognition_engine_(std::move(engine)),
      sample_rate_(audio_forwarder_config.has_value()
                       ? audio_forwarder_config.value().sample_rate
                       : kAudioSampleRate),
      endpointer_(sample_rate_),
      provisional_results_(provisional_results),
      end_of_utterance_(false),
      use_audio_capturer_source_(!audio_forwarder_config.has_value()),
      audio_forwarder_receiver_(
          this,
          audio_forwarder_config.has_value()
              ? std::move(audio_forwarder_config.value().audio_forwarder)
              : mojo::NullReceiver()) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(recognition_engine_ != nullptr);
  DCHECK(audio_system_ != nullptr);

  if (!continuous) {
    // In single shot (non-continous) recognition,
    // the session is automatically ended after:
    //  - 0.5 seconds of silence if time <  3 seconds
    //  - 1   seconds of silence if time >= 3 seconds
    endpointer_.set_speech_input_complete_silence_length(
        base::Time::kMicrosecondsPerSecond / 2);
    endpointer_.set_long_speech_input_complete_silence_length(
        base::Time::kMicrosecondsPerSecond);
    endpointer_.set_long_speech_length(3 * base::Time::kMicrosecondsPerSecond);
  } else {
    // In continuous recognition, the session is automatically ended after 15
    // seconds of silence.
    const int64_t cont_timeout_us = base::Time::kMicrosecondsPerSecond * 15;
    endpointer_.set_speech_input_complete_silence_length(cont_timeout_us);
    endpointer_.set_long_speech_length(0);  // Use only a single timeout.
  }
  endpointer_.StartSession();
  recognition_engine_->set_delegate(this);
}

// -------  Methods that trigger Finite State Machine (FSM) events ------------

// NOTE:all the external events and requests should be enqueued (PostTask), even
// if they come from the same (IO) thread, in order to preserve the relationship
// of causality between events and avoid interleaved event processing due to
// synchronous callbacks.

void SpeechRecognizerImpl::StartRecognition(const std::string& device_id) {
  DCHECK(!device_id.empty());
  device_id_ = device_id;

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(),
                                FSMEventArgs(EVENT_PREPARE)));
}

void SpeechRecognizerImpl::AbortRecognition() {
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(),
                                FSMEventArgs(EVENT_ABORT)));
}

void SpeechRecognizerImpl::StopAudioCapture() {
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(),
                                FSMEventArgs(EVENT_STOP_CAPTURE)));
}

bool SpeechRecognizerImpl::IsActive() const {
  // Checking the FSM state from another thread (thus, while the FSM is
  // potentially concurrently evolving) is meaningless.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return state_ != STATE_IDLE && state_ != STATE_ENDED;
}

bool SpeechRecognizerImpl::IsCapturingAudio() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);  // See IsActive().
  const bool is_capturing_audio = state_ >= STATE_STARTING &&
                                  state_ <= STATE_RECOGNIZING;
  return is_capturing_audio;
}

const SpeechRecognitionEngine&
SpeechRecognizerImpl::recognition_engine() const {
  return *(recognition_engine_.get());
}

SpeechRecognizerImpl::~SpeechRecognizerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  endpointer_.EndSession();
  if (use_audio_capturer_source_ && GetAudioCapturerSource()) {
    GetAudioCapturerSource()->Stop();
    audio_capturer_source_ = nullptr;
  }
}

void SpeechRecognizerImpl::Capture(const AudioBus* data,
                                   base::TimeTicks audio_capture_time,
                                   const AudioGlitchInfo& glitch_info,
                                   double volume,
                                   bool key_pressed) {
  // Convert audio from native format to fixed format used by WebSpeech.
  FSMEventArgs event_args(EVENT_AUDIO_DATA);
  event_args.audio_chunk = audio_converter_->Convert(data);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(), event_args));
  // See http://crbug.com/506051 regarding why one extra convert call can
  // sometimes be required. It should be a rare case.
  if (!audio_converter_->data_was_converted()) {
    event_args.audio_chunk = audio_converter_->Convert(data);
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                  weak_ptr_factory_.GetWeakPtr(), event_args));
  }
  // Something is seriously wrong here and we are most likely missing some
  // audio segments.
  CHECK(audio_converter_->data_was_converted());
}

void SpeechRecognizerImpl::OnCaptureError(
    media::AudioCapturerSource::ErrorCode code,
    const std::string& message) {
  FSMEventArgs event_args(EVENT_AUDIO_ERROR);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(), event_args));
}

void SpeechRecognizerImpl::AddAudioFromRenderer(
    media::mojom::AudioDataS16Ptr buffer) {
  if (audio_converter_ == nullptr) {
    return;
  }

  std::unique_ptr<media::AudioBus> data =
      AudioBus::Create(buffer->channel_count, buffer->frame_count);

  data->FromInterleaved<media::SignedInt16SampleTypeTraits>(
      buffer->data.data(), buffer->frame_count);

  scoped_refptr<AudioChunk> chunk(new AudioChunk(
      buffer->channel_count * buffer->frame_count * kNumBitsPerAudioSample / 8,
      kNumBitsPerAudioSample / 8));
  data->ToInterleaved<media::SignedInt16SampleTypeTraits>(
      data->frames(), reinterpret_cast<int16_t*>(chunk->writable_data()));
  FSMEventArgs event_args(EVENT_AUDIO_DATA);
  event_args.audio_chunk = std::move(chunk);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(), event_args));
}

void SpeechRecognizerImpl::OnSpeechRecognitionEngineResults(
    const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results) {
  FSMEventArgs event_args(EVENT_ENGINE_RESULT);
  event_args.engine_results = mojo::Clone(results);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(), event_args));
}

void SpeechRecognizerImpl::OnSpeechRecognitionEngineEndOfUtterance() {
  DCHECK(!end_of_utterance_);
  end_of_utterance_ = true;
}

void SpeechRecognizerImpl::OnSpeechRecognitionEngineError(
    const media::mojom::SpeechRecognitionError& error) {
  FSMEventArgs event_args(EVENT_ENGINE_ERROR);
  event_args.engine_error = error;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(), event_args));
}

// -----------------------  Core FSM implementation ---------------------------
// TODO(primiano): After the changes in the media package (r129173), this class
// slightly violates the SpeechRecognitionEventListener interface contract. In
// particular, it is not true anymore that this class can be freed after the
// OnRecognitionEnd event, since the audio_capturer_source_->Stop() asynchronous
// call can be still in progress after the end event. Currently, it does not
// represent a problem for the browser itself, since refcounting protects us
// against such race conditions. However, we should fix this in the next CLs.

// ----------- Contract for all the FSM evolution functions below -------------
//  - Are guaranteed to be executed in the IO thread;
//  - Are guaranteed to be not reentrant (themselves and each other);
//  - event_args members are guaranteed to be stable during the call;
//  - The class won't be freed in the meanwhile due to callbacks;

// TODO(primiano): the audio pipeline is currently serial. However, the
// clipper->endpointer->vumeter chain and the sr_engine could be parallelized.
// We should profile the execution to see if it would be worth or not.
void SpeechRecognizerImpl::DispatchEvent(const FSMEventArgs& event_args) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_LE(event_args.event, EVENT_MAX_VALUE);
  DCHECK_LE(state_, STATE_MAX_VALUE);

  // Event dispatching must be sequential, otherwise it will break all the rules
  // and the assumptions of the finite state automata model.
  DCHECK(!is_dispatching_event_);
  is_dispatching_event_ = true;

  // Guard against the delegate freeing us until we finish processing the event.
  scoped_refptr<SpeechRecognizerImpl> me(this);

  if (event_args.event == EVENT_AUDIO_DATA) {
    DCHECK(event_args.audio_chunk.get() != nullptr);
    ProcessAudioPipeline(event_args);
  }

  // The audio pipeline must be processed before the event dispatch, otherwise
  // it would take actions according to the future state instead of the current.
  state_ = ExecuteTransitionAndGetNextState(event_args);
  is_dispatching_event_ = false;
}

void SpeechRecognizerImpl::ProcessAudioPipeline(
    const FSMEventArgs& event_args) {
  DCHECK(event_args.audio_chunk.get() != nullptr);
  const AudioChunk& raw_audio = *event_args.audio_chunk.get();
  const bool route_to_endpointer = state_ >= STATE_ESTIMATING_ENVIRONMENT &&
                                   state_ <= STATE_RECOGNIZING;
  const bool route_to_sr_engine = route_to_endpointer;
  const bool route_to_vumeter = state_ >= STATE_WAITING_FOR_SPEECH &&
                                state_ <= STATE_RECOGNIZING;
  const bool clip_detected = DetectClipping(raw_audio);
  float rms = 0.0f;

  num_samples_recorded_ += raw_audio.NumSamples();

  if (route_to_endpointer)
    endpointer_.ProcessAudio(raw_audio, &rms);

  if (route_to_vumeter) {
    DCHECK(route_to_endpointer);  // Depends on endpointer due to |rms|.
    UpdateSignalAndNoiseLevels(rms, clip_detected);
  }
  if (route_to_sr_engine) {
    DCHECK(recognition_engine_.get() != nullptr);
    recognition_engine_->TakeAudioChunk(raw_audio);
  }
}

void SpeechRecognizerImpl::OnAudioParametersReceived(
    const std::optional<media::AudioParameters>& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  audio_parameters_ = params.value_or(AudioParameters());
  DVLOG(1) << "Audio parameters: " << audio_parameters_.AsHumanReadableString();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                weak_ptr_factory_.GetWeakPtr(),
                                FSMEventArgs(EVENT_START)));
}

SpeechRecognizerImpl::FSMState SpeechRecognizerImpl::PrepareRecognition(
    const FSMEventArgs&) {
  DCHECK(state_ == STATE_IDLE);
  DCHECK(recognition_engine_.get() != nullptr);
  DCHECK(!IsCapturingAudio());
  if (!use_audio_capturer_source_) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&SpeechRecognizerImpl::DispatchEvent,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  FSMEventArgs(EVENT_START)));
  } else {
    GetAudioSystem()->GetInputStreamParameters(
        device_id_,
        base::BindOnce(&SpeechRecognizerImpl::OnAudioParametersReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  listener()->OnRecognitionStart(session_id());
  return STATE_PREPARING;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::StartRecording(const FSMEventArgs&) {
  DCHECK(state_ == STATE_PREPARING);
  DCHECK(recognition_engine_.get() != nullptr);
  DCHECK(!IsCapturingAudio());

  DVLOG(1) << "SpeechRecognizerImpl starting audio capture.";
  num_samples_recorded_ = 0;
  audio_level_ = 0;
  end_of_utterance_ = false;

  int chunk_duration_ms = recognition_engine_->GetDesiredAudioChunkDurationMs();

  if (!audio_parameters_.IsValid()) {
    DLOG(WARNING) << "Audio input device not found, but one should exist -- "
                     "using fake audio input parameters.";

    // It's okay to try with fake parameters since we've already been given
    // permission from SpeechRecognitionManagerImpl. If no device exists, this
    // will just result in an OnCaptureError().
    audio_parameters_ = media::AudioParameters::UnavailableDeviceParams();
  }

  // Audio converter shall provide audio based on these parameters as output.
  // Hard coded, WebSpeech specific parameters are utilized here.
  int frames_per_buffer = (kAudioSampleRate * chunk_duration_ms) / 1000;
  AudioParameters output_parameters =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      media::ChannelLayoutConfig::FromLayout<kChannelLayout>(),
                      sample_rate_, frames_per_buffer);
  DVLOG(1) << "SRI::output_parameters: "
           << output_parameters.AsHumanReadableString();

  // Audio converter will receive audio based on these parameters as input.
  // On Windows we start by verifying that Core Audio is supported. If not,
  // the WaveIn API is used and we might as well avoid all audio conversations
  // since WaveIn does the conversion for us.
  // TODO(henrika): this code should be moved to platform dependent audio
  // managers.
  bool use_native_audio_params = true;
#if BUILDFLAG(IS_WIN)
  use_native_audio_params = media::CoreAudioUtil::IsSupported();
  DVLOG_IF(1, !use_native_audio_params) << "Reverting to WaveIn for WebSpeech";
#endif

  AudioParameters input_parameters = output_parameters;

  // AUDIO_FAKE means we are running a test.
  if (use_native_audio_params &&
      audio_parameters_.format() != media::AudioParameters::AUDIO_FAKE) {
    // Use native audio parameters but avoid opening up at the native buffer
    // size. Instead use same frame size (in milliseconds) as WebSpeech uses.
    // We rely on internal buffers in the audio back-end to fulfill this request
    // and the idea is to simplify the audio conversion since each Convert()
    // call will then render exactly one ProvideInput() call.
    input_parameters = audio_parameters_;
    frames_per_buffer =
        ((input_parameters.sample_rate() * chunk_duration_ms) / 1000.0) + 0.5;
    input_parameters.set_frames_per_buffer(frames_per_buffer);
    DVLOG(1) << "SRI::input_parameters: "
             << input_parameters.AsHumanReadableString();
  }

  // Create an audio converter which converts data between native input format
  // and WebSpeech specific output format.
  audio_converter_ =
      std::make_unique<OnDataConverter>(input_parameters, output_parameters);
  recognition_engine_->SetAudioParameters(output_parameters);

  // The endpointer needs to estimate the environment/background noise before
  // starting to treat the audio as user input. We wait in the state
  // ESTIMATING_ENVIRONMENT until such interval has elapsed before switching
  // to user input mode.
  endpointer_.SetEnvironmentEstimationMode();

  if (use_audio_capturer_source_) {
    CreateAudioCapturerSource();
    GetAudioCapturerSource()->Initialize(input_parameters, this);
    GetAudioCapturerSource()->Start();
  }

  return STATE_STARTING;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::StartRecognitionEngine(const FSMEventArgs& event_args) {
  // This is the first audio packet captured, so the recognition engine is
  // started and the delegate notified about the event.
  DCHECK(recognition_engine_.get() != nullptr);
  recognition_engine_->StartRecognition();
  listener()->OnAudioStart(session_id());

  // This is a little hack, since TakeAudioChunk() is already called by
  // ProcessAudioPipeline(). It is the best tradeoff, unless we allow dropping
  // the first audio chunk captured after opening the audio device.
  recognition_engine_->TakeAudioChunk(*(event_args.audio_chunk.get()));
  return STATE_ESTIMATING_ENVIRONMENT;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::WaitEnvironmentEstimationCompletion(const FSMEventArgs&) {
  DCHECK(endpointer_.IsEstimatingEnvironment());
  if (GetElapsedTimeMs() >= kEndpointerEstimationTimeMs) {
    endpointer_.SetUserInputMode();
    return STATE_WAITING_FOR_SPEECH;
  } else {
    return STATE_ESTIMATING_ENVIRONMENT;
  }
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::DetectUserSpeechOrTimeout(const FSMEventArgs&) {
  if (endpointer_.DidStartReceivingSpeech()) {
    listener()->OnSoundStart(session_id());
    return STATE_RECOGNIZING;
  } else if (GetElapsedTimeMs() >= kNoSpeechTimeoutMs) {
    return Abort(media::mojom::SpeechRecognitionError(
        media::mojom::SpeechRecognitionErrorCode::kNoSpeech,
        media::mojom::SpeechAudioErrorDetails::kNone));
  }
  return STATE_WAITING_FOR_SPEECH;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::DetectEndOfSpeech(const FSMEventArgs& event_args) {
  if (end_of_utterance_ || endpointer_.speech_input_complete())
    return StopCaptureAndWaitForResult(event_args);
  return STATE_RECOGNIZING;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::StopCaptureAndWaitForResult(const FSMEventArgs&) {
  DCHECK(state_ >= STATE_ESTIMATING_ENVIRONMENT && state_ <= STATE_RECOGNIZING);

  DVLOG(1) << "Concluding recognition";
  CloseAudioCapturerSource();
  recognition_engine_->AudioChunksEnded();

  if (state_ > STATE_WAITING_FOR_SPEECH)
    listener()->OnSoundEnd(session_id());

  listener()->OnAudioEnd(session_id());
  return STATE_WAITING_FINAL_RESULT;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::AbortSilently(const FSMEventArgs& event_args) {
  DCHECK_NE(event_args.event, EVENT_AUDIO_ERROR);
  DCHECK_NE(event_args.event, EVENT_ENGINE_ERROR);
  return Abort(media::mojom::SpeechRecognitionError(
      media::mojom::SpeechRecognitionErrorCode::kNone,
      media::mojom::SpeechAudioErrorDetails::kNone));
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::AbortWithError(const FSMEventArgs& event_args) {
  if (event_args.event == EVENT_AUDIO_ERROR) {
    return Abort(media::mojom::SpeechRecognitionError(
        media::mojom::SpeechRecognitionErrorCode::kAudioCapture,
        media::mojom::SpeechAudioErrorDetails::kNone));
  } else if (event_args.event == EVENT_ENGINE_ERROR) {
    return Abort(event_args.engine_error);
  }
  return Abort(media::mojom::SpeechRecognitionError(
      media::mojom::SpeechRecognitionErrorCode::kAborted,
      media::mojom::SpeechAudioErrorDetails::kNone));
}

SpeechRecognizerImpl::FSMState SpeechRecognizerImpl::Abort(
    const media::mojom::SpeechRecognitionError& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsCapturingAudio())
    CloseAudioCapturerSource();

  DVLOG(1) << "SpeechRecognizerImpl canceling recognition. ";

  if (state_ == STATE_PREPARING) {
    // Cancel an outstanding reply from AudioSystem.
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  // The recognition engine is initialized only after STATE_STARTING.
  if (state_ > STATE_STARTING) {
    DCHECK(recognition_engine_.get() != nullptr);
    recognition_engine_->EndRecognition();
  }

  if (state_ > STATE_WAITING_FOR_SPEECH && state_ < STATE_WAITING_FINAL_RESULT)
    listener()->OnSoundEnd(session_id());

  if (state_ > STATE_STARTING && state_ < STATE_WAITING_FINAL_RESULT)
    listener()->OnAudioEnd(session_id());

  if (error.code != media::mojom::SpeechRecognitionErrorCode::kNone) {
    listener()->OnRecognitionError(session_id(), error);
  }

  listener()->OnRecognitionEnd(session_id());

  return STATE_ENDED;
}

SpeechRecognizerImpl::FSMState SpeechRecognizerImpl::ProcessIntermediateResult(
    const FSMEventArgs& event_args) {
  // In continuous recognition, intermediate results can occur even when we are
  // in the ESTIMATING_ENVIRONMENT or WAITING_FOR_SPEECH states (if the
  // recognition engine is "faster" than our endpointer). In these cases we
  // skip the endpointer and fast-forward to the RECOGNIZING state, with respect
  // of the events triggering order.
  if (state_ == STATE_ESTIMATING_ENVIRONMENT) {
    DCHECK(endpointer_.IsEstimatingEnvironment());
    endpointer_.SetUserInputMode();
  } else if (state_ == STATE_WAITING_FOR_SPEECH) {
    listener()->OnSoundStart(session_id());
  } else {
    DCHECK_EQ(STATE_RECOGNIZING, state_);
  }

  listener()->OnRecognitionResults(session_id(), event_args.engine_results);
  return STATE_RECOGNIZING;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::ProcessFinalResult(const FSMEventArgs& event_args) {
  const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results =
      event_args.engine_results;
  auto i = results.begin();
  bool provisional_results_pending = false;
  bool results_are_empty = true;
  for (; i != results.end(); ++i) {
    const media::mojom::WebSpeechRecognitionResultPtr& result = *i;
    if (result->is_provisional) {
      DCHECK(provisional_results_);
      provisional_results_pending = true;
    } else if (results_are_empty) {
      results_are_empty = result->hypotheses.empty();
    }
  }

  if (provisional_results_pending) {
    listener()->OnRecognitionResults(session_id(), results);
    // We don't end the recognition if a provisional result is received in
    // STATE_WAITING_FINAL_RESULT. A definitive result will come next and will
    // end the recognition.
    return state_;
  }

  recognition_engine_->EndRecognition();

  if (!results_are_empty) {
    // We could receive an empty result (which we won't propagate further)
    // in the following (continuous) scenario:
    //  1. The caller start pushing audio and receives some results;
    //  2. A |StopAudioCapture| is issued later;
    //  3. The final audio frames captured in the interval ]1,2] do not lead to
    //     any result (nor any error);
    //  4. The speech recognition engine, therefore, emits an empty result to
    //     notify that the recognition is ended with no error, yet neither any
    //     further result.
    listener()->OnRecognitionResults(session_id(), results);
  }

  listener()->OnRecognitionEnd(session_id());
  return STATE_ENDED;
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::DoNothing(const FSMEventArgs&) const {
  return state_;  // Just keep the current state.
}

SpeechRecognizerImpl::FSMState
SpeechRecognizerImpl::NotFeasible(const FSMEventArgs& event_args) {
  NOTREACHED_IN_MIGRATION()
      << "Unfeasible event " << event_args.event << " in state " << state_;
  return state_;
}

void SpeechRecognizerImpl::CloseAudioCapturerSource() {
  DCHECK(IsCapturingAudio());
  DVLOG(1) << "SpeechRecognizerImpl closing audio capturer source.";

  if (use_audio_capturer_source_) {
    GetAudioCapturerSource()->Stop();
    audio_capturer_source_ = nullptr;
  }
}

int SpeechRecognizerImpl::GetElapsedTimeMs() const {
  return (num_samples_recorded_ * 1000) / sample_rate_;
}

void SpeechRecognizerImpl::UpdateSignalAndNoiseLevels(const float& rms,
                                                  bool clip_detected) {
  // Calculate the input volume to display in the UI, smoothing towards the
  // new level.
  // TODO(primiano): Do we really need all this floating point arith here?
  // Perhaps it might be quite expensive on mobile.
  float level = (rms - kAudioMeterMinDb) /
      (kAudioMeterDbRange / kAudioMeterRangeMaxUnclipped);
  level = std::clamp(level, 0.0f, kAudioMeterRangeMaxUnclipped);
  const float smoothing_factor = (level > audio_level_) ? kUpSmoothingFactor :
                                                          kDownSmoothingFactor;
  audio_level_ += (level - audio_level_) * smoothing_factor;

  float noise_level = (endpointer_.NoiseLevelDb() - kAudioMeterMinDb) /
      (kAudioMeterDbRange / kAudioMeterRangeMaxUnclipped);
  noise_level = std::clamp(noise_level, 0.0f, kAudioMeterRangeMaxUnclipped);

  listener()->OnAudioLevelsChange(
      session_id(), clip_detected ? 1.0f : audio_level_, noise_level);
}

void SpeechRecognizerImpl::SetAudioEnvironmentForTesting(
    media::AudioSystem* audio_system,
    media::AudioCapturerSource* audio_capturer_source) {
  audio_system_for_tests_ = audio_system;
  audio_capturer_source_for_tests_ = audio_capturer_source;
}

media::AudioSystem* SpeechRecognizerImpl::GetAudioSystem() {
  return audio_system_for_tests_ ? audio_system_for_tests_
                                 : audio_system_.get();
}

void SpeechRecognizerImpl::CreateAudioCapturerSource() {
  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
  GetAudioServiceStreamFactoryBinder().Run(
      stream_factory.InitWithNewPipeAndPassReceiver());
  audio_capturer_source_ = audio::CreateInputDevice(
      std::move(stream_factory), device_id_,
      audio::DeadStreamDetection::kEnabled,
      MediaInternals::GetInstance()->CreateMojoAudioLog(
          media::AudioLogFactory::AudioComponent::kAudioInputController,
          0 /* component_id */));
}

media::AudioCapturerSource* SpeechRecognizerImpl::GetAudioCapturerSource() {
  return audio_capturer_source_for_tests_ ? audio_capturer_source_for_tests_
                                          : audio_capturer_source_.get();
}

}  // namespace content
