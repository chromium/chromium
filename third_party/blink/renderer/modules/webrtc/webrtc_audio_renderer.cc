// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_renderer.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/sample_rates.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_renderer.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/webrtc/peer_connection_remote_audio_source.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
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

// Time constant for AudioPowerMonitor. See See AudioPowerMonitor ctor comments
// for details.
constexpr base::TimeDelta kPowerMeasurementTimeConstant =
    base::Milliseconds(10);

// Time in seconds between two successive measurements of audio power levels.
constexpr base::TimeDelta kPowerMonitorLogInterval = base::Seconds(15);

// Used for UMA histograms.
const int kRenderTimeHistogramMinMicroseconds = 100;
const int kRenderTimeHistogramMaxMicroseconds = 1 * 1000 * 1000;  // 1 second

const char* OutputDeviceStatusToString(media::OutputDeviceStatus status) {
  switch (status) {
    case media::OUTPUT_DEVICE_STATUS_OK:
      return "OK";
    case media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND:
      return "ERROR_NOT_FOUND";
    case media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED:
      return "ERROR_NOT_AUTHORIZED";
    case media::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT:
      return "ERROR_TIMED_OUT";
    case media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL:
      return "ERROR_INTERNAL";
  }
}

const char* StateToString(WebRtcAudioRenderer::State state) {
  switch (state) {
    case WebRtcAudioRenderer::kUninitialized:
      return "UNINITIALIZED";
    case WebRtcAudioRenderer::kPlaying:
      return "PLAYING";
    case WebRtcAudioRenderer::kPaused:
      return "PAUSED";
  }
}

// This is a simple wrapper class that's handed out to users of a shared
// WebRtcAudioRenderer instance.  This class maintains the per-user 'playing'
// and 'started' states to avoid problems related to incorrect usage which
// might violate the implementation assumptions inside WebRtcAudioRenderer
// (see the play reference count).
class SharedAudioRenderer : public MediaStreamAudioRenderer {
 public:
  // Callback definition for a callback that is called when when Play(), Pause()
  // or SetVolume are called (whenever the internal |playing_state_| changes).
  using OnPlayStateChanged =
      base::RepeatingCallback<void(MediaStreamDescriptor*,
                                   WebRtcAudioRenderer::PlayingState*)>;

  // Signals that the PlayingState* is about to become invalid, see comment in
  // OnPlayStateRemoved.
  using OnPlayStateRemoved =
      base::OnceCallback<void(WebRtcAudioRenderer::PlayingState*)>;

  SharedAudioRenderer(const scoped_refptr<MediaStreamAudioRenderer>& delegate,
                      MediaStreamDescriptor* media_stream_descriptor,
                      const OnPlayStateChanged& on_play_state_changed,
                      OnPlayStateRemoved on_play_state_removed)
      : delegate_(delegate),
        media_stream_descriptor_(media_stream_descriptor),
        started_(false),
        on_play_state_changed_(on_play_state_changed),
        on_play_state_removed_(std::move(on_play_state_removed)) {
    DCHECK(!on_play_state_changed_.is_null());
    DCHECK(media_stream_descriptor_);
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
    on_play_state_changed_.Run(media_stream_descriptor_, &playing_state_);
  }

  void Pause() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (!started_ || !playing_state_.playing())
      return;
    playing_state_.set_playing(false);
    on_play_state_changed_.Run(media_stream_descriptor_, &playing_state_);
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
    on_play_state_changed_.Run(media_stream_descriptor_, &playing_state_);
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

 private:
  THREAD_CHECKER(thread_checker_);
  const scoped_refptr<MediaStreamAudioRenderer> delegate_;
  Persistent<MediaStreamDescriptor> media_stream_descriptor_;
  bool started_;
  WebRtcAudioRenderer::PlayingState playing_state_;
  OnPlayStateChanged on_play_state_changed_;
  OnPlayStateRemoved on_play_state_removed_;
};

}  // namespace

