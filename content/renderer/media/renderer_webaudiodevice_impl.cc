// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "content/renderer/media/audio/audio_device_factory.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/silent_sink_suspender.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using blink::WebAudioDevice;
using blink::WebAudioLatencyHint;
using blink::WebLocalFrame;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

blink::WebAudioDeviceSourceType GetLatencyHintSourceType(
    WebAudioLatencyHint::AudioContextLatencyCategory latency_category) {
  switch (latency_category) {
    case WebAudioLatencyHint::kCategoryInteractive:
      return blink::WebAudioDeviceSourceType::kWebAudioInteractive;
    case WebAudioLatencyHint::kCategoryBalanced:
      return blink::WebAudioDeviceSourceType::kWebAudioBalanced;
    case WebAudioLatencyHint::kCategoryPlayback:
      return blink::WebAudioDeviceSourceType::kWebAudioPlayback;
    case WebAudioLatencyHint::kCategoryExact:
      return blink::WebAudioDeviceSourceType::kWebAudioExact;
  }
  NOTREACHED();
  return blink::WebAudioDeviceSourceType::kWebAudioInteractive;
}

int GetOutputBufferSize(const blink::WebAudioLatencyHint& latency_hint,
                        media::AudioLatency::LatencyType latency,
                        const media::AudioParameters& hardware_params) {
  media::AudioParameters::HardwareCapabilities hardware_capabilities =
      hardware_params.hardware_capabilities().value_or(
          media::AudioParameters::HardwareCapabilities());

  // Adjust output buffer size according to the latency requirement.
  switch (latency) {
    case media::AudioLatency::LATENCY_INTERACTIVE:
      return media::AudioLatency::GetInteractiveBufferSize(
          hardware_params.frames_per_buffer());
      break;
    case media::AudioLatency::LATENCY_RTC:
      return media::AudioLatency::GetRtcBufferSize(
          hardware_params.sample_rate(), hardware_params.frames_per_buffer());
      break;
    case media::AudioLatency::LATENCY_PLAYBACK:
      return media::AudioLatency::GetHighLatencyBufferSize(
          hardware_params.sample_rate(), hardware_params.frames_per_buffer());
      break;
    case media::AudioLatency::LATENCY_EXACT_MS:
      return media::AudioLatency::GetExactBufferSize(
          base::TimeDelta::FromSecondsD(latency_hint.Seconds()),
          hardware_params.sample_rate(), hardware_params.frames_per_buffer(),
          hardware_capabilities.min_frames_per_buffer,
          hardware_capabilities.max_frames_per_buffer,
          media::limits::kMaxWebAudioBufferSize);
      break;
    default:
      NOTREACHED();
  }
  return 0;
}

int FrameIdFromCurrentContext() {
  // Assumption: This method is being invoked within a V8 call stack.  CHECKs
  // will fail in the call to frameForCurrentContext() otherwise.
  //
  // Therefore, we can perform look-ups to determine which RenderView is
  // starting the audio device.  The reason for all this is because the creator
  // of the WebAudio objects might not be the actual source of the audio (e.g.,
  // an extension creates a object that is passed and used within a page).
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();
  RenderFrame* const render_frame = RenderFrame::FromWebFrame(web_frame);
  return render_frame ? render_frame->GetRoutingID() : MSG_ROUTING_NONE;
}

media::AudioParameters GetOutputDeviceParameters(
    int frame_id,
    const base::UnguessableToken& session_id,
    const std::string& device_id) {
  return AudioDeviceFactory::GetOutputDeviceInfo(frame_id,
                                                 {session_id, device_id})
      .output_params();
}

}  // namespace

