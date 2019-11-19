// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/cras_input.h"

#include <math.h>
#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/cras/audio_manager_cras.h"

namespace media {

CrasInputStream::CrasInputStream(const AudioParameters& params,
                                 AudioManagerCras* manager,
                                 const std::string& device_id)
    : audio_manager_(manager),
      bytes_per_frame_(0),
      callback_(NULL),
      client_(NULL),
      params_(params),
      started_(false),
      stream_id_(0),
      stream_direction_(CRAS_STREAM_INPUT),
      pin_device_(NO_DEVICE),
      is_loopback_(AudioDeviceDescription::IsLoopbackDevice(device_id)),
      mute_system_audio_(device_id ==
                         AudioDeviceDescription::kLoopbackWithMuteDeviceId),
      mute_done_(false),
      input_volume_(1.0f) {
  DCHECK(audio_manager_);
  audio_bus_ = AudioBus::Create(params_);
  if (!audio_manager_->IsDefault(device_id, true)) {
    uint64_t cras_node_id;
    base::StringToUint64(device_id, &cras_node_id);
    pin_device_ = dev_index_of(cras_node_id);
  }
}

CrasInputStream::~CrasInputStream() {
  DCHECK(!client_);
}

bool CrasInputStream::Open() {
  if (client_) {
    NOTREACHED() << "CrasInputStream already open";
    return false;  // Already open.
  }

  // Sanity check input values.
  if (params_.sample_rate() <= 0) {
    DLOG(WARNING) << "Unsupported audio frequency.";
    return false;
  }

  if (AudioParameters::AUDIO_PCM_LINEAR != params_.format() &&
      AudioParameters::AUDIO_PCM_LOW_LATENCY != params_.format()) {
    DLOG(WARNING) << "Unsupported audio format.";
    return false;
  }

  // Create the client and connect to the CRAS server.
  if (cras_client_create(&client_) < 0) {
    DLOG(WARNING) << "Couldn't create CRAS client.\n";
    client_ = NULL;
    return false;
  }

  if (cras_client_connect(client_)) {
    DLOG(WARNING) << "Couldn't connect CRAS client.\n";
    cras_client_destroy(client_);
    client_ = NULL;
    return false;
  }

  // Then start running the client.
  if (cras_client_run_thread(client_)) {
    DLOG(WARNING) << "Couldn't run CRAS client.\n";
    cras_client_destroy(client_);
    client_ = NULL;
    return false;
  }

  if (is_loopback_) {
    if (cras_client_connected_wait(client_) < 0) {
      DLOG(WARNING) << "Couldn't synchronize data.";
      // TODO(chinyue): Add a DestroyClientOnError method to de-duplicate the
      // cleanup code.
      cras_client_destroy(client_);
      client_ = NULL;
      return false;
    }

    pin_device_ = cras_client_get_first_dev_type_idx(client_,
        CRAS_NODE_TYPE_POST_MIX_PRE_DSP, CRAS_STREAM_INPUT);
    if (pin_device_ < 0) {
      DLOG(WARNING) << "Couldn't find CRAS loopback device.";
      cras_client_destroy(client_);
      client_ = NULL;
      return false;
    }
  }

  return true;
}

void CrasInputStream::Close() {
  Stop();

  if (client_) {
    cras_client_stop(client_);
    cras_client_destroy(client_);
    client_ = NULL;
  }

  // Signal to the manager that we're closed and can be removed.
  // Should be last call in the method as it deletes "this".
  audio_manager_->ReleaseInputStream(this);
}

inline bool CrasInputStream::UseCrasAec() const {
  return params_.effects() & AudioParameters::ECHO_CANCELLER;
}

void CrasInputStream::Start(AudioInputCallback* callback) {
  DCHECK(client_);
  DCHECK(callback);

  // Channel map to CRAS_CHANNEL, values in the same order of
  // corresponding source in Chromium defined Channels.
  static const int kChannelMap[] = {
    CRAS_CH_FL,
    CRAS_CH_FR,
    CRAS_CH_FC,
    CRAS_CH_LFE,
    CRAS_CH_RL,
    CRAS_CH_RR,
    CRAS_CH_FLC,
    CRAS_CH_FRC,
    CRAS_CH_RC,
    CRAS_CH_SL,
    CRAS_CH_SR
  };
  static_assert(base::size(kChannelMap) == CHANNELS_MAX + 1,
                "kChannelMap array size should match");

  // If already playing, stop before re-starting.
  if (started_)
    return;

  StartAgc();

  callback_ = callback;

  // Prepare |audio_format| and |stream_params| for the stream we
  // will create.
  cras_audio_format* audio_format = cras_audio_format_create(
      SND_PCM_FORMAT_S16, params_.sample_rate(), params_.channels());
  if (!audio_format) {
    DLOG(WARNING) << "Error setting up audio parameters.";
    callback_->OnError();
    callback_ = NULL;
    return;
  }

  // Initialize channel layout to all -1 to indicate that none of
  // the channels is set in the layout.
  int8_t layout[CRAS_CH_MAX];
  for (size_t i = 0; i < base::size(layout); ++i)
    layout[i] = -1;

  // Converts to CRAS defined channels. ChannelOrder will return -1
  // for channels that are not present in params_.channel_layout().
  for (size_t i = 0; i < base::size(kChannelMap); ++i) {
    layout[kChannelMap[i]] = ChannelOrder(params_.channel_layout(),
                                          static_cast<Channels>(i));
  }
  if (cras_audio_format_set_channel_layout(audio_format, layout) != 0) {
    DLOG(WARNING) << "Error setting channel layout.";
    callback->OnError();
    return;
  }

  uint32_t flags = 0;
  if (params_.effects() & AudioParameters::PlatformEffectsMask::HOTWORD)
    flags = HOTWORD_STREAM;

  unsigned int frames_per_packet = params_.frames_per_buffer();
  cras_stream_params* stream_params = cras_client_stream_params_create(
      stream_direction_,
      frames_per_packet,  // Total latency.
      frames_per_packet,  // Call back when this many ready.
      frames_per_packet,  // Minimum Callback level ignored for capture streams.
      CRAS_STREAM_TYPE_DEFAULT,
      flags,
      this,
      CrasInputStream::SamplesReady,
      CrasInputStream::StreamError,
      audio_format);
  if (!stream_params) {
    DLOG(WARNING) << "Error setting up stream parameters.";
    callback_->OnError();
    callback_ = NULL;
    cras_audio_format_destroy(audio_format);
    return;
  }

  cras_client_stream_params_set_client_type(stream_params,
                                            CRAS_CLIENT_TYPE_CHROME);

  if (UseCrasAec())
    cras_client_stream_params_enable_aec(stream_params);

  // Before starting the stream, save the number of bytes in a frame for use in
  // the callback.
  bytes_per_frame_ = cras_client_format_bytes_per_frame(audio_format);

  // Adding the stream will start the audio callbacks.
  if (cras_client_add_pinned_stream(client_, pin_device_, &stream_id_,
                                    stream_params)) {
    DLOG(WARNING) << "Failed to add the stream.";
    callback_->OnError();
    callback_ = NULL;
  }

  // Mute system audio if requested.
  if (mute_system_audio_ && !cras_client_get_system_muted(client_)) {
    cras_client_set_system_mute(client_, 1);
    mute_done_ = true;
  }

  // Done with config params.
  cras_audio_format_destroy(audio_format);
  cras_client_stream_params_destroy(stream_params);

  started_ = true;
}

void CrasInputStream::Stop() {
  if (!client_)
    return;

  if (!callback_ || !started_)
    return;

  if (mute_system_audio_ && mute_done_) {
    cras_client_set_system_mute(client_, 0);
    mute_done_ = false;
  }

  StopAgc();

  // Removing the stream from the client stops audio.
  cras_client_rm_stream(client_, stream_id_);

  started_ = false;
  callback_ = NULL;
}

// Static callback asking for samples.  Run on high priority thread.
int CrasInputStream::SamplesReady(cras_client* client,
                                  cras_stream_id_t stream_id,
                                  uint8_t* samples,
                                  size_t frames,
                                  const timespec* sample_ts,
                                  void* arg) {
  CrasInputStream* me = static_cast<CrasInputStream*>(arg);
  me->ReadAudio(frames, samples, sample_ts);
  return frames;
}

// Static callback for stream errors.
int CrasInputStream::StreamError(cras_client* client,
                                 cras_stream_id_t stream_id,
                                 int err,
                                 void* arg) {
  CrasInputStream* me = static_cast<CrasInputStream*>(arg);
  me->NotifyStreamError(err);
  return 0;
}

void CrasInputStream::ReadAudio(size_t frames,
                                uint8_t* buffer,
                                const timespec* sample_ts) {
  DCHECK(callback_);

  // Update the AGC volume level once every second. Note that, |volume| is
  // also updated each time SetVolume() is called through IPC by the
  // render-side AGC.
  double normalized_volume = 0.0;
  GetAgcVolume(&normalized_volume);

  // Don't just assume sample_ts is from the same clock as base::TimeTicks (it
  // is not). Instead, convert it to a latency with a cras utility function
  // (guaranteed to use the same clock) and apply that latency to
  // TimeTicks::Now().
  timespec latency_ts = {0, 0};
  cras_client_calc_capture_latency(sample_ts, &latency_ts);

  const base::TimeDelta delay =
      std::max(base::TimeDelta::FromTimeSpec(latency_ts), base::TimeDelta());

  // The delay says how long ago the capture was, so we subtract the delay from
  // Now() to find the capture time.
  const base::TimeTicks capture_time = base::TimeTicks::Now() - delay;

  audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(
      reinterpret_cast<int16_t*>(buffer), audio_bus_->frames());
  callback_->OnData(audio_bus_.get(), capture_time, normalized_volume);
}

void CrasInputStream::NotifyStreamError(int err) {
  if (callback_)
    callback_->OnError();
}

double CrasInputStream::GetMaxVolume() {
  return 1.0f;
}

void CrasInputStream::SetVolume(double volume) {
  DCHECK(client_);

  // Set the volume ratio to CRAS's softare and stream specific gain.
  input_volume_ = volume;
  cras_client_set_stream_volume(client_, stream_id_, input_volume_);

  // Update the AGC volume level based on the last setting above. Note that,
  // the volume-level resolution is not infinite and it is therefore not
  // possible to assume that the volume provided as input parameter can be
  // used directly. Instead, a new query to the audio hardware is required.
  // This method does nothing if AGC is disabled.
  UpdateAgcVolume();
}

double CrasInputStream::GetVolume() {
  if (!client_)
    return 0.0;

  return input_volume_;
}

bool CrasInputStream::IsMuted() {
  return false;
}

void CrasInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

}  // namespace media