WebRtcAudioRenderer::AudioStreamTracker::AudioStreamTracker(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    WebRtcAudioRenderer* renderer,
    int sample_rate)
    : task_runner_(std::move(task_runner)),
      renderer_(renderer),
      start_time_(base::TimeTicks::Now()),
      render_callbacks_started_(false),
      check_alive_timer_(task_runner_,
                         this,
                         &WebRtcAudioRenderer::AudioStreamTracker::CheckAlive),
      power_monitor_(sample_rate, kPowerMeasurementTimeConstant),
      last_audio_level_log_time_(base::TimeTicks::Now()) {
  weak_this_ = weak_factory_.GetWeakPtr();
  // CheckAlive() will look to see if |render_callbacks_started_| is true
  // after the timeout expires and log this. If the stream is paused/closed
  // before the timer fires, a warning is logged instead.
  check_alive_timer_.StartOneShot(base::Seconds(5), FROM_HERE);
}

WebRtcAudioRenderer::AudioStreamTracker::~AudioStreamTracker() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(renderer_);
  const auto duration = base::TimeTicks::Now() - start_time_;
  renderer_->SendLogMessage(
      String::Format("%s => (media stream duration=%" PRId64 " seconds)",
                     __func__, duration.InSeconds()));
}

void WebRtcAudioRenderer::AudioStreamTracker::OnRenderCallbackCalled() {
  DCHECK(renderer_->CurrentThreadIsRenderingThread());
  // Indicate that render callbacks has started as expected and within a
  // reasonable time. Since this thread is the only writer of
  // |render_callbacks_started_| once the thread starts, it's safe to compare
  // and then change the state once.
  if (!render_callbacks_started_)
    render_callbacks_started_ = true;
}

void WebRtcAudioRenderer::AudioStreamTracker::MeasurePower(
    const media::AudioBus& buffer,
    int frames) {
  DCHECK(renderer_->CurrentThreadIsRenderingThread());
  // Update the average power estimate on the rendering thread to avoid posting
  // a task which also has to copy the audio bus. According to comments in
  // AudioPowerMonitor::Scan(), it should be safe. Note that, we only check the
  // power once every ten seconds (on the |task_runner_| thread) and the result
  // is only used for logging purposes.
  power_monitor_.Scan(buffer, frames);
  const auto now = base::TimeTicks::Now();
  if ((now - last_audio_level_log_time_) > kPowerMonitorLogInterval) {
    // Log the current audio level but avoid using the render thread to reduce
    // its load and to ensure that |power_monitor_| is mainly accessed on one
    // thread. |weak_ptr_factory_| ensures that the task is canceled when
    // |this| is destroyed since we can't guarantee that |this| outlives the
    // task.
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(&AudioStreamTracker::LogAudioPowerLevel,
                            weak_this_));
    last_audio_level_log_time_ = now;
  }
}

void WebRtcAudioRenderer::AudioStreamTracker::LogAudioPowerLevel() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  std::pair<float, bool> power_and_clip =
      power_monitor_.ReadCurrentPowerAndClip();
  renderer_->SendLogMessage(String::Format(
      "%s => (average audio level=%.2f dBFS)", __func__, power_and_clip.first));
}

void WebRtcAudioRenderer::AudioStreamTracker::CheckAlive(TimerBase*) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(renderer_);
  renderer_->SendLogMessage(String::Format(
      "%s => (%s)", __func__,
      render_callbacks_started_ ? "stream is alive"
                                : "WARNING: stream is not alive"));
}

WebRtcAudioRenderer::WebRtcAudioRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& signaling_thread,
    MediaStreamDescriptor* media_stream_descriptor,
    WebLocalFrame& web_frame,
    const base::UnguessableToken& session_id,
    const String& device_id,
    base::RepeatingCallback<void()> on_render_error_callback)
    : task_runner_(web_frame.GetTaskRunner(TaskType::kInternalMediaRealTime)),
      state_(kUninitialized),
      source_frame_(To<LocalFrame>(WebFrame::ToCoreFrame(web_frame))),
      session_id_(session_id),
      signaling_thread_(signaling_thread),
      media_stream_descriptor_(media_stream_descriptor),
      media_stream_descriptor_id_(media_stream_descriptor_->Id()),
      source_(nullptr),
      play_ref_count_(0),
      start_ref_count_(0),
      sink_params_(kFormat, media::ChannelLayoutConfig::Stereo(), 0, 0),
      output_device_id_(device_id),
      on_render_error_callback_(std::move(on_render_error_callback)) {
  if (web_frame.Client()) {
    speech_recognition_client_ =
        web_frame.Client()->CreateSpeechRecognitionClient();
  }

  SendLogMessage(
      String::Format("%s({session_id=%s}, {device_id=%s})", __func__,
                     session_id.is_empty() ? "" : session_id.ToString().c_str(),
                     device_id.Utf8().c_str()));
}

