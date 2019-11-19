// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/webrtc/webrtc_audio_renderer.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "media/base/sample_rates.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_track.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/peer_connection_remote_audio_source.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace WTF {

template <typename T>
struct CrossThreadCopier<rtc::scoped_refptr<T>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = rtc::scoped_refptr<T>;
  static Type Copy(Type pointer) { return pointer; }
};

}  // namespace WTF

namespace blink {

namespace {

// Audio parameters that don't change.
const media::AudioParameters::Format kFormat =
    media::AudioParameters::AUDIO_PCM_LOW_LATENCY;

// Used for UMA histograms.
const int kRenderTimeHistogramMinMicroseconds = 100;
const int kRenderTimeHistogramMaxMicroseconds = 1 * 1000 * 1000;  // 1 second

// This is a simple wrapper class that's handed out to users of a shared
// WebRtcAudioRenderer instance.  This class maintains the per-user 'playing'
// and 'started' states to avoid problems related to incorrect usage which
// might violate the implementation assumptions inside WebRtcAudioRenderer
// (see the play reference count).
class SharedAudioRenderer : public WebMediaStreamAudioRenderer {
 public:
  // Callback definition for a callback that is called when when Play(), Pause()
  // or SetVolume are called (whenever the internal |playing_state_| changes).
  using OnPlayStateChanged =
      base::RepeatingCallback<void(const WebMediaStream&,
                                   WebRtcAudioRenderer::PlayingState*)>;

  // Signals that the PlayingState* is about to become invalid, see comment in
  // OnPlayStateRemoved.
  using OnPlayStateRemoved =
      base::OnceCallback<void(WebRtcAudioRenderer::PlayingState*)>;

  SharedAudioRenderer(
      const scoped_refptr<WebMediaStreamAudioRenderer>& delegate,
      const WebMediaStream& media_stream,
      const OnPlayStateChanged& on_play_state_changed,
      OnPlayStateRemoved on_play_state_removed)
      : delegate_(delegate),
        media_stream_(media_stream),
        started_(false),
        on_play_state_changed_(on_play_state_changed),
        on_play_state_removed_(std::move(on_play_state_removed)) {
    DCHECK(!on_play_state_changed_.is_null());
    DCHECK(!media_stream_.IsNull());
  }

 protected:
  ~SharedAudioRenderer() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DVLOG(1) << __func__;
    Stop();
    std::move(on_play_state_removed_).Run(&playing_state_);
  }

  void Start() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (started_)
      return;
    started_ = true;
    delegate_->Start();
  }

  void Play() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (!started_ || playing_state_.playing())
      return;
    playing_state_.set_playing(true);
    on_play_state_changed_.Run(media_stream_, &playing_state_);
  }

  void Pause() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (!started_ || !playing_state_.playing())
      return;
    playing_state_.set_playing(false);
    on_play_state_changed_.Run(media_stream_, &playing_state_);
  }

  void Stop() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (!started_)
      return;
    Pause();
    started_ = false;
    delegate_->Stop();
  }

  void SetVolume(float volume) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(volume >= 0.0f && volume <= 1.0f);
    playing_state_.set_volume(volume);
    on_play_state_changed_.Run(media_stream_, &playing_state_);
  }

  void SwitchOutputDevice(const std::string& device_id,
                          media::OutputDeviceStatusCB callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return delegate_->SwitchOutputDevice(device_id, std::move(callback));
  }

  base::TimeDelta GetCurrentRenderTime() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return delegate_->GetCurrentRenderTime();
  }

  bool IsLocalRenderer() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return delegate_->IsLocalRenderer();
  }

 private:
  THREAD_CHECKER(thread_checker_);
  const scoped_refptr<WebMediaStreamAudioRenderer> delegate_;
  const WebMediaStream media_stream_;
  bool started_;
  WebRtcAudioRenderer::PlayingState playing_state_;
  OnPlayStateChanged on_play_state_changed_;
  OnPlayStateRemoved on_play_state_removed_;
};

}  // namespace

class WebRtcAudioRenderer::InternalFrame {
 public:
  explicit InternalFrame(WebLocalFrame* web_frame)
      : frame_(web_frame ? static_cast<LocalFrame*>(
                               WebLocalFrame::ToCoreFrame(*web_frame))
                         : nullptr) {}

