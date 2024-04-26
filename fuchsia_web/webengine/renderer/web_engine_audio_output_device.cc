// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_audio_output_device.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_timestamp_helper.h"

namespace {

// Total number of buffers used for AudioConsumer.
constexpr size_t kNumBuffers = 4;

// Extra lead time added to min_lead_time reported by AudioConsumer when
// scheduling PumpSamples() timer. This is necessary to make it more likely
// that each packet is sent on time, even if the timer is delayed. Higher values
// increase playback latency, but make underflow less likely. 20ms allows to
// keep latency reasonably low, while making playback reliable under normal
// conditions.
//
// TODO(crbug.com/40159229): It may be possible to reduce this value to reduce
// total latency, but that requires that an elevated scheduling profile is
// applied to this thread.
constexpr base::TimeDelta kLeadTimeExtra = base::Milliseconds(20);

class DefaultAudioThread {
 public:
  DefaultAudioThread() : thread_("WebEngineAudioOutputDevice") {
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    options.thread_type = base::ThreadType::kRealtimeAudio;
    thread_.StartWithOptions(std::move(options));
  }
  ~DefaultAudioThread() = default;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return thread_.task_runner();
  }

 private:
  base::Thread thread_;
};

scoped_refptr<base::SingleThreadTaskRunner> GetDefaultAudioTaskRunner() {
  static base::NoDestructor<DefaultAudioThread> default_audio_thread;
  return default_audio_thread->task_runner();
}

}  // namespace

// static
scoped_refptr<WebEngineAudioOutputDevice> WebEngineAudioOutputDevice::Create(
    fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer_handle,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  scoped_refptr<WebEngineAudioOutputDevice> result(
      new WebEngineAudioOutputDevice(task_runner));
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebEngineAudioOutputDevice::BindAudioConsumerOnAudioThread, result,
          std::move(audio_consumer_handle)));
  return result;
}

// static
scoped_refptr<WebEngineAudioOutputDevice>
WebEngineAudioOutputDevice::CreateOnDefaultThread(
    fidl::InterfaceHandle<fuchsia::media::AudioConsumer>
        audio_consumer_handle) {
  return Create(std::move(audio_consumer_handle), GetDefaultAudioTaskRunner());
}

WebEngineAudioOutputDevice::WebEngineAudioOutputDevice(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

WebEngineAudioOutputDevice::~WebEngineAudioOutputDevice() = default;

void WebEngineAudioOutputDevice::Initialize(
    const media::AudioParameters& params,
    RenderCallback* callback) {
  DCHECK(callback);

  // Save |callback| synchronously here to handle the case when Stop() is called
  // before the DoInitialize() task is processed.
  {
    base::AutoLock auto_lock(callback_lock_);
    DCHECK(!callback_);
    callback_ = callback;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebEngineAudioOutputDevice::InitializeOnAudioThread, this,
                     params));
}

void WebEngineAudioOutputDevice::Start() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebEngineAudioOutputDevice::StartOnAudioThread, this));
}

void WebEngineAudioOutputDevice::Stop() {
  {
    base::AutoLock auto_lock(callback_lock_);
    callback_ = nullptr;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebEngineAudioOutputDevice::StopOnAudioThread, this));
}

void WebEngineAudioOutputDevice::Pause() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebEngineAudioOutputDevice::PauseOnAudioThread, this));
}

void WebEngineAudioOutputDevice::Play() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebEngineAudioOutputDevice::PlayOnAudioThread, this));
}

void WebEngineAudioOutputDevice::Flush() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebEngineAudioOutputDevice::FlushOnAudioThread, this));
}

bool WebEngineAudioOutputDevice::SetVolume(double volume) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebEngineAudioOutputDevice::SetVolumeOnAudioThread, this,
                     volume));
  return true;
}

media::OutputDeviceInfo WebEngineAudioOutputDevice::GetOutputDeviceInfo() {
  // AudioConsumer doesn't provider any information about the output device.
  //
  // TODO(crbug.com/42050621): Update this method when that functionality is
  // implemented.
  return media::OutputDeviceInfo(
      std::string(), media::OUTPUT_DEVICE_STATUS_OK,
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Stereo(), 48000, 480));
}

void WebEngineAudioOutputDevice::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  std::move(info_cb).Run(GetOutputDeviceInfo());
}

bool WebEngineAudioOutputDevice::IsOptimizedForHardwareParameters() {
  // AudioConsumer doesn't provide device parameters (since target device may
  // change).
  return false;
}

bool WebEngineAudioOutputDevice::CurrentThreadIsRenderingThread() {
  return task_runner_->BelongsToCurrentThread();
}

void WebEngineAudioOutputDevice::BindAudioConsumerOnAudioThread(
    fidl::InterfaceHandle<fuchsia::media::AudioConsumer>
        audio_consumer_handle) {
  DCHECK(CurrentThreadIsRenderingThread());
  DCHECK(!audio_consumer_);

  audio_consumer_.Bind(std::move(audio_consumer_handle));
  audio_consumer_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "AudioConsumer disconnected.";
    ReportError();
  });
}

