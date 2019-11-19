// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_track.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

const int kMaxAudioLatencyMs = 5000;
static_assert(std::numeric_limits<int>::max() / media::limits::kMaxSampleRate >
                  kMaxAudioLatencyMs,
              "The maxium audio latency can cause overflow.");

// TODO(https://crbug.com/638081):
// Like in ProcessedLocalAudioSource::GetBufferSize(), we should re-evaluate
// whether Android needs special treatment here.
const int kFallbackAudioLatencyMs =
#if defined(OS_ANDROID)
    20;
#else
    10;
#endif

static_assert(kFallbackAudioLatencyMs >= 0,
              "Audio latency has to be non-negative.");
static_assert(kFallbackAudioLatencyMs <= kMaxAudioLatencyMs,
              "Fallback audio latency exceeds maximum.");

MediaStreamAudioSource::MediaStreamAudioSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool is_local_source,
    bool disable_local_echo)
    : is_local_source_(is_local_source),
      disable_local_echo_(disable_local_echo),
      is_stopped_(false),
      task_runner_(std::move(task_runner)) {
  DVLOG(1) << "MediaStreamAudioSource@" << this << "::MediaStreamAudioSource("
           << (is_local_source_ ? "local" : "remote") << " source)";
}

MediaStreamAudioSource::MediaStreamAudioSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool is_local_source)
    : MediaStreamAudioSource(std::move(task_runner),
                             is_local_source,
                             false /* disable_local_echo */) {}

MediaStreamAudioSource::~MediaStreamAudioSource() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "MediaStreamAudioSource@" << this << " is being destroyed.";
}

// static
MediaStreamAudioSource* MediaStreamAudioSource::From(
    const WebMediaStreamSource& source) {
  if (source.IsNull() || source.GetType() != WebMediaStreamSource::kTypeAudio) {
    return nullptr;
  }
  return static_cast<MediaStreamAudioSource*>(source.GetPlatformSource());
}

bool MediaStreamAudioSource::ConnectToTrack(
    const WebMediaStreamTrack& blink_track) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!blink_track.IsNull());

  // Sanity-check that there is not already a MediaStreamAudioTrack instance
  // associated with |blink_track|.
  if (MediaStreamAudioTrack::From(blink_track)) {
    LOG(DFATAL)
        << "Attempting to connect another source to a WebMediaStreamTrack.";
    return false;
  }

  // Unless the source has already been permanently stopped, ensure it is
  // started. If the source cannot start, the new MediaStreamAudioTrack will be
  // initialized to the stopped/ended state.
  if (!is_stopped_) {
    if (!EnsureSourceIsStarted())
      StopSource();
  }

  // Create and initialize a new MediaStreamAudioTrack and pass ownership of it
  // to the WebMediaStreamTrack.
  WebMediaStreamTrack mutable_blink_track = blink_track;
  mutable_blink_track.SetPlatformTrack(
      CreateMediaStreamAudioTrack(blink_track.Id().Utf8()));

  // Propagate initial "enabled" state.
  MediaStreamAudioTrack* const track = MediaStreamAudioTrack::From(blink_track);
  DCHECK(track);
  track->SetEnabled(blink_track.IsEnabled());

  // If the source is stopped, do not start the track.
  if (is_stopped_)
    return false;

  track->Start(WTF::Bind(&MediaStreamAudioSource::StopAudioDeliveryTo,
                         weak_factory_.GetWeakPtr(), WTF::Unretained(track)));
  DVLOG(1) << "Adding MediaStreamAudioTrack@" << track
           << " as a consumer of MediaStreamAudioSource@" << this << '.';
  deliverer_.AddConsumer(track);
  return true;
}

media::AudioParameters MediaStreamAudioSource::GetAudioParameters() const {
  return deliverer_.GetAudioParameters();
}

bool MediaStreamAudioSource::RenderToAssociatedSinkEnabled() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return device().matched_output_device_id.has_value();
}

