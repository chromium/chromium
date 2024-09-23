// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_audio_source_provider_impl.h"

#include <atomic>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/thread_annotations.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/media_log.h"
#include "third_party/blink/renderer/platform/media/web_audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// TeeFilter is a RenderCallback implementation that allows for a client to get
// a copy of the data being rendered by the |renderer_| on Render(). This class
// also holds on to the necessary audio parameters.
class WebAudioSourceProviderImpl::TeeFilter
    : public AudioRendererSink::RenderCallback {
 public:
  TeeFilter() : copy_required_(false) {}
  TeeFilter(const TeeFilter&) = delete;
  TeeFilter& operator=(const TeeFilter&) = delete;
  ~TeeFilter() override = default;

  void Initialize(AudioRendererSink::RenderCallback* renderer,
                  int channels,
                  int sample_rate) {
    DCHECK(renderer);
    renderer_ = renderer;
    channels_ = channels;
    sample_rate_ = sample_rate;
  }

  // AudioRendererSink::RenderCallback implementation.
  // These are forwarders to |renderer_| and are here to allow for a client to
  // get a copy of the rendered audio by SetCopyAudioCallback().
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* audio_bus) override {
    DCHECK(initialized());
    DCHECK_EQ(audio_bus->channels(), channels_);

    const int num_rendered_frames =
        renderer_->Render(delay, delay_timestamp, glitch_info, audio_bus);

    // Avoid taking the copy lock for the vast majority of cases.
    if (copy_required_) {
      base::AutoLock auto_lock(copy_lock_);
      if (!copy_audio_bus_callback_.is_null()) {
        const int64_t frames_delayed =
            media::AudioTimestampHelper::TimeToFrames(delay, sample_rate_);
        std::unique_ptr<media::AudioBus> bus_copy =
            media::AudioBus::Create(audio_bus->channels(), audio_bus->frames());
        // Disable copying when origin is tainted.
        if (origin_tainted_.IsSet())
          bus_copy->Zero();
        else
          audio_bus->CopyTo(bus_copy.get());

        // TODO(fhernqvist): Propagate glitch info through here if the callback
        // needs it.
        copy_audio_bus_callback_.Run(std::move(bus_copy),
                                     static_cast<uint32_t>(frames_delayed),
                                     sample_rate_);
      }
    }

    return num_rendered_frames;
  }

  void OnRenderError() override {
    DCHECK(initialized());
    renderer_->OnRenderError();
  }

  bool initialized() const { return !!renderer_; }
  int channels() const { return channels_; }
  int sample_rate() const { return sample_rate_; }

  void SetCopyAudioCallback(CopyAudioCB callback) {
    copy_required_ = !callback.is_null();
    base::AutoLock auto_lock(copy_lock_);
    copy_audio_bus_callback_ = std::move(callback);
  }

  void TaintOrigin() { origin_tainted_.Set(); }
  bool is_tainted() const { return origin_tainted_.IsSet(); }

 private:
  raw_ptr<AudioRendererSink::RenderCallback, DanglingUntriaged> renderer_ =
      nullptr;
  int channels_ = 0;
  int sample_rate_ = 0;

  // Indicates whether the audio source is tainted, and output should be muted.
  // This can happen if the media element source is a cross-origin source which
  // the page is not allowed to access due to CORS restrictions.
  base::AtomicFlag origin_tainted_;

  // The vast majority of the time we're operating in passthrough mode. So only
  // acquire a lock to read |copy_audio_bus_callback_| when necessary.
  std::atomic<bool> copy_required_;
  base::Lock copy_lock_;
  CopyAudioCB copy_audio_bus_callback_ GUARDED_BY(copy_lock_);
};

WebAudioSourceProviderImpl::WebAudioSourceProviderImpl(
    scoped_refptr<media::SwitchableAudioRendererSink> sink,
    media::MediaLog* media_log,
    base::OnceClosure on_set_client_callback /* = base::OnceClosure()*/)
    : sink_(std::move(sink)),
      tee_filter_(std::make_unique<TeeFilter>()),
      media_log_(media_log),
      on_set_client_callback_(std::move(on_set_client_callback)) {}