void WebEngineAudioOutputDevice::InitializeOnAudioThread(
    const media::AudioParameters& params) {
  DCHECK(CurrentThreadIsRenderingThread());

  params_ = params;
  audio_bus_ = media::AudioBus::Create(params_);

  UpdateVolume();

  WatchAudioConsumerStatus();
}

void WebEngineAudioOutputDevice::StartOnAudioThread() {
  DCHECK(CurrentThreadIsRenderingThread());

  if (!audio_consumer_)
    return;

  CreateStreamSink();

  media_pos_frames_ = 0;
  audio_consumer_->Start(fuchsia::media::AudioConsumerStartFlags::LOW_LATENCY,
                         fuchsia::media::NO_TIMESTAMP, 0);

  // When AudioConsumer handles the Start() message sent above, it will update
  // its state and sent WatchStatus() response. OnAudioConsumerStatusChanged()
  // will then call SchedulePumpSamples() to start sending audio packets.
}

void WebEngineAudioOutputDevice::StopOnAudioThread() {
  DCHECK(CurrentThreadIsRenderingThread());

  if (!audio_consumer_)
    return;

  audio_consumer_->Stop();
  pump_samples_timer_.Stop();

  audio_consumer_.Unbind();
  stream_sink_.Unbind();
  volume_control_.Unbind();
}

void WebEngineAudioOutputDevice::PauseOnAudioThread() {
  DCHECK(CurrentThreadIsRenderingThread());

  if (!audio_consumer_)
    return;

  paused_ = true;
  audio_consumer_->SetRate(0.0);
  pump_samples_timer_.Stop();
}

void WebEngineAudioOutputDevice::PlayOnAudioThread() {
  DCHECK(CurrentThreadIsRenderingThread());

  if (!audio_consumer_)
    return;

  paused_ = false;
  audio_consumer_->SetRate(1.0);
}

void WebEngineAudioOutputDevice::FlushOnAudioThread() {
  DCHECK(CurrentThreadIsRenderingThread());

  if (!stream_sink_)
    return;

  stream_sink_->DiscardAllPacketsNoReply();
}

void WebEngineAudioOutputDevice::SetVolumeOnAudioThread(double volume) {
  DCHECK(CurrentThreadIsRenderingThread());

  volume_ = volume;
  if (audio_consumer_)
    UpdateVolume();
}

void WebEngineAudioOutputDevice::CreateStreamSink() {
  DCHECK(CurrentThreadIsRenderingThread());
  DCHECK(audio_consumer_);

  // Allocate buffers for the StreamSink.
  size_t buffer_size = params_.GetBytesPerBuffer(media::kSampleFormatF32);
  stream_sink_buffers_.reserve(kNumBuffers);
  available_buffers_indices_.clear();
  std::vector<zx::vmo> vmos_for_stream_sink;
  vmos_for_stream_sink.reserve(kNumBuffers);
  for (size_t i = 0; i < kNumBuffers; ++i) {
    auto region = base::WritableSharedMemoryRegion::Create(buffer_size);
    auto mapping = region.Map();
    if (!mapping.IsValid()) {
      LOG(WARNING) << "Failed to allocate VMO of size " << buffer_size;
      ReportError();
      return;
    }
    stream_sink_buffers_.push_back(std::move(mapping));
    available_buffers_indices_.push_back(i);

    auto read_only_region =
        base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));

    vmos_for_stream_sink.push_back(
        base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
            std::move(read_only_region))
            .PassPlatformHandle());
  }

  // Configure StreamSink.
  fuchsia::media::AudioStreamType stream_type;
  stream_type.channels = params_.channels();
  stream_type.frames_per_second = params_.sample_rate();
  stream_type.sample_format = fuchsia::media::AudioSampleFormat::FLOAT;
  audio_consumer_->CreateStreamSink(std::move(vmos_for_stream_sink),
                                    std::move(stream_type), nullptr,
                                    stream_sink_.NewRequest());
  stream_sink_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "StreamSink disconnected.";
    ReportError();
  });
}

void WebEngineAudioOutputDevice::UpdateVolume() {
  DCHECK(CurrentThreadIsRenderingThread());
  DCHECK(audio_consumer_);
  if (!volume_control_) {
    audio_consumer_->BindVolumeControl(volume_control_.NewRequest());
    volume_control_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "VolumeControl disconnected.";
    });
  }
  volume_control_->SetVolume(volume_);
}

void WebEngineAudioOutputDevice::WatchAudioConsumerStatus() {
  DCHECK(CurrentThreadIsRenderingThread());
  audio_consumer_->WatchStatus(fit::bind_member(
      this, &WebEngineAudioOutputDevice::OnAudioConsumerStatusChanged));
}