  LocalFrame* frame() { return frame_.Get(); }
  WebLocalFrame* web_frame() {
    if (!frame_)
      return nullptr;

    return static_cast<WebLocalFrame*>(WebFrame::FromFrame(frame()));
  }

 private:
  WeakPersistent<LocalFrame> frame_;
};

WebRtcAudioRenderer::WebRtcAudioRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& signaling_thread,
    const WebMediaStream& media_stream,
    WebLocalFrame* web_frame,
    const base::UnguessableToken& session_id,
    const std::string& device_id)
    : state_(UNINITIALIZED),
      source_internal_frame_(std::make_unique<InternalFrame>(web_frame)),
      session_id_(session_id),
      signaling_thread_(signaling_thread),
      media_stream_(media_stream),
      source_(nullptr),
      play_ref_count_(0),
      start_ref_count_(0),
      sink_params_(kFormat, media::CHANNEL_LAYOUT_STEREO, 0, 0),
      output_device_id_(device_id) {
  WebRtcLogMessage(base::StringPrintf("WAR::WAR. session_id=%s, effects=%i",
                                      session_id.ToString().c_str(),
                                      sink_params_.effects()));
}

WebRtcAudioRenderer::~WebRtcAudioRenderer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, UNINITIALIZED);
}

bool WebRtcAudioRenderer::Initialize(WebRtcAudioRendererSource* source) {
  DVLOG(1) << "WebRtcAudioRenderer::Initialize()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(source);
  DCHECK(!sink_.get());
  // DCHECK_GE(session_id_, 0);
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(state_, UNINITIALIZED);
    DCHECK(!source_);
  }

  media::AudioSinkParameters sink_params(session_id_, output_device_id_);
  sink_params.processing_id = source->GetAudioProcessingId();
  sink_ = Platform::Current()->NewAudioRendererSink(
      WebAudioDeviceSourceType::kWebRtc, source_internal_frame_->web_frame(),
      sink_params);

  media::OutputDeviceStatus sink_status =
      sink_->GetOutputDeviceInfo().device_status();
  UMA_HISTOGRAM_ENUMERATION("Media.Audio.WebRTCAudioRenderer.DeviceStatus",
                            sink_status, media::OUTPUT_DEVICE_STATUS_MAX + 1);
  if (sink_status != media::OUTPUT_DEVICE_STATUS_OK) {
    sink_->Stop();
    return false;
  }

  PrepareSink();
  {
    // No need to reassert the preconditions because the other thread accessing
    // the fields only reads them.
    base::AutoLock auto_lock(lock_);
    source_ = source;

    // User must call Play() before any audio can be heard.
    state_ = PAUSED;
  }
  source_->SetOutputDeviceForAec(output_device_id_);
  sink_->Start();
  sink_->Play();  // Not all the sinks play on start.

  return true;
}

scoped_refptr<WebMediaStreamAudioRenderer>
WebRtcAudioRenderer::CreateSharedAudioRendererProxy(
    const WebMediaStream& media_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SharedAudioRenderer::OnPlayStateChanged on_play_state_changed =
      WTF::BindRepeating(&WebRtcAudioRenderer::OnPlayStateChanged,
                         WrapRefCounted(this));
  SharedAudioRenderer::OnPlayStateRemoved on_play_state_removed =
      WTF::Bind(&WebRtcAudioRenderer::OnPlayStateRemoved, WrapRefCounted(this));
  return new SharedAudioRenderer(this, media_stream,
                                 std::move(on_play_state_changed),
                                 std::move(on_play_state_removed));
}

bool WebRtcAudioRenderer::IsStarted() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return start_ref_count_ != 0;
}

bool WebRtcAudioRenderer::CurrentThreadIsRenderingThread() {
  return sink_->CurrentThreadIsRenderingThread();
}

void WebRtcAudioRenderer::Start() {
  DVLOG(1) << "WebRtcAudioRenderer::Start()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++start_ref_count_;
}

void WebRtcAudioRenderer::Play() {
  DVLOG(1) << "WebRtcAudioRenderer::Play()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (playing_state_.playing())
    return;

  playing_state_.set_playing(true);

  OnPlayStateChanged(media_stream_, &playing_state_);
}

