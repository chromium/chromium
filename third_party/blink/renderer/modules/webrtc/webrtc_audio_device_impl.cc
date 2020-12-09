// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/sample_rates.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_renderer.h"

using media::AudioParameters;
using media::ChannelLayout;

namespace blink {

namespace {

void SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage("WRADI::" + message);
}

}  // namespace

WebRtcAudioDeviceImpl::WebRtcAudioDeviceImpl()
    : audio_processing_id_(base::UnguessableToken::Create()),
      audio_transport_callback_(nullptr),
      output_delay_ms_(0),
      initialized_(false),
      playing_(false),
      recording_(false) {
  SendLogMessage(base::StringPrintf("%s({id=%s})", __func__,
                                    GetAudioProcessingId().ToString().c_str()));
  // This object can be constructed on either the signaling thread or the main
  // thread, so we need to detach these thread checkers here and have them
  // initialize automatically when the first methods are called.
  DETACH_FROM_THREAD(signaling_thread_checker_);
  DETACH_FROM_THREAD(main_thread_checker_);

  DETACH_FROM_THREAD(worker_thread_checker_);
  DETACH_FROM_THREAD(audio_renderer_thread_checker_);
}

WebRtcAudioDeviceImpl::~WebRtcAudioDeviceImpl() {
  SendLogMessage(base::StringPrintf("%s() [id=%s]", __func__,
                                    GetAudioProcessingId().ToString().c_str()));
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(!initialized_) << "Terminate must have been called.";
}

void WebRtcAudioDeviceImpl::RenderData(media::AudioBus* audio_bus,
                                       int sample_rate,
                                       int audio_delay_milliseconds,
                                       base::TimeDelta* current_time) {
  {
    base::AutoLock auto_lock(lock_);
#if DCHECK_IS_ON()
    DCHECK(!renderer_ || renderer_->CurrentThreadIsRenderingThread());
    if (!audio_renderer_thread_checker_.CalledOnValidThread()) {
      for (auto* sink : playout_sinks_)
        sink->OnRenderThreadChanged();
    }
#endif
    if (!playing_ || audio_bus->channels() > 8) {
      // Force silence to AudioBus after stopping playout in case
      // there is lingering audio data in AudioBus or if the audio device has
      // more than eight channels (which is not supported by the channel mixer
      // in WebRTC).
      // See http://crbug.com/986415 for details on why the extra check for
      // number of channels is required.
      audio_bus->Zero();
      return;
    }
    DCHECK(audio_transport_callback_);
    // Store the reported audio delay locally.
    output_delay_ms_ = audio_delay_milliseconds;
  }

  const int frames_per_10_ms = sample_rate / 100;
  DCHECK_EQ(audio_bus->frames(), frames_per_10_ms);
  DCHECK_GE(audio_bus->channels(), 1);
  DCHECK_LE(audio_bus->channels(), 8);

  // Get 10ms audio and copy result to temporary byte buffer.
  render_buffer_.resize(audio_bus->frames() * audio_bus->channels());
  constexpr int kBytesPerSample = 2;
  static_assert(sizeof(render_buffer_[0]) == kBytesPerSample,
                "kBytesPerSample and FromInterleaved expect 2 bytes.");
  int64_t elapsed_time_ms = -1;
  int64_t ntp_time_ms = -1;
  int16_t* audio_data = render_buffer_.data();

  TRACE_EVENT_BEGIN0("audio", "VoE::PullRenderData");
  audio_transport_callback_->PullRenderData(
      kBytesPerSample * 8, sample_rate, audio_bus->channels(), frames_per_10_ms,
      audio_data, &elapsed_time_ms, &ntp_time_ms);
  TRACE_EVENT_END2("audio", "VoE::PullRenderData", "elapsed_time_ms",
                   elapsed_time_ms, "ntp_time_ms", ntp_time_ms);
  if (elapsed_time_ms >= 0)
    *current_time = base::TimeDelta::FromMilliseconds(elapsed_time_ms);

  // De-interleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0 to match the callback format.
  audio_bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
      audio_data, audio_bus->frames());

  // Pass the render data to the playout sinks.
  base::AutoLock auto_lock(lock_);
  for (auto* sink : playout_sinks_)
    sink->OnPlayoutData(audio_bus, sample_rate, audio_delay_milliseconds);
}