WebRtcAudioRenderer::~WebRtcAudioRenderer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, kUninitialized);
}

bool WebRtcAudioRenderer::Initialize(WebRtcAudioRendererSource* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(source);
  DCHECK(!sink_.get());
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(state_, kUninitialized);
    DCHECK(!source_);
  }
  SendLogMessage(
      String::Format("%s([state=%s])", __func__, StateToString(state_)));

  media::AudioSinkParameters sink_params(session_id_, output_device_id_.Utf8());
  sink_ = Platform::Current()->NewAudioRendererSink(
      WebAudioDeviceSourceType::kWebRtc,
      static_cast<WebLocalFrame*>(WebFrame::FromCoreFrame(source_frame_)),
      sink_params);

  media::OutputDeviceStatus sink_status =
      sink_->GetOutputDeviceInfo().device_status();
  UMA_HISTOGRAM_ENUMERATION("Media.Audio.WebRTCAudioRenderer.DeviceStatus",
                            sink_status, media::OUTPUT_DEVICE_STATUS_MAX + 1);
  SendLogMessage(String::Format("%s => (sink device_status=%s)", __func__,
                                OutputDeviceStatusToString(sink_status)));
  if (sink_status != media::OUTPUT_DEVICE_STATUS_OK) {
    SendLogMessage(String::Format("%s => (ERROR: invalid output device status)",
                                  __func__));
    sink_->Stop();
    return false;
  }

  PrepareSink();
  {
    // No need to reassert the preconditions because the other thread
    // accessing the fields only reads them.
    base::AutoLock auto_lock(lock_);
    source_ = source;

    // User must call Play() before any audio can be heard.
    state_ = kPaused;
  }
  source_->SetOutputDeviceForAec(output_device_id_);
  sink_->Start();
  sink_->Play();  // Not all the sinks play on start.

  return true;
}

scoped_refptr<MediaStreamAudioRenderer>
WebRtcAudioRenderer::CreateSharedAudioRendererProxy(
    MediaStreamDescriptor* media_stream_descriptor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SharedAudioRenderer::OnPlayStateChanged on_play_state_changed =
      WTF::BindRepeating(&WebRtcAudioRenderer::OnPlayStateChanged,
                         WrapRefCounted(this));
  SharedAudioRenderer::OnPlayStateRemoved on_play_state_removed = WTF::BindOnce(
      &WebRtcAudioRenderer::OnPlayStateRemoved, WrapRefCounted(this));
  return base::MakeRefCounted<SharedAudioRenderer>(
      this, media_stream_descriptor, std::move(on_play_state_changed),
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(
      String::Format("%s([state=%s])", __func__, StateToString(state_)));
  ++start_ref_count_;
}

void WebRtcAudioRenderer::Play() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(
      String::Format("%s([state=%s])", __func__, StateToString(state_)));
  if (playing_state_.playing())
    return;

  playing_state_.set_playing(true);

  OnPlayStateChanged(media_stream_descriptor_, &playing_state_);
}

void WebRtcAudioRenderer::EnterPlayState() {
  DVLOG(1) << "WebRtcAudioRenderer::EnterPlayState()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(start_ref_count_, 0) << "Did you forget to call Start()?";
  SendLogMessage(
      String::Format("%s([state=%s])", __func__, StateToString(state_)));
  base::AutoLock auto_lock(lock_);
  if (state_ == kUninitialized)
    return;

  DCHECK(play_ref_count_ == 0 || state_ == kPlaying);
  ++play_ref_count_;

  if (state_ != kPlaying) {
    state_ = kPlaying;

    audio_stream_tracker_.emplace(task_runner_, this,
                                  sink_params_.sample_rate());

    if (audio_fifo_) {
      audio_delay_ = base::TimeDelta();
      audio_fifo_->Clear();
    }
  }
  SendLogMessage(
      String::Format("%s => (state=%s)", __func__, StateToString(state_)));
}