void WebRtcAudioRenderer::EnterPlayState() {
  DVLOG(1) << "WebRtcAudioRenderer::EnterPlayState()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(start_ref_count_, 0) << "Did you forget to call Start()?";
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK(play_ref_count_ == 0 || state_ == PLAYING);
  ++play_ref_count_;

  if (state_ != PLAYING) {
    state_ = PLAYING;

    if (audio_fifo_) {
      audio_delay_ = base::TimeDelta();
      audio_fifo_->Clear();
    }
  }
}

void WebRtcAudioRenderer::Pause() {
  DVLOG(1) << "WebRtcAudioRenderer::Pause()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!playing_state_.playing())
    return;

  playing_state_.set_playing(false);

  OnPlayStateChanged(media_stream_, &playing_state_);
}

void WebRtcAudioRenderer::EnterPauseState() {
  DVLOG(1) << "WebRtcAudioRenderer::EnterPauseState()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(start_ref_count_, 0) << "Did you forget to call Start()?";
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK_EQ(state_, PLAYING);
  DCHECK_GT(play_ref_count_, 0);
  if (!--play_ref_count_)
    state_ = PAUSED;
}

void WebRtcAudioRenderer::Stop() {
  DVLOG(1) << "WebRtcAudioRenderer::Stop()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    base::AutoLock auto_lock(lock_);
    if (state_ == UNINITIALIZED)
      return;

    if (--start_ref_count_)
      return;

    DVLOG(1) << "Calling RemoveAudioRenderer and Stop().";

    source_->RemoveAudioRenderer(this);
    source_ = nullptr;
    state_ = UNINITIALIZED;
  }

  // Apart from here, |max_render_time_| is only accessed in SourceCallback(),
  // which is guaranteed to not run after |source_| has been set to null, and
  // not before this function has returned.
  // If |max_render_time_| is zero, no render call has been made.
  if (!max_render_time_.is_zero()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Media.Audio.Render.GetSourceDataTimeMax.WebRTC",
        static_cast<int>(max_render_time_.InMicroseconds()),
        kRenderTimeHistogramMinMicroseconds,
        kRenderTimeHistogramMaxMicroseconds, 50);
    max_render_time_ = base::TimeDelta();
  }

  // Make sure to stop the sink while _not_ holding the lock since the Render()
  // callback may currently be executing and trying to grab the lock while we're
  // stopping the thread on which it runs.
  sink_->Stop();
}

void WebRtcAudioRenderer::SetVolume(float volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(volume >= 0.0f && volume <= 1.0f);

  playing_state_.set_volume(volume);
  OnPlayStateChanged(media_stream_, &playing_state_);
}

base::TimeDelta WebRtcAudioRenderer::GetCurrentRenderTime() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock auto_lock(lock_);
  return current_time_;
}

bool WebRtcAudioRenderer::IsLocalRenderer() {
  return false;
}

void WebRtcAudioRenderer::SwitchOutputDevice(
    const std::string& device_id,
    media::OutputDeviceStatusCB callback) {
  DVLOG(1) << "WebRtcAudioRenderer::SwitchOutputDevice()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!source_) {
    std::move(callback).Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, UNINITIALIZED);
  }

  media::AudioSinkParameters sink_params(session_id_, device_id);
  sink_params.processing_id = source_->GetAudioProcessingId();
  scoped_refptr<media::AudioRendererSink> new_sink =
      Platform::Current()->NewAudioRendererSink(
          WebAudioDeviceSourceType::kWebRtc,
          source_internal_frame_->web_frame(), sink_params);
  media::OutputDeviceStatus status =
      new_sink->GetOutputDeviceInfo().device_status();
  UMA_HISTOGRAM_ENUMERATION(
      "Media.Audio.WebRTCAudioRenderer.SwitchDeviceStatus", status,
      media::OUTPUT_DEVICE_STATUS_MAX + 1);

  if (status != media::OUTPUT_DEVICE_STATUS_OK) {
    new_sink->Stop();
    std::move(callback).Run(status);
    return;
  }

  // Make sure to stop the sink while _not_ holding the lock since the Render()
  // callback may currently be executing and trying to grab the lock while we're
  // stopping the thread on which it runs.
  sink_->Stop();
  sink_ = new_sink;
  output_device_id_ = device_id;
  {
    base::AutoLock auto_lock(lock_);
    source_->AudioRendererThreadStopped();
  }
  source_->SetOutputDeviceForAec(output_device_id_);
  PrepareSink();
  sink_->Start();
  sink_->Play();  // Not all the sinks play on start.

  std::move(callback).Run(media::OUTPUT_DEVICE_STATUS_OK);
}