void WebRtcAudioDeviceImpl::RemoveAudioRenderer(
    blink::WebRtcAudioRenderer* renderer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(renderer, renderer_.get());
  // Notify the playout sink of the change.
  for (auto* sink : playout_sinks_)
    sink->OnPlayoutDataSourceChanged();

  renderer_ = nullptr;
}

void WebRtcAudioDeviceImpl::AudioRendererThreadStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DETACH_FROM_THREAD(audio_renderer_thread_checker_);
  // Notify the playout sink of the change.
  // Not holding |lock_| because the caller must guarantee that the audio
  // renderer thread is dead, so no race is possible with |playout_sinks_|
  for (auto* sink : TS_UNCHECKED_READ(playout_sinks_))
    sink->OnPlayoutDataSourceChanged();
}

void WebRtcAudioDeviceImpl::SetOutputDeviceForAec(
    const String& output_device_id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  SendLogMessage(base::StringPrintf("%s({output_device_id=%s}) [id=%s]",
                                    __func__, output_device_id.Utf8().c_str(),
                                    GetAudioProcessingId().ToString().c_str()));
  output_device_id_for_aec_ = output_device_id;
  base::AutoLock lock(lock_);
  for (auto* capturer : capturers_) {
    capturer->SetOutputDeviceForAec(output_device_id.Utf8());
  }
}

base::UnguessableToken WebRtcAudioDeviceImpl::GetAudioProcessingId() const {
  return audio_processing_id_;
}

int32_t WebRtcAudioDeviceImpl::RegisterAudioCallback(
    webrtc::AudioTransport* audio_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  SendLogMessage(base::StringPrintf("%s() [id=%s]", __func__,
                                    GetAudioProcessingId().ToString().c_str()));
  base::AutoLock lock(lock_);
  DCHECK_EQ(!audio_transport_callback_, !!audio_callback);
  audio_transport_callback_ = audio_callback;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::Init() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::Init()";
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);

  // We need to return a success to continue the initialization of WebRtc VoE
  // because failure on the capturer_ initialization should not prevent WebRTC
  // from working. See issue http://crbug.com/144421 for details.
  initialized_ = true;

  return 0;
}

int32_t WebRtcAudioDeviceImpl::Terminate() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::Terminate()";
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);

  // Calling Terminate() multiple times in a row is OK.
  if (!initialized_)
    return 0;

  StopRecording();
  StopPlayout();

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!renderer_ || !renderer_->IsStarted())
        << "The shared audio renderer shouldn't be running";
    capturers_.clear();
  }

  initialized_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Initialized() const {
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  return initialized_;
}

int32_t WebRtcAudioDeviceImpl::PlayoutIsAvailable(bool* available) {
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  *available = initialized_;
  return 0;
}

bool WebRtcAudioDeviceImpl::PlayoutIsInitialized() const {
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  return initialized_;
}

int32_t WebRtcAudioDeviceImpl::RecordingIsAvailable(bool* available) {
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  base::AutoLock auto_lock(lock_);
  *available = !capturers_.empty();
  return 0;
}

bool WebRtcAudioDeviceImpl::RecordingIsInitialized() const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::RecordingIsInitialized()";
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  base::AutoLock auto_lock(lock_);
  return !capturers_.empty();
}

