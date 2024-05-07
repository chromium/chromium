// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/cras_unified.h"

#include <inttypes.h>

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/typed_macros.h"
#include "media/audio/cras/audio_manager_cras_base.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

namespace {

// Used to log errors in `CrasUnifiedStream::Open`.
enum class StreamOpenResult {
  kCallbackOpenSuccess = 0,
  kCallbackOpenUnsupportedAudioFrequency = 1,
  kCallbackOpenCannotCreateCrasClient = 2,
  kCallbackOpenCannotConnectToCrasClient = 3,
  kCallbackOpenCannotRunCrasClient = 4,
  kMaxValue = kCallbackOpenCannotRunCrasClient
};

// Used to log errors in `CrasUnifiedStream::Start`.
enum class StreamStartResult {
  kCallbackStartSuccess = 0,
  kCallbackStartCreatingStreamParamsFailed = 1,
  kCallbackStartSettingUpStreamParamsFailed = 2,
  kCallbackStartSettingUpChannelLayoutFailed = 3,
  kCallbackStartAddingStreamFailed = 4,
  kMaxValue = kCallbackStartAddingStreamFailed
};

void ReportStreamOpenResult(StreamOpenResult result) {
  base::UmaHistogramEnumeration("Media.Audio.CrasUnifiedStreamOpenSuccess",
                                result);
}

void ReportStreamStartResult(StreamStartResult result) {
  base::UmaHistogramEnumeration("Media.Audio.CrasUnifiedStreamStartSuccess",
                                result);
}

void ReportNotifyStreamErrors(int err) {
  base::UmaHistogramSparse("Media.Audio.CrasUnifiedStreamNotifyStreamError",
                           err);
}

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

CrasUnifiedStream::CrasUnifiedStream(
    const AudioParameters& params,
    AudioManagerCrasBase* manager,
    const std::string& device_id,
    const AudioManager::LogCallback& log_callback)
    : params_(params),
      manager_(manager),
      output_bus_(AudioBus::Create(params)),
      pin_device_(GetDevicePin(manager, device_id)),
      glitch_reporter_(SystemGlitchReporter::StreamType::kRender),
      log_callback_(std::move(log_callback)) {
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
    ReportStreamOpenResult(
        StreamOpenResult::kCallbackOpenUnsupportedAudioFrequency);
    return false;
  }

  // Create the client and connect to the CRAS server.
  client_ = libcras_client_create();
  if (!client_) {
    LOG(WARNING) << "Couldn't create CRAS client.\n";
    ReportStreamOpenResult(
        StreamOpenResult::kCallbackOpenCannotCreateCrasClient);
    client_ = NULL;
    return false;
  }

  if (libcras_client_connect(client_)) {
    LOG(WARNING) << "Couldn't connect CRAS client.\n";
    ReportStreamOpenResult(
        StreamOpenResult::kCallbackOpenCannotConnectToCrasClient);
    libcras_client_destroy(client_);
    client_ = NULL;
    return false;
  }

  // Then start running the client.
  if (libcras_client_run_thread(client_)) {
    LOG(WARNING) << "Couldn't run CRAS client.\n";
    ReportStreamOpenResult(StreamOpenResult::kCallbackOpenCannotRunCrasClient);
    libcras_client_destroy(client_);
    client_ = NULL;
    return false;
  }
  ReportStreamOpenResult(StreamOpenResult::kCallbackOpenSuccess);

  return true;
}

