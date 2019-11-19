// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/webaudiosourceprovider_impl.h"

#include <atomic>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/thread_annotations.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_log.h"
#include "third_party/blink/public/platform/web_audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// Simple helper class for Try() locks.  Lock is Try()'d on construction and
// must be checked via the locked() attribute.  If acquisition was successful
// the lock will be released upon destruction.
// TODO(dalecurtis): This should probably move to base/ if others start using
// this pattern.
class AutoTryLock {
 public:
  explicit AutoTryLock(base::Lock& lock)
      : lock_(lock), acquired_(lock_.Try()) {}

  bool locked() const { return acquired_; }

  ~AutoTryLock() {
    if (acquired_) {
      lock_.AssertAcquired();
      lock_.Release();
    }
  }

 private:
  base::Lock& lock_;
  const bool acquired_;
  DISALLOW_COPY_AND_ASSIGN(AutoTryLock);
};

}  // namespace

// TeeFilter is a RenderCallback implementation that allows for a client to get
// a copy of the data being rendered by the |renderer_| on Render(). This class
// also holds on to the necessary audio parameters.
class WebAudioSourceProviderImpl::TeeFilter
    : public AudioRendererSink::RenderCallback {
 public:
  TeeFilter() : copy_required_(false) {}
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
             int prior_frames_skipped,
             media::AudioBus* dest) override;
  void OnRenderError() override;

  bool initialized() const { return !!renderer_; }
  int channels() const { return channels_; }
  int sample_rate() const { return sample_rate_; }

  void SetCopyAudioCallback(CopyAudioCB callback) {
    copy_required_ = !callback.is_null();
    base::AutoLock auto_lock(copy_lock_);
    copy_audio_bus_callback_ = std::move(callback);
  }

 private:
  AudioRendererSink::RenderCallback* renderer_ = nullptr;
  int channels_ = 0;
  int sample_rate_ = 0;

  // The vast majority of the time we're operating in passthrough mode. So only
  // acquire a lock to read |copy_audio_bus_callback_| when necessary.
  std::atomic<bool> copy_required_;
  base::Lock copy_lock_;
  CopyAudioCB copy_audio_bus_callback_ GUARDED_BY(copy_lock_);

  DISALLOW_COPY_AND_ASSIGN(TeeFilter);
};

WebAudioSourceProviderImpl::WebAudioSourceProviderImpl(
    scoped_refptr<media::SwitchableAudioRendererSink> sink,
    media::MediaLog* media_log)
    : volume_(1.0),
      state_(kStopped),
      client_(nullptr),
      sink_(std::move(sink)),
      tee_filter_(new TeeFilter()),
      media_log_(media_log) {}

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

    set_format_cb_ = media::BindToCurrentLoop(WTF::BindRepeating(
        &WebAudioSourceProviderImpl::OnSetFormat, weak_factory_.GetWeakPtr()));

    // If |tee_filter_| is Initialize()d - then run |set_format_cb_| to send
    // |client_| the current format info. Otherwise |set_format_cb_| will get
    // called when Initialize() is called. Note: Always using |set_format_cb_|
    // ensures we have the same locking order when calling into |client_|.
    if (tee_filter_->initialized())
      std::move(set_format_cb_).Run();
    return;
  }

  // Drop client, but normal playback can't be restored. This is okay, the only
  // way to disconnect a client is internally at time of destruction.
  client_ = nullptr;
}

void WebAudioSourceProviderImpl::ProvideInput(
    const WebVector<float*>& audio_data,
    size_t number_of_frames) {
  if (!bus_wrapper_ ||
      static_cast<size_t>(bus_wrapper_->channels()) != audio_data.size()) {
    bus_wrapper_ =
        media::AudioBus::CreateWrapper(static_cast<int>(audio_data.size()));
  }

  const int incoming_number_of_frames = static_cast<int>(number_of_frames);
  bus_wrapper_->set_frames(incoming_number_of_frames);
  for (size_t i = 0; i < audio_data.size(); ++i)
    bus_wrapper_->SetChannelData(static_cast<int>(i), audio_data[i]);

  // Use a try lock to avoid contention in the real-time audio thread.
  AutoTryLock auto_try_lock(sink_lock_);
  if (!auto_try_lock.locked() || state_ != kPlaying) {
    // Provide silence if we failed to acquire the lock or the source is not
    // running.
    bus_wrapper_->Zero();
    return;
  }

  DCHECK(client_);
  DCHECK_EQ(tee_filter_->channels(), bus_wrapper_->channels());
  const int frames = tee_filter_->Render(
      base::TimeDelta(), base::TimeTicks::Now(), 0, bus_wrapper_.get());
  if (frames < incoming_number_of_frames)
    bus_wrapper_->ZeroFramesPartial(frames, incoming_number_of_frames - frames);

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
    std::move(set_format_cb_).Run();
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
  NOTREACHED();  // The blocking API is intentionally not supported.
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
  media::BindToCurrentLoop(
      WTF::Bind(std::move(info_cb),
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

void WebAudioSourceProviderImpl::SetCopyAudioCallback(CopyAudioCB callback) {
  DCHECK(!callback.is_null());
  tee_filter_->SetCopyAudioCallback(std::move(callback));
}

void WebAudioSourceProviderImpl::ClearCopyAudioCallback() {
  tee_filter_->SetCopyAudioCallback(CopyAudioCB());
}

int WebAudioSourceProviderImpl::RenderForTesting(media::AudioBus* audio_bus) {
  return tee_filter_->Render(base::TimeDelta(), base::TimeTicks::Now(), 0,
                             audio_bus);
}

void WebAudioSourceProviderImpl::OnSetFormat() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    return;

  // Inform Blink about the audio stream format.
  client_->SetFormat(tee_filter_->channels(), tee_filter_->sample_rate());
}

int WebAudioSourceProviderImpl::TeeFilter::Render(
    base::TimeDelta delay,
    base::TimeTicks delay_timestamp,
    int prior_frames_skipped,
    media::AudioBus* audio_bus) {
  DCHECK(initialized());

  const int num_rendered_frames = renderer_->Render(
      delay, delay_timestamp, prior_frames_skipped, audio_bus);

  // Avoid taking the copy lock for the vast majority of cases.
  if (copy_required_) {
    base::AutoLock auto_lock(copy_lock_);
    if (!copy_audio_bus_callback_.is_null()) {
      const int64_t frames_delayed =
          media::AudioTimestampHelper::TimeToFrames(delay, sample_rate_);
      std::unique_ptr<media::AudioBus> bus_copy =
          media::AudioBus::Create(audio_bus->channels(), audio_bus->frames());
      audio_bus->CopyTo(bus_copy.get());
      copy_audio_bus_callback_.Run(std::move(bus_copy),
                                   static_cast<uint32_t>(frames_delayed),
                                   sample_rate_);
    }
  }

  return num_rendered_frames;
}

void WebAudioSourceProviderImpl::TeeFilter::OnRenderError() {
  DCHECK(initialized());
  renderer_->OnRenderError();
}

}  // namespace blink