WebAudioSourceProviderImpl::~WebAudioSourceProviderImpl() = default;

void WebAudioSourceProviderImpl::SetClient(
    WebAudioSourceProviderClient* client) {
  // Skip taking the lock if unnecessary. This function is the only setter for
  // |client_| so it's safe to check |client_| outside of the lock.
  if (client_ == client)
    return;

  base::AutoLock auto_lock(sink_lock_);
  if (client) {
    // Detach the audio renderer from normal playback.
    if (sink_) {
      sink_->Stop();

      // It's not possible to resume an element after disconnection, so just
      // drop the sink entirely for now.
      sink_ = nullptr;
    }

    // The client will now take control by calling provideInput() periodically.
    client_ = client;

    set_format_cb_ = base::BindPostTaskToCurrentDefault(WTF::BindRepeating(
        &WebAudioSourceProviderImpl::OnSetFormat, weak_factory_.GetWeakPtr()));

    // If |tee_filter_| is Initialize()d - then run |set_format_cb_| to send
    // |client_| the current format info. Otherwise |set_format_cb_| will get
    // called when Initialize() is called. Note: Always using |set_format_cb_|
    // ensures we have the same locking order when calling into |client_|.
    if (tee_filter_->initialized())
      set_format_cb_.Run();

    if (on_set_client_callback_)
      std::move(on_set_client_callback_).Run();

    return;
  }

  // Drop client, but normal playback can't be restored. This is okay, the only
  // way to disconnect a client is internally at time of destruction.
  client_ = nullptr;

  // We need to invalidate WeakPtr references on the renderer thread.
  set_format_cb_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

void WebAudioSourceProviderImpl::ProvideInput(
    const WebVector<float*>& audio_data,
    int number_of_frames) {
  if (!bus_wrapper_ ||
      static_cast<size_t>(bus_wrapper_->channels()) != audio_data.size()) {
    bus_wrapper_ =
        media::AudioBus::CreateWrapper(static_cast<int>(audio_data.size()));
  }

  bus_wrapper_->set_frames(number_of_frames);
  for (size_t i = 0; i < audio_data.size(); ++i)
    bus_wrapper_->SetChannelData(static_cast<int>(i), audio_data[i]);

  // Use a try lock to avoid contention in the real-time audio thread.
  base::AutoTryLock auto_try_lock(sink_lock_);
  if (!auto_try_lock.is_acquired() || state_ != kPlaying) {
    // Provide silence if we failed to acquire the lock or the source is not
    // running.
    bus_wrapper_->Zero();
    return;
  }

  DCHECK(client_);

  // It may be the case that the given |audio_data| doesn't have the same number
  // of channels as we were expecting, due to a race condition. In that case,
  // simply output silence.
  if (tee_filter_->channels() != bus_wrapper_->channels()) {
    DVLOG(2) << "Outputting silence due to mismatched channel count";
    bus_wrapper_->Zero();
    return;
  }

  // TODO(fhernqvist): If we need glitches propagated through WebAudio, plumb
  // them through here.
  const int frames = tee_filter_->Render(
      base::TimeDelta(), base::TimeTicks::Now(), {}, bus_wrapper_.get());

  // Zero out frames after rendering for tainted origins.
  if (tee_filter_->is_tainted()) {
    bus_wrapper_->Zero();
    return;
  }

  if (frames < number_of_frames)
    bus_wrapper_->ZeroFramesPartial(frames, number_of_frames - frames);

  bus_wrapper_->Scale(volume_);
}

void WebAudioSourceProviderImpl::Initialize(
    const media::AudioParameters& params,
    RenderCallback* renderer) {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK_EQ(state_, kStopped);

  tee_filter_->Initialize(renderer, params.channels(), params.sample_rate());

  if (sink_)
    sink_->Initialize(params, tee_filter_.get());

  if (set_format_cb_)
    set_format_cb_.Run();
}

void WebAudioSourceProviderImpl::Start() {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK(tee_filter_);
  DCHECK_EQ(state_, kStopped);
  state_ = kStarted;
  if (!client_ && sink_)
    sink_->Start();
}

void WebAudioSourceProviderImpl::Stop() {
  base::AutoLock auto_lock(sink_lock_);
  state_ = kStopped;
  if (!client_ && sink_)
    sink_->Stop();
}

void WebAudioSourceProviderImpl::Play() {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK_EQ(state_, kStarted);
  state_ = kPlaying;
  if (!client_ && sink_)
    sink_->Play();
}

void WebAudioSourceProviderImpl::Pause() {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK(state_ == kPlaying || state_ == kStarted);
  state_ = kStarted;
  if (!client_ && sink_)
    sink_->Pause();
}

void WebAudioSourceProviderImpl::Flush() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_ && sink_)
    sink_->Flush();
}