void WebRtcAudioRenderer::Pause() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(
      String::Format("%s([state=%s])", __func__, StateToString(state_)));
  if (!playing_state_.playing())
    return;

  playing_state_.set_playing(false);

  OnPlayStateChanged(media_stream_descriptor_, &playing_state_);
}

void WebRtcAudioRenderer::EnterPauseState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(start_ref_count_, 0) << "Did you forget to call Start()?";
  SendLogMessage(
      String::Format("%s([state=%s])", __func__, StateToString(state_)));
  base::AutoLock auto_lock(lock_);
  if (state_ == kUninitialized)
    return;

  DCHECK_EQ(state_, kPlaying);
  DCHECK_GT(play_ref_count_, 0);
  if (!--play_ref_count_)
    state_ = kPaused;
  SendLogMessage(
      String::Format("%s => (state=%s)", __func__, StateToString(state_)));
}

void WebRtcAudioRenderer::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    SendLogMessage(
        String::Format("%s([state=%s])", __func__, StateToString(state_)));
    base::AutoLock auto_lock(lock_);
    if (state_ == kUninitialized)
      return;

    if (--start_ref_count_)
      return;

    audio_stream_tracker_.reset();
    source_->RemoveAudioRenderer(this);
    source_ = nullptr;
    state_ = kUninitialized;
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
    SendLogMessage(String::Format("%s => (max_render_time=%.3f ms)", __func__,
                                  max_render_time_.InMillisecondsF()));
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
  SendLogMessage(String::Format("%s({volume=%.2f})", __func__, volume));

  playing_state_.set_volume(volume);
  OnPlayStateChanged(media_stream_descriptor_, &playing_state_);
}

base::TimeDelta WebRtcAudioRenderer::GetCurrentRenderTime() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock auto_lock(lock_);
  return current_time_;
}

void WebRtcAudioRenderer::SwitchOutputDevice(
    const std::string& device_id,
    media::OutputDeviceStatusCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(String::Format("%s({device_id=%s} [state=%s])", __func__,
                                device_id.c_str(), StateToString(state_)));
  if (!source_) {
    SendLogMessage(String::Format(
        "%s => (ERROR: OUTPUT_DEVICE_STATUS_ERROR_INTERNAL)", __func__));
    std::move(callback).Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, kUninitialized);
  }

  auto* web_frame =
      static_cast<WebLocalFrame*>(WebFrame::FromCoreFrame(source_frame_));
  if (!web_frame) {
    SendLogMessage(String::Format("%s => (ERROR: No Frame)", __func__));
    std::move(callback).Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
    return;
  }

  if (sink_ && output_device_id_ == String::FromUTF8(device_id)) {
    std::move(callback).Run(media::OUTPUT_DEVICE_STATUS_OK);
    return;
  }

  media::AudioSinkParameters sink_params(session_id_, device_id);
  scoped_refptr<media::AudioRendererSink> new_sink =
      Platform::Current()->NewAudioRendererSink(
          WebAudioDeviceSourceType::kWebRtc, web_frame, sink_params);
  media::OutputDeviceStatus status =
      new_sink->GetOutputDeviceInfo().device_status();
  UMA_HISTOGRAM_ENUMERATION(
      "Media.Audio.WebRTCAudioRenderer.SwitchDeviceStatus", status,
      media::OUTPUT_DEVICE_STATUS_MAX + 1);
  SendLogMessage(String::Format("%s => (sink device_status=%s)", __func__,
                                OutputDeviceStatusToString(status)));

  if (status != media::OUTPUT_DEVICE_STATUS_OK) {
    SendLogMessage(
        String::Format("%s => (ERROR: invalid sink device status)", __func__));
    new_sink->Stop();
    std::move(callback).Run(status);
    return;
  }

  // Make sure to stop the sink while _not_ holding the lock since the Render()
  // callback may currently be executing and trying to grab the lock while we're
  // stopping the thread on which it runs.
  sink_->Stop();
  sink_ = new_sink;
  output_device_id_ = String::FromUTF8(device_id);
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
                                const media::AudioGlitchInfo& glitch_info,
                                media::AudioBus* audio_bus) {
  TRACE_EVENT("audio", "WebRtcAudioRenderer::Render", "playout_delay (ms)",
              delay.InMillisecondsF(), "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF());
  DCHECK(sink_->CurrentThreadIsRenderingThread());
  DCHECK_LE(sink_params_.channels(), 8);
  base::AutoLock auto_lock(lock_);
  if (!source_)
    return 0;

  audio_delay_ = delay;
  glitch_info_accumulator_.Add(glitch_info);

  // Pull the data we will deliver.
  if (audio_fifo_)
    audio_fifo_->Consume(audio_bus, audio_bus->frames());
  else
    SourceCallback(0, audio_bus);

  if (state_ == kPlaying && audio_stream_tracker_) {
    // Mark the stream as alive the first time this method is called.
    audio_stream_tracker_->OnRenderCallbackCalled();
    audio_stream_tracker_->MeasurePower(*audio_bus, audio_bus->frames());
  }

  if (speech_recognition_client_) {
    speech_recognition_client_->AddAudio(*audio_bus);
  }

  return (state_ == kPlaying) ? audio_bus->frames() : 0;
}

void WebRtcAudioRenderer::OnRenderError() {
  DCHECK(on_render_error_callback_);
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebRtcAudioRenderer::OnRenderErrorCrossThread,
                          WrapRefCounted(this)));
}