void CrasUnifiedStream::Close() {
  if (client_) {
    libcras_client_stop(client_);
    libcras_client_destroy(client_);
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
      CRAS_CH_FL,  CRAS_CH_FR,  CRAS_CH_FC, CRAS_CH_LFE, CRAS_CH_RL, CRAS_CH_RR,
      CRAS_CH_FLC, CRAS_CH_FRC, CRAS_CH_RC, CRAS_CH_SL,  CRAS_CH_SR};

  source_callback_ = callback;

  // Only start if we can enter the playing state.
  if (is_playing_) {
    return;
  }

  struct libcras_stream_params* stream_params = libcras_stream_params_create();
  if (!stream_params) {
    DLOG(ERROR) << "Error creating stream params.";
    ReportStreamStartResult(
        StreamStartResult::kCallbackStartCreatingStreamParamsFailed);
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
  }

  unsigned int frames_per_packet = params_.frames_per_buffer();

  int rc = libcras_stream_params_set(
      stream_params, stream_direction_, frames_per_packet * 2,
      frames_per_packet, CRAS_STREAM_TYPE_DEFAULT, manager_->GetClientType(), 0,
      this, CrasUnifiedStream::UnifiedCallback, CrasUnifiedStream::StreamError,
      params_.sample_rate(), SND_PCM_FORMAT_S16, params_.channels());

  if (rc) {
    LOG(WARNING) << "Error setting up stream parameters.";
    ReportStreamStartResult(
        StreamStartResult::kCallbackStartSettingUpStreamParamsFailed);
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
    libcras_stream_params_destroy(stream_params);
    return;
  }

  // Initialize channel layout to all -1 to indicate that none of
  // the channels is set in the layout.
  int8_t layout[CRAS_CH_MAX] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

  // Converts to CRAS defined channels. ChannelOrder will return -1
  // for channels that does not present in params_.channel_layout().
  for (size_t i = 0; i < std::size(kChannelMap); ++i) {
    layout[kChannelMap[i]] =
        ChannelOrder(params_.channel_layout(), static_cast<Channels>(i));
  }

  rc = libcras_stream_params_set_channel_layout(stream_params, CRAS_CH_MAX,
                                                layout);
  if (rc) {
    DLOG(WARNING) << "Error setting up the channel layout.";
    ReportStreamStartResult(
        StreamStartResult::kCallbackStartSettingUpChannelLayoutFailed);
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
    libcras_stream_params_destroy(stream_params);
    return;
  }

  // Recreate `peak_detector_` every time we create a new stream, to
  // avoid ThreadChecker DCHECKs.
  peak_detector_ = std::make_unique<AmplitudePeakDetector>(base::BindRepeating(
      &AudioManager::TraceAmplitudePeak, base::Unretained(manager_),
      /*trace_start=*/false));

  // Adding the stream will start the audio callbacks requesting data.
  if (libcras_client_add_pinned_stream(client_, pin_device_, &stream_id_,
                                       stream_params)) {
    LOG(WARNING) << "Failed to add the stream.";
    ReportStreamStartResult(
        StreamStartResult::kCallbackStartAddingStreamFailed);
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
    libcras_stream_params_destroy(stream_params);
    return;
  }

  // Set initial volume.
  libcras_client_set_stream_volume(client_, stream_id_, volume_);

  // Done with config params.
  libcras_stream_params_destroy(stream_params);

  is_playing_ = true;

  ReportStreamStartResult(StreamStartResult::kCallbackStartSuccess);
}

void CrasUnifiedStream::Stop() {
  if (!client_) {
    return;
  }

  // Removing the stream from the client stops audio.
  libcras_client_rm_stream(client_, stream_id_);

  peak_detector_.reset();

  ReportAndResetStats();

  is_playing_ = false;
}

void CrasUnifiedStream::SetVolume(double volume) {
  if (!client_) {
    return;
  }
  volume_ = static_cast<float>(volume);
  libcras_client_set_stream_volume(client_, stream_id_, volume_);
}

void CrasUnifiedStream::GetVolume(double* volume) {
  *volume = volume_;
}

