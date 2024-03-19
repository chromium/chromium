// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/audio_glitch_info.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// TODO(https://crbug.com/638081):
// Like in ProcessedLocalAudioSource::GetBufferSize(), we should re-evaluate
// whether Android needs special treatment here.
const int kFallbackAudioLatencyMs =
#if BUILDFLAG(IS_ANDROID)
    20;
#else
    10;
#endif

static_assert(kFallbackAudioLatencyMs >= 0,
              "Audio latency has to be non-negative.");
static_assert(kFallbackAudioLatencyMs <= 5000,
              "Fallback audio latency exceeds maximum.");

MediaStreamAudioSource::MediaStreamAudioSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool is_local_source,
    bool disable_local_echo)
    : WebPlatformMediaStreamSource(std::move(task_runner)),
      is_local_source_(is_local_source),
      disable_local_echo_(disable_local_echo),
      is_stopped_(false) {
  LogMessage(
      base::StringPrintf("%s({is_local_source=%s}, {disable_local_echo=%s})",
                         __func__, is_local_source ? "local" : "remote",
                         disable_local_echo ? "true" : "false"));
}

MediaStreamAudioSource::MediaStreamAudioSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool is_local_source)
    : MediaStreamAudioSource(std::move(task_runner),
                             is_local_source,
                             false /* disable_local_echo */) {}

MediaStreamAudioSource::~MediaStreamAudioSource() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
}

// static
MediaStreamAudioSource* MediaStreamAudioSource::From(
    MediaStreamSource* source) {
  if (!source || source->GetType() != MediaStreamSource::kTypeAudio) {
    return nullptr;
  }
  return static_cast<MediaStreamAudioSource*>(source->GetPlatformSource());
}

bool MediaStreamAudioSource::ConnectToInitializedTrack(
    MediaStreamComponent* component) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(component);
  DCHECK(MediaStreamAudioTrack::From(component));

  LogMessage(base::StringPrintf("%s(track=%s)", __func__,
                                component->ToString().Utf8().c_str()));

  // Unless the source has already been permanently stopped, ensure it is
  // started. If the source cannot start, the new MediaStreamAudioTrack will be
  // initialized to the stopped/ended state.
  if (!is_stopped_) {
    if (!EnsureSourceIsStarted())
      StopSource();
  }

  // Propagate initial "enabled" state.
  MediaStreamAudioTrack* const track = MediaStreamAudioTrack::From(component);
  DCHECK(track);
  track->SetEnabled(component->Enabled());

  // If the source is stopped, do not start the track.
  if (is_stopped_)
    return false;

  track->Start(WTF::BindOnce(&MediaStreamAudioSource::StopAudioDeliveryTo,
                             weak_factory_.GetWeakPtr(),
                             WTF::Unretained(track)));
  deliverer_.AddConsumer(track);
  LogMessage(
      base::StringPrintf("%s => (added new MediaStreamAudioTrack as consumer, "
                         "total number of consumers=%zu)",
                         __func__, NumTracks()));
  return true;
}

media::AudioParameters MediaStreamAudioSource::GetAudioParameters() const {
  return deliverer_.GetAudioParameters();
}

bool MediaStreamAudioSource::RenderToAssociatedSinkEnabled() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return device().matched_output_device_id.has_value();
}

void* MediaStreamAudioSource::GetClassIdentifier() const {
  return nullptr;
}

bool MediaStreamAudioSource::HasSameReconfigurableSettings(
    const blink::AudioProcessingProperties& selected_properties) const {
  std::optional<blink::AudioProcessingProperties> configured_properties =
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

  std::optional<blink::AudioProcessingProperties> others_properties =
      other_source->GetAudioProcessingProperties();
  std::optional<blink::AudioProcessingProperties> this_properties =
      GetAudioProcessingProperties();

  if (!others_properties || !this_properties)
    return false;

  return this_properties->HasSameNonReconfigurableSettings(*others_properties);
}

void MediaStreamAudioSource::DoChangeSource(
    const MediaStreamDevice& new_device) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (is_stopped_)
    return;

  ChangeSourceImpl(new_device);
}