void WebRtcAudioRenderer::OnRenderErrorCrossThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  on_render_error_callback_.Run();
}

// Called by AudioPullFifo when more data is necessary.
void WebRtcAudioRenderer::SourceCallback(int fifo_frame_delay,
                                         media::AudioBus* audio_bus) {
  TRACE_EVENT("audio", "WebRtcAudioRenderer::SourceCallback", "delay (frames)",
              fifo_frame_delay);
  DCHECK(sink_->CurrentThreadIsRenderingThread());
  base::TimeTicks start_time = base::TimeTicks::Now();
  DVLOG(2) << "WRAR::SourceCallback(" << fifo_frame_delay << ", "
           << audio_bus->channels() << ", " << audio_bus->frames() << ")";

  const base::TimeDelta output_delay =
      audio_delay_ + media::AudioTimestampHelper::FramesToTime(
                         fifo_frame_delay, sink_params_.sample_rate());
  DVLOG(2) << "output_delay (ms): " << output_delay.InMillisecondsF();

  // We need to keep render data for the |source_| regardless of |state_|,
  // otherwise the data will be buffered up inside |source_|.
  source_->RenderData(audio_bus, sink_params_.sample_rate(), output_delay,
                      &current_time_, glitch_info_accumulator_.GetAndReset());

  // Avoid filling up the audio bus if we are not playing; instead
  // return here and ensure that the returned value in Render() is 0.
  if (state_ != kPlaying)
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

  SendLogMessage(String::Format("%s => (source volume changed to %.2f)",
                                __func__, volume));
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
  SendLogMessage(String::Format("%s => (number of playing audio sources=%d)",
                                __func__, static_cast<int>(array.size())));

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
  auto state_it = base::ranges::find(array, state);
  if (state_it == array.end())
    return false;

  array.erase(state_it);

  if (array.empty())
    source_playing_states_.erase(found);

  return true;
}

void WebRtcAudioRenderer::OnPlayStateChanged(
    MediaStreamDescriptor* media_stream_descriptor,
    PlayingState* state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const HeapVector<Member<MediaStreamComponent>>& components =
      media_stream_descriptor->AudioComponents();

  for (auto component : components) {
    // WebRtcAudioRenderer can only render audio tracks received from a remote
    // peer. Since the actual MediaStream is mutable from JavaScript, we need
    // to make sure |component| is actually a remote track.
    PeerConnectionRemoteAudioTrack* const remote_track =
        PeerConnectionRemoteAudioTrack::From(
            MediaStreamAudioTrack::From(component.Get()));
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
    std::erase(states, state);
    if (states.empty())
      it = source_playing_states_.erase(it);
    else
      ++it;
  }
}