// Static callback asking for samples.
int CrasUnifiedStream::UnifiedCallback(struct libcras_stream_cb_data* data) {
  unsigned int frames;
  uint8_t* buf;
  struct timespec latency;
  void* usr_arg;
  struct timespec underrun_duration_ts;
  cras_stream_id_t stream_id;
  libcras_stream_cb_data_get_frames(data, &frames);
  libcras_stream_cb_data_get_buf(data, &buf);
  libcras_stream_cb_data_get_latency(data, &latency);
  libcras_stream_cb_data_get_usr_arg(data, &usr_arg);
  libcras_stream_cb_data_get_underrun_duration(data, &underrun_duration_ts);
  libcras_stream_cb_data_get_stream_id(data, &stream_id);
  TRACE_EVENT_BEGIN(
      "audio", "CrasUnifiedStream::UnifiedCallback",
      perfetto::Flow::ProcessScoped(static_cast<uint64_t>(stream_id)));

  CrasUnifiedStream* me = static_cast<CrasUnifiedStream*>(usr_arg);
  base::TimeDelta underrun_duration =
      base::TimeDelta::FromTimeSpec(underrun_duration_ts);
  me->CalculateAudioGlitches(underrun_duration);
  uint32_t filled_frames = me->WriteAudio(frames, buf, &latency);
  TRACE_EVENT_END("audio", [&](perfetto::EventContext ctx) {
    auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
    auto* data = event->set_chromeos_cras_unified();
    data->set_requested_frames(frames);
    data->set_filled_frames(filled_frames);
  });
  return filled_frames;
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

uint32_t CrasUnifiedStream::WriteAudio(size_t frames,
                                       uint8_t* buffer,
                                       const timespec* latency_ts) {
  DCHECK_EQ(frames, static_cast<size_t>(output_bus_->frames()));
  const base::TimeDelta latency = base::TimeDelta::FromTimeSpec(*latency_ts);
  TRACE_EVENT("audio", "CrasUnifiedStream::WriteAudio",
              [&](perfetto::EventContext ctx) {
                auto* event =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                auto* data = event->set_chromeos_cras_unified();
                data->set_sample_rate(params_.sample_rate());
                data->set_latency_us(latency.InMicroseconds());
              });

  // Treat negative latency (if we are too slow to render) as 0.
  const base::TimeDelta delay = std::max(latency, base::TimeDelta());
  const AudioGlitchInfo glitch_info = glitch_info_accumulator_.GetAndReset();

  UMA_HISTOGRAM_COUNTS_1000("Media.Audio.Render.SystemDelay",
                            delay.InMilliseconds());
  int frames_filled =
      source_callback_->OnMoreData(BoundedDelay(delay), base::TimeTicks::Now(),
                                   glitch_info, output_bus_.get());

  peak_detector_->FindPeak(output_bus_.get());

  // Note: If this ever changes to output raw float the data must be clipped and
  // sanitized since it may come from an untrusted source such as NaCl.
  output_bus_->ToInterleaved<SignedInt16SampleTypeTraits>(
      frames_filled, reinterpret_cast<int16_t*>(buffer));

  return frames_filled;
}

void CrasUnifiedStream::NotifyStreamError(int err) {
  // This will remove the stream from the client.
  // TODO(dalecurtis): Consider sending a translated |err| code.
  ReportNotifyStreamErrors(err);
  if (source_callback_) {
    source_callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
  }
}

void CrasUnifiedStream::ReportAndResetStats() {
  SystemGlitchReporter::Stats stats =
      glitch_reporter_.GetLongTermStatsAndReset();

  if (!log_callback_.is_null()) {
    std::string log_message = base::StringPrintf(
        "CRAS out: (num_glitches_detected=[%d], cumulative_audio_lost=[%" PRId64
        " ms],largest_glitch=[%" PRId64 " ms])",
        stats.glitches_detected, stats.total_glitch_duration.InMilliseconds(),
        stats.largest_glitch_duration.InMilliseconds());

    log_callback_.Run(log_message);
    if (stats.glitches_detected != 0) {
      DLOG(WARNING) << log_message;
    }
  }

  last_underrun_duration_ = base::TimeDelta();
  glitch_info_accumulator_.GetAndReset();
}

void CrasUnifiedStream::CalculateAudioGlitches(
    base::TimeDelta underrun_duration) {
  TRACE_EVENT(
      "audio", "CrasUnifiedStream::CalculateAudioGlitches",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chromeos_cras_unified();
        data->set_underrun_duration_us(underrun_duration.InMicroseconds());
        data->set_last_underrun_duration_us(
            last_underrun_duration_.InMicroseconds());
      });
  // |underrun_duration| obtained from callback is the cumulative value
  // of the filled zero frames of the whole stream. Calculate
  // the filled zero frames duration this callback.
  DCHECK_GE(underrun_duration, last_underrun_duration_);
  base::TimeDelta underrun_glitch_duration =
      underrun_duration - last_underrun_duration_;

  glitch_reporter_.UpdateStats(underrun_glitch_duration);

  if (underrun_glitch_duration.is_positive()) {
    glitch_info_accumulator_.Add(AudioGlitchInfo::SingleBoundedSystemGlitch(
        underrun_glitch_duration, AudioGlitchInfo::Direction::kRender));
    TRACE_EVENT_INSTANT("audio", "glitch", [&](perfetto::EventContext ctx) {
      auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
      auto* data = event->set_chromeos_cras_unified();
      data->set_underrun_glitch_duration_us(
          underrun_glitch_duration.InMicroseconds());
    });
  }
  last_underrun_duration_ = underrun_duration;
}

}  // namespace media