void WebEngineAudioOutputDevice::OnAudioConsumerStatusChanged(
    fuchsia::media::AudioConsumerStatus status) {
  DCHECK(CurrentThreadIsRenderingThread());

  if (!status.has_min_lead_time()) {
    DLOG(ERROR) << "AudioConsumerStatus.min_lead_time isn't set.";
    ReportError();
    return;
  }

  min_lead_time_ = base::Nanoseconds(status.min_lead_time());

  if (status.has_presentation_timeline()) {
    timeline_reference_time_ = base::TimeTicks::FromZxTime(
        status.presentation_timeline().reference_time);
    timeline_subject_time_ =
        base::Nanoseconds(status.presentation_timeline().subject_time);
    timeline_reference_delta_ = status.presentation_timeline().reference_delta;
    timeline_subject_delta_ = status.presentation_timeline().subject_delta;
  } else {
    // Reset |timeline_reference_time_| to null value, which is used to indicate
    // that there is no presentation timeline.
    timeline_reference_time_ = base::TimeTicks();
  }

  // Reschedule the timer for the new timeline.
  pump_samples_timer_.Stop();
  SchedulePumpSamples();

  WatchAudioConsumerStatus();
}

void WebEngineAudioOutputDevice::SchedulePumpSamples() {
  DCHECK(CurrentThreadIsRenderingThread());

  if (paused_ || timeline_reference_time_.is_null() ||
      pump_samples_timer_.IsRunning() || available_buffers_indices_.empty()) {
    return;
  }

  // Current position in the stream.
  auto media_pos = media::AudioTimestampHelper::FramesToTime(
      media_pos_frames_, params_.sample_rate());

  // Calculate expected playback time for the next sample based on the
  // presentation timeline provided by the AudioConsumer.
  // See https://fuchsia.dev/reference/fidl/fuchsia.media#formulas .
  // AudioConsumer uses monotonic clock (aka base::TimeTicks) as a reference
  // timeline. Subject timeline corresponds to position within the stream, which
  // is stored as |media_pos_frames_| and then passed in the |pts| field in each
  // packet produced in PumpSamples().
  auto playback_time = timeline_reference_time_ +
                       (media_pos - timeline_subject_time_) *
                           timeline_reference_delta_ / timeline_subject_delta_;

  base::TimeTicks now = base::TimeTicks::Now();

  // Target time for when PumpSamples() should run.
  base::TimeTicks target_time = playback_time - min_lead_time_ - kLeadTimeExtra;

  base::TimeDelta delay = target_time - now;
  pump_samples_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&WebEngineAudioOutputDevice::PumpSamples, this,
                     playback_time));
}

void WebEngineAudioOutputDevice::PumpSamples(base::TimeTicks playback_time) {
  DCHECK(CurrentThreadIsRenderingThread());

  auto now = base::TimeTicks::Now();

  // Check if it's too late to send the next packet. If it is, then advance
  // current stream position.
  auto lead_time = playback_time - now;
  if (lead_time < min_lead_time_) {
    auto new_playback_time = now + min_lead_time_;
    auto skipped_time = new_playback_time - playback_time;
    media_pos_frames_ += media::AudioTimestampHelper::TimeToFrames(
        skipped_time, params_.sample_rate());
    playback_time += skipped_time;
  }

  int frames_filled;
  {
    base::AutoLock auto_lock(callback_lock_);

    // |callback_| may be reset in Stop(). No need to keep rendering the stream
    // in that case.
    if (!callback_)
      return;

    frames_filled =
        callback_->Render(playback_time - now, now, {}, audio_bus_.get());
  }

  if (frames_filled) {
    DCHECK(!available_buffers_indices_.empty());
    int buffer_index = available_buffers_indices_.back();
    available_buffers_indices_.pop_back();

    audio_bus_->ToInterleaved<media::Float32SampleTypeTraitsNoClip>(
        frames_filled,
        static_cast<float*>(stream_sink_buffers_[buffer_index].memory()));

    fuchsia::media::StreamPacket packet;
    packet.payload_buffer_id = buffer_index;
    packet.pts = media::AudioTimestampHelper::FramesToTime(
                     media_pos_frames_, params_.sample_rate())
                     .InNanoseconds();
    packet.payload_offset = 0;
    packet.payload_size = frames_filled * sizeof(float) * params_.channels();

    stream_sink_->SendPacket(std::move(packet), [this, buffer_index]() {
      OnStreamSendDone(buffer_index);
    });

    media_pos_frames_ += frames_filled;
  }

  SchedulePumpSamples();
}

void WebEngineAudioOutputDevice::OnStreamSendDone(size_t buffer_index) {
  DCHECK(CurrentThreadIsRenderingThread());

  available_buffers_indices_.push_back(buffer_index);
  SchedulePumpSamples();
}

void WebEngineAudioOutputDevice::ReportError() {
  DCHECK(CurrentThreadIsRenderingThread());

  audio_consumer_.Unbind();
  stream_sink_.Unbind();
  volume_control_.Unbind();
  pump_samples_timer_.Stop();
  {
    base::AutoLock auto_lock(callback_lock_);
    if (callback_)
      callback_->OnRenderError();
  }
}
