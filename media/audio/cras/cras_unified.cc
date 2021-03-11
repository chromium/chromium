// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/cras_unified.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/audio/cras/audio_manager_cras_base.h"

namespace media {

namespace {

int GetDevicePin(AudioManagerCrasBase* manager, const std::string& device_id) {
  if (!manager->IsDefault(device_id, false)) {
    uint64_t cras_node_id;
    base::StringToUint64(device_id, &cras_node_id);
    return dev_index_of(cras_node_id);
  }
  return NO_DEVICE;
}

}  // namespace

// Overview of operation:
// 1) An object of CrasUnifiedStream is created by the AudioManager
// factory: audio_man->MakeAudioStream().
// 2) Next some thread will call Open(), at that point a client is created and
// configured for the correct format and sample rate.
// 3) Then Start(source) is called and a stream is added to the CRAS client
// which will create its own thread that periodically calls the source for more
// data as buffers are being consumed.
// 4) When finished Stop() is called, which is handled by stopping the stream.
// 5) Finally Close() is called. It cleans up and notifies the audio manager,
// which likely will destroy this object.
//
// Simplified data flow for output only streams:
//
//   +-------------+                  +------------------+
//   | CRAS Server |                  | Chrome Client    |
//   +------+------+    Add Stream    +---------+--------+
//          |<----------------------------------|
//          |                                   |
//          | Near out of samples, request more |
//          |---------------------------------->|
//          |                                   |  UnifiedCallback()
//          |                                   |  WriteAudio()
//          |                                   |
//          |  buffer_frames written to shm     |
//          |<----------------------------------|
//          |                                   |
//         ...  Repeats for each block.        ...
//          |                                   |
//          |                                   |
//          |  Remove stream                    |
//          |<----------------------------------|
//          |                                   |
//
// For Unified streams the Chrome client is notified whenever buffer_frames have
// been captured.  For Output streams the client is notified a few milliseconds
// before the hardware buffer underruns and fills the buffer with another block
// of audio.

CrasUnifiedStream::CrasUnifiedStream(const AudioParameters& params,
                                     AudioManagerCrasBase* manager,
                                     const std::string& device_id)
    : client_(NULL),
      stream_id_(0),
      params_(params),
      bytes_per_frame_(0),
      is_playing_(false),
      volume_(1.0),
      manager_(manager),
      source_callback_(NULL),
      output_bus_(AudioBus::Create(params)),
      stream_direction_(CRAS_STREAM_OUTPUT),
      pin_device_(GetDevicePin(manager, device_id)) {
  DCHECK(manager_);
  DCHECK_GT(params_.channels(), 0);
}

CrasUnifiedStream::~CrasUnifiedStream() {
  DCHECK(!is_playing_);
}

bool CrasUnifiedStream::Open() {
  // Sanity check input values.
  if (params_.sample_rate() <= 0) {
    LOG(WARNING) << "Unsupported audio frequency.";
    return false;
  }

  // Create the client and connect to the CRAS server.
  if (cras_client_create(&client_)) {
    LOG(WARNING) << "Couldn't create CRAS client.\n";
    client_ = NULL;
    return false;
  }

  if (cras_client_connect(client_)) {
    LOG(WARNING) << "Couldn't connect CRAS client.\n";
    cras_client_destroy(client_);
    client_ = NULL;
    return false;
  }

  // Then start running the client.
  if (cras_client_run_thread(client_)) {
    LOG(WARNING) << "Couldn't run CRAS client.\n";
    cras_client_destroy(client_);
    client_ = NULL;
    return false;
  }

  return true;
}

void CrasUnifiedStream::Close() {
  if (client_) {
    cras_client_stop(client_);
    cras_client_destroy(client_);
    client_ = NULL;
  }

  // Signal to the manager that we're closed and can be removed.
  // Should be last call in the method as it deletes "this".
  manager_->ReleaseOutputStream(this);
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void CrasUnifiedStream::Flush() {}

void CrasUnifiedStream::Start(AudioSourceCallback* callback) {
  CHECK(callback);

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

  source_callback_ = callback;

  // Only start if we can enter the playing state.
  if (is_playing_)
    return;

  // Prepare |audio_format| and |stream_params| for the stream we
  // will create.
  cras_audio_format* audio_format = cras_audio_format_create(
      SND_PCM_FORMAT_S16, params_.sample_rate(), params_.channels());
  if (!audio_format) {
    LOG(WARNING) << "Error setting up audio parameters.";
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
    return;
  }

  // Initialize channel layout to all -1 to indicate that none of
  // the channels is set in the layout.
  int8_t layout[CRAS_CH_MAX] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

  // Converts to CRAS defined channels. ChannelOrder will return -1
  // for channels that does not present in params_.channel_layout().
  for (size_t i = 0; i < base::size(kChannelMap); ++i)
    layout[kChannelMap[i]] = ChannelOrder(params_.channel_layout(),
                                          static_cast<Channels>(i));

  if (cras_audio_format_set_channel_layout(audio_format, layout)) {
    LOG(WARNING) << "Error setting channel layout.";
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
    return;
  }

  cras_stream_params* stream_params = cras_client_unified_params_create(
      stream_direction_,
      params_.frames_per_buffer(),
      CRAS_STREAM_TYPE_DEFAULT,
      0,
      this,
      CrasUnifiedStream::UnifiedCallback,
      CrasUnifiedStream::StreamError,
      audio_format);
  if (!stream_params) {
    LOG(WARNING) << "Error setting up stream parameters.";
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
    cras_audio_format_destroy(audio_format);
    return;
  }

  cras_client_stream_params_set_client_type(stream_params,
                                            manager_->GetClientType());

  // Before starting the stream, save the number of bytes in a frame for use in
  // the callback.
  bytes_per_frame_ = cras_client_format_bytes_per_frame(audio_format);

  // Adding the stream will start the audio callbacks requesting data.
  if (cras_client_add_pinned_stream(client_, pin_device_, &stream_id_,
                                    stream_params)) {
    LOG(WARNING) << "Failed to add the stream.";
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
    cras_audio_format_destroy(audio_format);
    cras_client_stream_params_destroy(stream_params);
    return;
  }

  // Set initial volume.
  cras_client_set_stream_volume(client_, stream_id_, volume_);

  // Done with config params.
  cras_audio_format_destroy(audio_format);
  cras_client_stream_params_destroy(stream_params);

  is_playing_ = true;
}

void CrasUnifiedStream::Stop() {
  if (!client_)
    return;

  // Removing the stream from the client stops audio.
  cras_client_rm_stream(client_, stream_id_);

  is_playing_ = false;
}

void CrasUnifiedStream::SetVolume(double volume) {
  if (!client_)
    return;
  volume_ = static_cast<float>(volume);
  cras_client_set_stream_volume(client_, stream_id_, volume_);
}

void CrasUnifiedStream::GetVolume(double* volume) {
  *volume = volume_;
}

// Static callback asking for samples.
int CrasUnifiedStream::UnifiedCallback(cras_client* client,
                                       cras_stream_id_t stream_id,
                                       uint8_t* input_samples,
                                       uint8_t* output_samples,
                                       unsigned int frames,
                                       const timespec* input_ts,
                                       const timespec* output_ts,
                                       void* arg) {
  CrasUnifiedStream* me = static_cast<CrasUnifiedStream*>(arg);
  return me->DispatchCallback(frames,
                              input_samples,
                              output_samples,
                              input_ts,
                              output_ts);
}

// Static callback for stream errors.
int CrasUnifiedStream::StreamError(cras_client* client,
                                   cras_stream_id_t stream_id,
                                   int err,
                                   void* arg) {
  CrasUnifiedStream* me = static_cast<CrasUnifiedStream*>(arg);
  me->NotifyStreamError(err);
  return 0;
}

// Calls the appropriate rendering function for this type of stream.
uint32_t CrasUnifiedStream::DispatchCallback(size_t frames,
                                             uint8_t* input_samples,
                                             uint8_t* output_samples,
                                             const timespec* input_ts,
                                             const timespec* output_ts) {
  switch (stream_direction_) {
    case CRAS_STREAM_OUTPUT:
      return WriteAudio(frames, output_samples, output_ts);
    case CRAS_STREAM_INPUT:
      NOTREACHED() << "CrasUnifiedStream doesn't support input streams.";
      return 0;
    default:
      break;
  }

  return 0;
}

uint32_t CrasUnifiedStream::WriteAudio(size_t frames,
                                       uint8_t* buffer,
                                       const timespec* sample_ts) {
  DCHECK_EQ(frames, static_cast<size_t>(output_bus_->frames()));

  // Determine latency and pass that on to the source.
  timespec latency_ts  = {0, 0};
  cras_client_calc_playback_latency(sample_ts, &latency_ts);

  // Treat negative latency (if we are too slow to render) as 0.
  const base::TimeDelta delay =
      std::max(base::TimeDelta::FromTimeSpec(latency_ts), base::TimeDelta());

  int frames_filled = source_callback_->OnMoreData(
      delay, base::TimeTicks::Now(), 0, output_bus_.get());

  // Note: If this ever changes to output raw float the data must be clipped and
  // sanitized since it may come from an untrusted source such as NaCl.
  output_bus_->ToInterleaved<SignedInt16SampleTypeTraits>(
      frames_filled, reinterpret_cast<int16_t*>(buffer));

  return frames_filled;
}

void CrasUnifiedStream::NotifyStreamError(int err) {
  // This will remove the stream from the client.
  // TODO(dalecurtis): Consider sending a translated |err| code.
  if (source_callback_)
    source_callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
}

}  // namespace media