int WebRtcAudioRenderer::Render(base::TimeDelta delay,
                                base::TimeTicks delay_timestamp,
                                int prior_frames_skipped,
                                media::AudioBus* audio_bus) {
  DCHECK(sink_->CurrentThreadIsRenderingThread());
  DCHECK_LE(sink_params_.channels(), 8);
  base::AutoLock auto_lock(lock_);
  if (!source_)
    return 0;

  DVLOG(2) << "WebRtcAudioRenderer::Render()";
  DVLOG(2) << "audio_delay: " << delay;

  audio_delay_ = delay;

  // If there are skipped frames, pull and throw away the same amount. We always
  // pull 10 ms of data from the source (see PrepareSink()), so the fifo is only
  // required if the number of frames to drop doesn't correspond to 10 ms.
  if (prior_frames_skipped > 0) {
    const int source_frames_per_buffer = sink_params_.sample_rate() / 100;
    if (!audio_fifo_ && prior_frames_skipped != source_frames_per_buffer) {
      audio_fifo_ = std::make_unique<media::AudioPullFifo>(
          sink_params_.channels(), source_frames_per_buffer,
          ConvertToBaseCallback(
              CrossThreadBindRepeating(&WebRtcAudioRenderer::SourceCallback,
                                       CrossThreadUnretained(this))));
    }

    std::unique_ptr<media::AudioBus> drop_bus =
        media::AudioBus::Create(audio_bus->channels(), prior_frames_skipped);
    if (audio_fifo_)
      audio_fifo_->Consume(drop_bus.get(), drop_bus->frames());
    else
      SourceCallback(0, drop_bus.get());
  }

  // Pull the data we will deliver.
  if (audio_fifo_)
    audio_fifo_->Consume(audio_bus, audio_bus->frames());
  else
    SourceCallback(0, audio_bus);

  return (state_ == PLAYING) ? audio_bus->frames() : 0;
}

void WebRtcAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
  LOG(ERROR) << "OnRenderError()";
}

// Called by AudioPullFifo when more data is necessary.
void WebRtcAudioRenderer::SourceCallback(int fifo_frame_delay,
                                         media::AudioBus* audio_bus) {
  DCHECK(sink_->CurrentThreadIsRenderingThread());
  base::TimeTicks start_time = base::TimeTicks::Now();
  DVLOG(2) << "WebRtcAudioRenderer::SourceCallback(" << fifo_frame_delay << ", "
           << audio_bus->channels() << ", " << audio_bus->frames() << ")";

  int output_delay_milliseconds =
      static_cast<int>(audio_delay_.InMilliseconds());
  // TODO(grunell): This integer division by sample_rate will cause loss of
  // partial milliseconds, and may cause avsync drift. http://crbug.com/586540
  output_delay_milliseconds += fifo_frame_delay *
                               base::Time::kMillisecondsPerSecond /
                               sink_params_.sample_rate();
  DVLOG(2) << "output_delay_milliseconds: " << output_delay_milliseconds;

  // We need to keep render data for the |source_| regardless of |state_|,
  // otherwise the data will be buffered up inside |source_|.
  source_->RenderData(audio_bus, sink_params_.sample_rate(),
                      output_delay_milliseconds, &current_time_);

  // Avoid filling up the audio bus if we are not playing; instead
  // return here and ensure that the returned value in Render() is 0.
  if (state_ != PLAYING)
    audio_bus->Zero();

  // Measure the elapsed time for this function and log it to UMA. Store the max
  // value. Don't do this for low resolution clocks to not skew data.
  if (base::TimeTicks::IsHighResolution()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.Audio.Render.GetSourceDataTime.WebRTC",
                                static_cast<int>(elapsed.InMicroseconds()),
                                kRenderTimeHistogramMinMicroseconds,
                                kRenderTimeHistogramMaxMicroseconds, 50);

    if (elapsed > max_render_time_)
      max_render_time_ = elapsed;
  }
}