int32_t WebRtcAudioDeviceImpl::StartPlayout() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StartPlayout()";
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  base::AutoLock auto_lock(lock_);
  if (!audio_transport_callback_) {
    LOG(ERROR) << "Audio transport is missing";
    return 0;
  }

  // webrtc::VoiceEngine assumes that it is OK to call Start() twice and
  // that the call is ignored the second time.
  playing_ = true;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopPlayout() {
  DVLOG(1) << "WebRtcAudioDeviceImpl::StopPlayout()";
  DCHECK(initialized_);
  // Can be called both from the worker thread (e.g. when called from webrtc)
  // or the signaling thread (e.g. when we call it ourselves internally).
  // The order in this check is important so that we won't incorrectly
  // initialize worker_thread_checker_ on the signaling thread.
#if DCHECK_IS_ON()
  DCHECK(signaling_thread_checker_.CalledOnValidThread() ||
         worker_thread_checker_.CalledOnValidThread());
#endif
  base::AutoLock auto_lock(lock_);
  // webrtc::VoiceEngine assumes that it is OK to call Stop() multiple times.
  playing_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Playing() const {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  base::AutoLock auto_lock(lock_);
  return playing_;
}

int32_t WebRtcAudioDeviceImpl::StartRecording() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(initialized_);
  SendLogMessage(base::StringPrintf("%s() [id=%s]", __func__,
                                    GetAudioProcessingId().ToString().c_str()));
  base::AutoLock auto_lock(lock_);
  if (!audio_transport_callback_) {
    LOG(ERROR) << "Audio transport is missing";
    return -1;
  }

  recording_ = true;

  return 0;
}

int32_t WebRtcAudioDeviceImpl::StopRecording() {
  DCHECK(initialized_);
  // Can be called both from the worker thread (e.g. when called from webrtc)
  // or the signaling thread (e.g. when we call it ourselves internally).
  // The order in this check is important so that we won't incorrectly
  // initialize worker_thread_checker_ on the signaling thread.
#if DCHECK_IS_ON()
  DCHECK(signaling_thread_checker_.CalledOnValidThread() ||
         worker_thread_checker_.CalledOnValidThread());
#endif
  SendLogMessage(base::StringPrintf("%s() [id=%s]", __func__,
                                    GetAudioProcessingId().ToString().c_str()));
  base::AutoLock auto_lock(lock_);
  recording_ = false;
  return 0;
}

bool WebRtcAudioDeviceImpl::Recording() const {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  base::AutoLock auto_lock(lock_);
  return recording_;
}

int32_t WebRtcAudioDeviceImpl::SetMicrophoneVolume(uint32_t volume) {
  DVLOG(1) << "WebRtcAudioDeviceImpl::SetMicrophoneVolume(" << volume << ")";
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  DCHECK(initialized_);

  // Only one microphone is supported at the moment, which is represented by
  // the default capturer.
  base::AutoLock auto_lock(lock_);
  if (capturers_.empty())
    return -1;
  capturers_.back()->SetVolume(volume);
  return 0;
}

// TODO(henrika): sort out calling thread once we start using this API.
int32_t WebRtcAudioDeviceImpl::MicrophoneVolume(uint32_t* volume) const {
  DVLOG(1) << "WebRtcAudioDeviceImpl::MicrophoneVolume()";
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  // We only support one microphone now, which is accessed via the default
  // capturer.
  DCHECK(initialized_);
  base::AutoLock auto_lock(lock_);
  if (capturers_.empty())
    return -1;
  *volume = static_cast<uint32_t>(capturers_.back()->Volume());
  return 0;
}