void* MediaStreamAudioSource::GetClassIdentifier() const {
  return nullptr;
}

bool MediaStreamAudioSource::HasSameReconfigurableSettings(
    const blink::AudioProcessingProperties& selected_properties) const {
  base::Optional<blink::AudioProcessingProperties> configured_properties =
      GetAudioProcessingProperties();
  if (!configured_properties)
    return false;

  return selected_properties.HasSameReconfigurableSettings(
      *configured_properties);
}

bool MediaStreamAudioSource::HasSameNonReconfigurableSettings(
    MediaStreamAudioSource* other_source) const {
  if (!other_source)
    return false;

  base::Optional<blink::AudioProcessingProperties> others_properties =
      other_source->GetAudioProcessingProperties();
  base::Optional<blink::AudioProcessingProperties> this_properties =
      GetAudioProcessingProperties();

  if (!others_properties || !this_properties)
    return false;

  return this_properties->HasSameNonReconfigurableSettings(*others_properties);
}

void MediaStreamAudioSource::DoChangeSource(
    const MediaStreamDevice& new_device) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (is_stopped_)
    return;

  ChangeSourceImpl(new_device);
}

std::unique_ptr<MediaStreamAudioTrack>
MediaStreamAudioSource::CreateMediaStreamAudioTrack(const std::string& id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return std::unique_ptr<MediaStreamAudioTrack>(
      new MediaStreamAudioTrack(is_local_source()));
}

bool MediaStreamAudioSource::EnsureSourceIsStarted() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "MediaStreamAudioSource@" << this << "::EnsureSourceIsStarted()";
  return true;
}

void MediaStreamAudioSource::EnsureSourceIsStopped() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "MediaStreamAudioSource@" << this << "::EnsureSourceIsStopped()";
}

void MediaStreamAudioSource::ChangeSourceImpl(
    const MediaStreamDevice& new_device) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "MediaStreamAudioSource@" << this << "::ChangeSourceImpl()";
  NOTIMPLEMENTED();
}

void MediaStreamAudioSource::SetFormat(const media::AudioParameters& params) {
  DVLOG(1) << "MediaStreamAudioSource@" << this << "::SetFormat("
           << params.AsHumanReadableString() << "), was previously set to {"
           << deliverer_.GetAudioParameters().AsHumanReadableString() << "}.";
  deliverer_.OnSetFormat(params);
}

void MediaStreamAudioSource::DeliverDataToTracks(
    const media::AudioBus& audio_bus,
    base::TimeTicks reference_time) {
  deliverer_.OnData(audio_bus, reference_time);
}

void MediaStreamAudioSource::DoStopSource() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  EnsureSourceIsStopped();
  is_stopped_ = true;
}

void MediaStreamAudioSource::StopAudioDeliveryTo(MediaStreamAudioTrack* track) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  const bool did_remove_last_track = deliverer_.RemoveConsumer(track);
  DVLOG(1) << "Removed MediaStreamAudioTrack@" << track
           << " as a consumer of MediaStreamAudioSource@" << this << '.';

  // The W3C spec requires a source automatically stop when the last track is
  // stopped.
  if (!is_stopped_ && did_remove_last_track)
    WebPlatformMediaStreamSource::StopSource();
}

void MediaStreamAudioSource::StopSourceOnError(const std::string& why) {
  VLOG(1) << why;

  // Stop source when error occurs.
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebPlatformMediaStreamSource::StopSource,
                          GetWeakPtr()));
}

void MediaStreamAudioSource::SetMutedState(bool muted_state) {
  DVLOG(3) << "MediaStreamAudioSource::SetMutedState state=" << muted_state;
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&WebPlatformMediaStreamSource::SetSourceMuted,
                               GetWeakPtr(), muted_state));
}

base::SingleThreadTaskRunner* MediaStreamAudioSource::GetTaskRunner() const {
  return task_runner_.get();
}

}  // namespace blink