void WebRtcAudioRenderer::UpdateSourceVolume(
    webrtc::AudioSourceInterface* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Note: If there are no playing audio renderers, then the volume will be
  // set to 0.0.
  float volume = 0.0f;

  auto entry = source_playing_states_.find(source);
  if (entry != source_playing_states_.end()) {
    PlayingStates& states = entry->second;
    for (PlayingStates::const_iterator it = states.begin(); it != states.end();
         ++it) {
      if ((*it)->playing())
        volume += (*it)->volume();
    }
  }

  // The valid range for volume scaling of a remote webrtc source is
  // 0.0-10.0 where 1.0 is no attenuation/boost.
  DCHECK(volume >= 0.0f);
  if (volume > 10.0f)
    volume = 10.0f;

  DVLOG(1) << "Setting remote source volume: " << volume;
  if (!signaling_thread_->BelongsToCurrentThread()) {
    // Libjingle hands out proxy objects in most cases, but the audio source
    // object is an exception (bug?).  So, to work around that, we need to make
    // sure we call SetVolume on the signaling thread.
    PostCrossThreadTask(
        *signaling_thread_, FROM_HERE,
        CrossThreadBindOnce(
            &webrtc::AudioSourceInterface::SetVolume,
            rtc::scoped_refptr<webrtc::AudioSourceInterface>(source), volume));
  } else {
    source->SetVolume(volume);
  }
}

bool WebRtcAudioRenderer::AddPlayingState(webrtc::AudioSourceInterface* source,
                                          PlayingState* state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state->playing());
  // Look up or add the |source| to the map.
  PlayingStates& array = source_playing_states_[source];
  if (base::Contains(array, state))
    return false;

  array.push_back(state);

  return true;
}

bool WebRtcAudioRenderer::RemovePlayingState(
    webrtc::AudioSourceInterface* source,
    PlayingState* state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!state->playing());
  auto found = source_playing_states_.find(source);
  if (found == source_playing_states_.end())
    return false;

  PlayingStates& array = found->second;
  auto state_it = std::find(array.begin(), array.end(), state);
  if (state_it == array.end())
    return false;

  array.erase(state_it);

  if (array.empty())
    source_playing_states_.erase(found);

  return true;
}

void WebRtcAudioRenderer::OnPlayStateChanged(const WebMediaStream& media_stream,
                                             PlayingState* state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  WebVector<WebMediaStreamTrack> web_tracks = media_stream.AudioTracks();

  for (const WebMediaStreamTrack& web_track : web_tracks) {
    // WebRtcAudioRenderer can only render audio tracks received from a remote
    // peer. Since the actual MediaStream is mutable from JavaScript, we need
    // to make sure |web_track| is actually a remote track.
    PeerConnectionRemoteAudioTrack* const remote_track =
        PeerConnectionRemoteAudioTrack::From(
            MediaStreamAudioTrack::From(web_track));
    if (!remote_track)
      continue;
    webrtc::AudioSourceInterface* source =
        remote_track->track_interface()->GetSource();
    DCHECK(source);
    if (!state->playing()) {
      if (RemovePlayingState(source, state))
        EnterPauseState();
    } else if (AddPlayingState(source, state)) {
      EnterPlayState();
    }
    UpdateSourceVolume(source);
  }
}

void WebRtcAudioRenderer::OnPlayStateRemoved(PlayingState* state) {
  // It is possible we associated |state| to a source for a track that is no
  // longer easily reachable. We iterate over |source_playing_states_| to
  // ensure there are no dangling pointers to |state| there. See
  // crbug.com/697256.
  // TODO(maxmorin): Clean up cleanup code in this and related classes so that
  // this hack isn't necessary.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto it = source_playing_states_.begin();
       it != source_playing_states_.end();) {
    PlayingStates& states = it->second;
    // We cannot use RemovePlayingState as it might invalidate |it|.
    base::Erase(states, state);
    if (states.empty())
      it = source_playing_states_.erase(it);
    else
      ++it;
  }
}