int32_t WebRtcAudioDeviceImpl::MaxMicrophoneVolume(uint32_t* max_volume) const {
  DCHECK(initialized_);
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  *max_volume = kMaxVolumeLevel;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::MinMicrophoneVolume(uint32_t* min_volume) const {
  DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
  *min_volume = 0;
  return 0;
}

int32_t WebRtcAudioDeviceImpl::PlayoutDelay(uint16_t* delay_ms) const {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  base::AutoLock auto_lock(lock_);
  *delay_ms = static_cast<uint16_t>(output_delay_ms_);
  return 0;
}

bool WebRtcAudioDeviceImpl::SetAudioRenderer(
    blink::WebRtcAudioRenderer* renderer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(renderer);
  SendLogMessage(base::StringPrintf("%s() [id=%s]", __func__,
                                    GetAudioProcessingId().ToString().c_str()));

  // Here we acquire |lock_| in order to protect the internal state.
  {
    base::AutoLock auto_lock(lock_);
    if (renderer_)
      return false;
  }

  // We release |lock_| here because invoking |renderer|->Initialize while
  // holding |lock_| would result in locks taken in the sequence
  // (|this->lock_|,  |renderer->lock_|) while another thread (i.e, the
  // AudioOutputDevice thread) might concurrently invoke a renderer method,
  // which can itself invoke a method from |this|, resulting in locks taken in
  // the sequence (|renderer->lock_|, |this->lock_|) in that thread.
  // This order discrepancy can cause a deadlock (see Issue 433993).
  // However, we do not need to hold |this->lock_| in order to invoke
  // |renderer|->Initialize, since it does not involve any unprotected access to
  // the internal state of |this|.
  if (!renderer->Initialize(this))
    return false;

  // The new audio renderer will create a new audio renderer thread. Detach
  // |audio_renderer_thread_checker_| from the old thread, if any, and let
  // it attach later to the new thread.
  DETACH_FROM_THREAD(audio_renderer_thread_checker_);

  // We acquire |lock_| again and assert our precondition, since we are
  // accessing the internal state again.
  base::AutoLock auto_lock(lock_);
  DCHECK(!renderer_);
  renderer_ = renderer;
  return true;
}

void WebRtcAudioDeviceImpl::AddAudioCapturer(
    ProcessedLocalAudioSource* capturer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  SendLogMessage(base::StringPrintf("%s() [id=%s]", __func__,
                                    GetAudioProcessingId().ToString().c_str()));
  DCHECK(capturer);
  DCHECK(!capturer->device().id.empty());

  base::AutoLock auto_lock(lock_);
  DCHECK(!base::Contains(capturers_, capturer));
  capturers_.push_back(capturer);
  capturer->SetOutputDeviceForAec(output_device_id_for_aec_.Utf8());
}

void WebRtcAudioDeviceImpl::RemoveAudioCapturer(
    ProcessedLocalAudioSource* capturer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  SendLogMessage(base::StringPrintf("%s() [id=%s]", __func__,
                                    GetAudioProcessingId().ToString().c_str()));
  DCHECK(capturer);
  base::AutoLock auto_lock(lock_);
  capturers_.remove(capturer);
}

void WebRtcAudioDeviceImpl::AddPlayoutSink(
    blink::WebRtcPlayoutDataSource::Sink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DVLOG(1) << "WebRtcAudioDeviceImpl::AddPlayoutSink()";
  DCHECK(sink);
  base::AutoLock auto_lock(lock_);
  DCHECK(!base::Contains(playout_sinks_, sink));
  playout_sinks_.push_back(sink);
}

void WebRtcAudioDeviceImpl::RemovePlayoutSink(
    blink::WebRtcPlayoutDataSource::Sink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DVLOG(1) << "WebRtcAudioDeviceImpl::RemovePlayoutSink()";
  DCHECK(sink);
  base::AutoLock auto_lock(lock_);
  playout_sinks_.remove(sink);
}

base::UnguessableToken
WebRtcAudioDeviceImpl::GetAuthorizedDeviceSessionIdForAudioRenderer() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  base::AutoLock lock(lock_);
  // If there is no capturer or there are more than one open capture devices,
  // return false.
  if (capturers_.size() != 1)
    return base::UnguessableToken();

  const blink::MediaStreamDevice& device = capturers_.back()->device();
  // if (device.session_id <= 0 || !device.matched_output_device_id)
  if (device.session_id().is_empty() || !device.matched_output_device_id)
    return base::UnguessableToken();

  return device.session_id();
}

}  // namespace blink