void WebRtcAudioRenderer::PrepareSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(String::Format("%s()", __func__));
  media::AudioParameters new_sink_params;
  {
    base::AutoLock lock(lock_);
    new_sink_params = sink_params_;
  }

  const media::OutputDeviceInfo& device_info = sink_->GetOutputDeviceInfo();
  DCHECK_EQ(device_info.device_status(), media::OUTPUT_DEVICE_STATUS_OK);
  SendLogMessage(String::Format(
      "%s => (hardware parameters=[%s])", __func__,
      device_info.output_params().AsHumanReadableString().c_str()));

  // WebRTC does not yet support higher rates than 192000 on the client side
  // and 48000 is the preferred sample rate. Therefore, if 192000 is detected,
  // we change the rate to 48000 instead. The consequence is that the native
  // layer will be opened up at 192kHz but WebRTC will provide data at 48kHz
  // which will then be resampled by the audio converted on the browser side
  // to match the native audio layer.
  int sample_rate = device_info.output_params().sample_rate();
  if (sample_rate >= 192000) {
    SendLogMessage(
        String::Format("%s => (WARNING: WebRTC provides audio at 48kHz and "
                       "resampling takes place to match %dHz)",
                       __func__, sample_rate));
    sample_rate = 48000;
  }
  DVLOG(1) << "WebRtcAudioRenderer::PrepareSink sample_rate " << sample_rate;

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
  SendLogMessage(String::Format("%s => (source_frames_per_buffer=%d)", __func__,
                                source_frames_per_buffer));

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
    SendLogMessage(
        String::Format("%s => (WARNING: sink falls back to stereo)", __func__));
    channels = 2;
    channel_layout = media::CHANNEL_LAYOUT_STEREO;
  }
  const int sink_frames_per_buffer = media::AudioLatency::GetRtcBufferSize(
      sample_rate, device_info.output_params().frames_per_buffer());
  new_sink_params.Reset(kFormat, {channel_layout, channels}, sample_rate,
                        sink_frames_per_buffer);
  DCHECK(new_sink_params.IsValid());

  // Create a FIFO if re-buffering is required to match the source input with
  // the sink request. The source acts as provider here and the sink as
  // consumer.
  const bool different_source_sink_frames =
      source_frames_per_buffer != new_sink_params.frames_per_buffer();
  if (different_source_sink_frames) {
    SendLogMessage(String::Format("%s => (INFO: rebuffering from %d to %d)",
                                  __func__, source_frames_per_buffer,
                                  new_sink_params.frames_per_buffer()));
  }
  {
    base::AutoLock lock(lock_);
    if ((!audio_fifo_ && different_source_sink_frames) ||
        (audio_fifo_ &&
         (audio_fifo_->SizeInFrames() != source_frames_per_buffer ||
          channels != sink_params_.channels()))) {
      audio_fifo_ = std::make_unique<media::AudioPullFifo>(
          channels, source_frames_per_buffer,
          ConvertToBaseRepeatingCallback(
              CrossThreadBindRepeating(&WebRtcAudioRenderer::SourceCallback,
                                       CrossThreadUnretained(this))));
    }
    sink_params_ = new_sink_params;
    SendLogMessage(
        String::Format("%s => (sink_params=[%s])", __func__,
                       sink_params_.AsHumanReadableString().c_str()));
  }

  // Specify the latency info to be passed to the browser side.
  new_sink_params.set_latency_tag(
      Platform::Current()->GetAudioSourceLatencyType(
          WebAudioDeviceSourceType::kWebRtc));

  // Reconfigure() is safe to call, since |sink_| has not started yet, so there
  // are no AddAudio() calls coming from the rendering thread.
  if (speech_recognition_client_) {
    speech_recognition_client_->Reconfigure(new_sink_params);
  }

  sink_->Initialize(new_sink_params, this);
}

void WebRtcAudioRenderer::SendLogMessage(const WTF::String& message) {
  WebRtcLogMessage(String::Format("WRAR::%s [label=%s]", message.Utf8().c_str(),
                                  media_stream_descriptor_id_.Utf8().c_str())
                       .Utf8());
}

}  // namespace blink