std::unique_ptr<MediaStreamAudioTrack>
MediaStreamAudioSource::CreateMediaStreamAudioTrack(const std::string& id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  LogMessage(base::StringPrintf("%s({id=%s}, {is_local_source=%s})", __func__,
                                id.c_str(),
                                is_local_source() ? "local" : "remote"));
  return std::make_unique<MediaStreamAudioTrack>(is_local_source());
}

bool MediaStreamAudioSource::EnsureSourceIsStarted() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "MediaStreamAudioSource@" << this << "::EnsureSourceIsStarted()";
  return true;
}

void MediaStreamAudioSource::EnsureSourceIsStopped() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "MediaStreamAudioSource@" << this << "::EnsureSourceIsStopped()";
}

void MediaStreamAudioSource::ChangeSourceImpl(
    const MediaStreamDevice& new_device) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "MediaStreamAudioSource@" << this << "::ChangeSourceImpl()";
  NOTIMPLEMENTED();
}

void MediaStreamAudioSource::SetFormat(const media::AudioParameters& params) {
  LogMessage(base::StringPrintf(
      "%s({params=[%s]}, {old_params=[%s]})", __func__,
      params.AsHumanReadableString().c_str(),
      deliverer_.GetAudioParameters().AsHumanReadableString().c_str()));
  deliverer_.OnSetFormat(params);
}

void MediaStreamAudioSource::DeliverDataToTracks(
    const media::AudioBus& audio_bus,
    base::TimeTicks reference_time,
    const media::AudioGlitchInfo& glitch_info) {
  deliverer_.OnData(audio_bus, reference_time, glitch_info);
}

void MediaStreamAudioSource::DoStopSource() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  LogMessage(base::StringPrintf("%s()", __func__));
  EnsureSourceIsStopped();
  is_stopped_ = true;
}

void MediaStreamAudioSource::StopAudioDeliveryTo(MediaStreamAudioTrack* track) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  const bool did_remove_last_track = deliverer_.RemoveConsumer(track);
  LogMessage(
      base::StringPrintf("%s => (removed MediaStreamAudioTrack as consumer, "
                         "total number of consumers=%zu)",
                         __func__, NumTracks()));

  // The W3C spec requires a source automatically stop when the last track is
  // stopped.
  if (!is_stopped_ && did_remove_last_track) {
    LogMessage(base::StringPrintf("%s => (last track removed, stopping source)",
                                  __func__));
    WebPlatformMediaStreamSource::StopSource();
  }
}

void MediaStreamAudioSource::StopSourceOnError(
    media::AudioCapturerSource::ErrorCode code,
    const std::string& why) {
  LogMessage(base::StringPrintf("%s({why=%s})", __func__, why.c_str()));

  // Stop source when error occurs.
  PostCrossThreadTask(
      *GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          &MediaStreamAudioSource::StopSourceOnErrorOnTaskRunner, GetWeakPtr(),
          code));
}

void MediaStreamAudioSource::StopSourceOnErrorOnTaskRunner(
    media::AudioCapturerSource::ErrorCode code) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  SetErrorCode(code);
  StopSource();
}

void MediaStreamAudioSource::SetMutedState(bool muted_state) {
  LogMessage(base::StringPrintf("%s({muted_state=%s})", __func__,
                                muted_state ? "true" : "false"));
  PostCrossThreadTask(
      *GetTaskRunner(), FROM_HERE,
      WTF::CrossThreadBindOnce(&WebPlatformMediaStreamSource::SetSourceMuted,
                               GetWeakPtr(), muted_state));
}

int MediaStreamAudioSource::NumPreferredChannels() const {
  return deliverer_.NumPreferredChannels();
}

size_t MediaStreamAudioSource::NumTracks() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  Vector<MediaStreamAudioTrack*> audio_tracks;
  deliverer_.GetConsumerList(&audio_tracks);
  return static_cast<int>(audio_tracks.size());
}

void MediaStreamAudioSource::LogMessage(const std::string& message) {
  blink::WebRtcLogMessage(
      base::StringPrintf("MSAS::%s [this=0x%" PRIXPTR "]", message.c_str(),
                         reinterpret_cast<uintptr_t>(this)));
}

}  // namespace blink