std::unique_ptr<RendererWebAudioDeviceImpl> RendererWebAudioDeviceImpl::Create(
    media::ChannelLayout layout,
    int channels,
    const blink::WebAudioLatencyHint& latency_hint,
    WebAudioDevice::RenderCallback* callback,
    const base::UnguessableToken& session_id) {
  return std::unique_ptr<RendererWebAudioDeviceImpl>(
      new RendererWebAudioDeviceImpl(
          layout, channels, latency_hint, callback, session_id,
          base::BindOnce(&GetOutputDeviceParameters),
          base::BindOnce(&FrameIdFromCurrentContext)));
}

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    media::ChannelLayout layout,
    int channels,
    const blink::WebAudioLatencyHint& latency_hint,
    WebAudioDevice::RenderCallback* callback,
    const base::UnguessableToken& session_id,
    OutputDeviceParamsCallback device_params_cb,
    RenderFrameIdCallback render_frame_id_cb)
    : latency_hint_(latency_hint),
      client_callback_(callback),
      session_id_(session_id),
      frame_id_(std::move(render_frame_id_cb).Run()) {
  DCHECK(client_callback_);
  DCHECK(session_id.is_empty() || frame_id_ != MSG_ROUTING_NONE);

  media::AudioParameters hardware_params(
      std::move(device_params_cb).Run(frame_id_, session_id_, std::string()));

  // On systems without audio hardware the returned parameters may be invalid.
  // In which case just choose whatever we want for the fake device.
  if (!hardware_params.IsValid()) {
    hardware_params.Reset(media::AudioParameters::AUDIO_FAKE,
                          media::CHANNEL_LAYOUT_STEREO, 48000, 480);
  }

  const media::AudioLatency::LatencyType latency =
      AudioDeviceFactory::GetSourceLatencyType(
          GetLatencyHintSourceType(latency_hint_.Category()));

  const int output_buffer_size =
      GetOutputBufferSize(latency_hint_, latency, hardware_params);
  DCHECK_NE(0, output_buffer_size);

  sink_params_.Reset(hardware_params.format(), layout,
                     hardware_params.sample_rate(), output_buffer_size);

  // Always set channels, this should be a no-op in all but the discrete case;
  // this call will fail if channels doesn't match the layout in other cases.
  sink_params_.set_channels_for_discrete(channels);

  // Specify the latency info to be passed to the browser side.
  sink_params_.set_latency_tag(latency);
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  DCHECK(!sink_);
}

void RendererWebAudioDeviceImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (sink_)
    return;  // Already started.

  sink_ = AudioDeviceFactory::NewAudioRendererSink(
      GetLatencyHintSourceType(latency_hint_.Category()), frame_id_,
      media::AudioSinkParameters(session_id_, std::string()));

  // Use the media thread instead of the render thread for fake Render() calls
  // since it has special connotations for Blink and garbage collection. Timeout
  // value chosen to be highly unlikely in the normal case.
  webaudio_suspender_.reset(new media::SilentSinkSuspender(
      this, base::TimeDelta::FromSeconds(30), sink_params_, sink_,
      GetMediaTaskRunner()));
  sink_->Initialize(sink_params_, webaudio_suspender_.get());

  sink_->Start();
  sink_->Play();
}

void RendererWebAudioDeviceImpl::Pause() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (sink_)
    sink_->Pause();
}

void RendererWebAudioDeviceImpl::Resume() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (sink_)
    sink_->Play();
}

void RendererWebAudioDeviceImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (sink_) {
    sink_->Stop();
    sink_ = nullptr;
  }

  webaudio_suspender_.reset();
}

double RendererWebAudioDeviceImpl::SampleRate() {
  return sink_params_.sample_rate();
}

int RendererWebAudioDeviceImpl::FramesPerBuffer() {
  return sink_params_.frames_per_buffer();
}

int RendererWebAudioDeviceImpl::Render(base::TimeDelta delay,
                                       base::TimeTicks delay_timestamp,
                                       int prior_frames_skipped,
                                       media::AudioBus* dest) {
  // Wrap the output pointers using WebVector.
  WebVector<float*> web_audio_dest_data(static_cast<size_t>(dest->channels()));
  for (int i = 0; i < dest->channels(); ++i)
    web_audio_dest_data[i] = dest->channel(i);

  if (!delay.is_zero()) {  // Zero values are send at the first call.
    // Substruct the bus duration to get hardware delay.
    delay -=
        media::AudioTimestampHelper::FramesToTime(dest->frames(), SampleRate());
  }
  DCHECK_GE(delay, base::TimeDelta());

  client_callback_->Render(
      web_audio_dest_data, dest->frames(), delay.InSecondsF(),
      (delay_timestamp - base::TimeTicks()).InSecondsF(), prior_frames_skipped);

  return dest->frames();
}

void RendererWebAudioDeviceImpl::OnRenderError() {
  // TODO(crogers): implement error handling.
}

void RendererWebAudioDeviceImpl::SetMediaTaskRunnerForTesting(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner) {
  media_task_runner_ = media_task_runner;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
RendererWebAudioDeviceImpl::GetMediaTaskRunner() {
  if (!media_task_runner_) {
    media_task_runner_ =
        RenderThreadImpl::current()->GetMediaThreadTaskRunner();
  }
  return media_task_runner_;
}

}  // namespace content