void WebRtcAudioRenderer::PrepareSink() {
  DVLOG(1) << "WebRtcAudioRenderer::PrepareSink()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  media::AudioParameters new_sink_params;
  {
    base::AutoLock lock(lock_);
    new_sink_params = sink_params_;
  }

  const media::OutputDeviceInfo& device_info = sink_->GetOutputDeviceInfo();
  DCHECK_EQ(device_info.device_status(), media::OUTPUT_DEVICE_STATUS_OK);
  DVLOG(1) << "Sink parameters: " << sink_params_.AsHumanReadableString();
  DVLOG(1) << "Hardware parameters: "
           << device_info.output_params().AsHumanReadableString();

  // WebRTC does not yet support higher rates than 96000 on the client side
  // and 48000 is the preferred sample rate. Therefore, if 192000 is detected,
  // we change the rate to 48000 instead. The consequence is that the native
  // layer will be opened up at 192kHz but WebRTC will provide data at 48kHz
  // which will then be resampled by the audio converted on the browser side
  // to match the native audio layer.
  int sample_rate = device_info.output_params().sample_rate();
  DVLOG(1) << "Audio output hardware sample rate: " << sample_rate;
  if (sample_rate >= 192000) {
    DVLOG(1) << "Resampling from 48000 to " << sample_rate << " is required";
    sample_rate = 48000;
  }
  media::AudioSampleRate asr;
  if (media::ToAudioSampleRate(sample_rate, &asr)) {
    UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputSampleRate", asr,
                              media::kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS_1M("WebRTC.AudioOutputSampleRateUnexpected",
                            sample_rate);
  }

  // Calculate the frames per buffer for the source, i.e. the WebRTC client. We
  // use 10 ms of data since the WebRTC client only supports multiples of 10 ms
  // as buffer size where 10 ms is preferred for lowest possible delay.
  const int source_frames_per_buffer = (sample_rate / 100);
  DVLOG(1) << "Using WebRTC output buffer size: " << source_frames_per_buffer;

  // Setup sink parameters using same channel configuration as the source.
  // This sink is an AudioRendererSink which is implemented by an
  // AudioOutputDevice. Note that we used to use hard-coded settings for
  // stereo here but this has been changed since crbug.com/982276.
  constexpr int kMaxChannels = 8;
  int channels = device_info.output_params().channels();
  media::ChannelLayout channel_layout =
      device_info.output_params().channel_layout();
  if (channels > kMaxChannels) {
    // WebRTC does not support channel remixing for more than 8 channels (7.1).
    // This is an attempt to "support" more than 8 channels by falling back to
    // stereo instead. See crbug.com/1003735.
    LOG(WARNING) << "Falling back to stereo sink";
    channels = 2;
    channel_layout = media::CHANNEL_LAYOUT_STEREO;
  }
  const int sink_frames_per_buffer = media::AudioLatency::GetRtcBufferSize(
      sample_rate, device_info.output_params().frames_per_buffer());
  new_sink_params.Reset(kFormat, channel_layout, sample_rate,
                        sink_frames_per_buffer);
  if (channel_layout == media::CHANNEL_LAYOUT_DISCRETE) {
    new_sink_params.set_channels_for_discrete(channels);
  }
  DVLOG(1) << new_sink_params.AsHumanReadableString();
  DCHECK(new_sink_params.IsValid());

  // Create a FIFO if re-buffering is required to match the source input with
  // the sink request. The source acts as provider here and the sink as
  // consumer.
  const bool different_source_sink_frames =
      source_frames_per_buffer != new_sink_params.frames_per_buffer();
  if (different_source_sink_frames) {
    DVLOG(1) << "Rebuffering from " << source_frames_per_buffer << " to "
             << new_sink_params.frames_per_buffer();
  }
  {
    base::AutoLock lock(lock_);
    if ((!audio_fifo_ && different_source_sink_frames) ||
        (audio_fifo_ &&
         audio_fifo_->SizeInFrames() != source_frames_per_buffer)) {
      audio_fifo_ = std::make_unique<media::AudioPullFifo>(
          channels, source_frames_per_buffer,
          ConvertToBaseCallback(
              CrossThreadBindRepeating(&WebRtcAudioRenderer::SourceCallback,
                                       CrossThreadUnretained(this))));
    }
    sink_params_ = new_sink_params;
    DVLOG(1) << "New sink parameters: " << sink_params_.AsHumanReadableString();
  }

  // Specify the latency info to be passed to the browser side.
  new_sink_params.set_latency_tag(
      Platform::Current()->GetAudioSourceLatencyType(
          WebAudioDeviceSourceType::kWebRtc));
  sink_->Initialize(new_sink_params, this);
}

}  // namespace blink