bool WebAudioSourceProviderImpl::SetVolume(double volume) {
  base::AutoLock auto_lock(sink_lock_);
  volume_ = volume;
  if (!client_ && sink_)
    sink_->SetVolume(volume);
  return true;
}

media::OutputDeviceInfo WebAudioSourceProviderImpl::GetOutputDeviceInfo() {
  NOTREACHED_IN_MIGRATION();  // The blocking API is intentionally not
                              // supported.
  return media::OutputDeviceInfo();
}

void WebAudioSourceProviderImpl::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  base::AutoLock auto_lock(sink_lock_);
  if (sink_) {
    sink_->GetOutputDeviceInfoAsync(std::move(info_cb));
    return;
  }

  // Just return empty hardware parameters. When a |client_| is attached, the
  // underlying audio renderer will prefer the media parameters. See
  // IsOptimizedForHardwareParameters() for more details.
  base::BindPostTaskToCurrentDefault(
      WTF::BindOnce(std::move(info_cb),
                    media::OutputDeviceInfo(media::OUTPUT_DEVICE_STATUS_OK)))
      .Run();
}

bool WebAudioSourceProviderImpl::IsOptimizedForHardwareParameters() {
  base::AutoLock auto_lock(sink_lock_);
  return client_ ? false : true;
}

bool WebAudioSourceProviderImpl::CurrentThreadIsRenderingThread() {
  NOTIMPLEMENTED();
  return false;
}

void WebAudioSourceProviderImpl::SwitchOutputDevice(
    const std::string& device_id,
    media::OutputDeviceStatusCB callback) {
  base::AutoLock auto_lock(sink_lock_);
  if (client_ || !sink_)
    std::move(callback).Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
  else
    sink_->SwitchOutputDevice(device_id, std::move(callback));
}

void WebAudioSourceProviderImpl::TaintOrigin() {
  tee_filter_->TaintOrigin();
}

void WebAudioSourceProviderImpl::SetCopyAudioCallback(CopyAudioCB callback) {
  DCHECK(!callback.is_null());
  tee_filter_->SetCopyAudioCallback(std::move(callback));
  has_copy_audio_callback_ = true;
}

void WebAudioSourceProviderImpl::ClearCopyAudioCallback() {
  tee_filter_->SetCopyAudioCallback(CopyAudioCB());
  has_copy_audio_callback_ = false;
}

int WebAudioSourceProviderImpl::RenderForTesting(media::AudioBus* audio_bus) {
  return tee_filter_->Render(base::TimeDelta(), base::TimeTicks::Now(), {},
                             audio_bus);
}

bool WebAudioSourceProviderImpl::IsAudioBeingCaptured() const {
  return has_copy_audio_callback_ || client_;
}

void WebAudioSourceProviderImpl::OnSetFormat() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    return;

  // Inform Blink about the audio stream format.
  client_->SetFormat(tee_filter_->channels(), tee_filter_->sample_rate());
}

}  // namespace blink
