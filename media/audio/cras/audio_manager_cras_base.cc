// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras_base.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/cras/cras_input.h"
#include "media/audio/cras/cras_unified.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/localized_strings.h"

namespace media {
namespace {

// Maximum number of output streams that can be open simultaneously.
const int kMaxOutputStreams = 50;

}  // namespace

AudioManagerCrasBase::AudioManagerCrasBase(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerCrasBase::~AudioManagerCrasBase() = default;

const char* AudioManagerCrasBase::GetName() {
  return "CRAS";
}

AudioOutputStream* AudioManagerCrasBase::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  // Pinning stream is not supported for MakeLinearOutputStream.
  return MakeOutputStream(params, AudioDeviceDescription::kDefaultDeviceId,
                          std::move(log_callback));
}

AudioOutputStream* AudioManagerCrasBase::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(params, device_id, std::move(log_callback));
}

AudioInputStream* AudioManagerCrasBase::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id, std::move(log_callback));
}

AudioInputStream* AudioManagerCrasBase::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id, std::move(log_callback));
}

AudioOutputStream* AudioManagerCrasBase::MakeOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return new CrasUnifiedStream(params, this, device_id,
                               std::move(log_callback));
}

AudioInputStream* AudioManagerCrasBase::MakeInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return new CrasInputStream(params, this, device_id, std::move(log_callback));
}

void AudioManagerCrasBase::RegisterSystemAecDumpSource(
    AecdumpRecordingSource* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (aecdump_recording_manager_) {
    aecdump_recording_manager_->RegisterAecdumpSource(stream);
  }
}

void AudioManagerCrasBase::DeregisterSystemAecDumpSource(
    AecdumpRecordingSource* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (aecdump_recording_manager_) {
    aecdump_recording_manager_->DeregisterAecdumpSource(stream);
  }
}

void AudioManagerCrasBase::SetAecDumpRecordingManager(
    base::WeakPtr<AecdumpRecordingManager> aecdump_recording_manager) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(!aecdump_recording_manager_);
  aecdump_recording_manager_ = aecdump_recording_manager;
}

}  // namespace media
