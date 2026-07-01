// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/cras_unified.h"

#include <inttypes.h>

#include <algorithm>
#include <array>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/typed_macros.h"
#include "media/audio/cras/audio_manager_cras_base.h"
#include "media/audio/cras/cras_util.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_sample_types.h"
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

constexpr auto kSampleFormat = SND_PCM_FORMAT_S16;

}  // namespace

// Thread-safe shim between the libcras real-time callback thread and
// CrasUnifiedStream. libcras invokes the static callbacks with this proxy as
// the user argument; the proxy forwards to the stream only while holding
// `lock_` and only if the stream is still attached.
// CrasUnifiedStream::Stop()/Close() call Detach(), which acquires the same
// lock and nulls the pointer, so it both blocks until any in-flight callback
// finishes and prevents future callbacks from reaching a stream that is about
// to be (or has been) freed. This mirrors the fix for the input stream (see
// CrasAudioInputStreamProxy).
class CrasUnifiedStreamProxy {
 public:
  explicit CrasUnifiedStreamProxy(CrasUnifiedStream* stream)
      : stream_(stream) {}

  CrasUnifiedStreamProxy(const CrasUnifiedStreamProxy&) = delete;
  CrasUnifiedStreamProxy& operator=(const CrasUnifiedStreamProxy&) = delete;

  void Detach() {
    base::AutoLock auto_lock(lock_);
    stream_ = nullptr;
  }

  // (Re-)attaches the proxy to its stream for the libcras stream identified by
  // `stream_id`. Used when a pooled output stream is restarted via Stop() then
  // Start() without an intervening Close(). Only callbacks with this exact
  // `stream_id` are forwarded, so a callback from a previous run delayed
  // across a restart (and therefore carries the previous stream id) is dropped
  // instead of writing into a stale buffer for the new run.
  void Attach(CrasUnifiedStream* stream, cras_stream_id_t stream_id) {
    base::AutoLock auto_lock(lock_);
    stream_ = stream;
    active_stream_id_ = stream_id;
  }

  int UnifiedCallback(struct libcras_stream_cb_data* data) {
    cras_stream_id_t stream_id;
    libcras_stream_cb_data_get_stream_id(data, &stream_id);
    base::AutoLock auto_lock(lock_);
    // Forward only if the stream is still attached AND this callback belongs to
    // the currently active libcras stream. A callback from a previous run of a
    // pooled (restarted) stream that is delayed past the re-attach carries a
    // different id and is dropped, rather than writing into a stale buffer.
    if (stream_ && stream_id == active_stream_id_) {
      return stream_->OnUnifiedCallback(data);
    }
    // The stream has been detached, or this is a stale callback from a previous
    // run; report that no frames were filled.
    return 0;
  }

  int StreamError(cras_client* client, cras_stream_id_t stream_id, int err) {
    base::AutoLock auto_lock(lock_);
    if (stream_ && stream_id == active_stream_id_) {
      return stream_->OnStreamError(client, stream_id, err);
    }
    return 0;
  }

 private:
  base::Lock lock_;
  raw_ptr<CrasUnifiedStream> stream_ GUARDED_BY(lock_);
  // The id of the libcras stream the proxy is currently attached to. A callback
  // for any other id (e.g. a late callback from a previous run of a pooled
  // stream) is dropped. Only meaningful while `stream_` is non-null.
  cras_stream_id_t active_stream_id_ GUARDED_BY(lock_) = 0;
};

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
  // The proxy lives for the entire lifetime of this stream; it only ever guards
  // `this`, which never changes. Recreating it per Start() would be unsafe:
  // pooled output streams are restarted in place (Stop() then Start() without
  // Close(), see AudioOutputDispatcherImpl), and a late libcras callback from a
  // previous run could still reference the old proxy.
  proxy_ = std::make_unique<CrasUnifiedStreamProxy>(this);
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
    client_ = nullptr;
    return false;
  }

  if (libcras_client_connect_timeout(client_, kCrasConnectTimeoutMs)) {
    LOG(WARNING) << "Couldn't connect CRAS client.\n";
    ReportStreamOpenResult(
        StreamOpenResult::kCallbackOpenCannotConnectToCrasClient);
    libcras_client_destroy(client_.ExtractAsDangling());
    client_ = nullptr;
    return false;
  }

  // Then start running the client.
  if (libcras_client_run_thread(client_)) {
    LOG(WARNING) << "Couldn't run CRAS client.\n";
    ReportStreamOpenResult(StreamOpenResult::kCallbackOpenCannotRunCrasClient);
    libcras_client_destroy(client_.ExtractAsDangling());
    client_ = nullptr;
    return false;
  }
  ReportStreamOpenResult(StreamOpenResult::kCallbackOpenSuccess);

  return true;
}

void CrasUnifiedStream::Close() {
  // Close() may be called without a preceding Stop(); detach the proxy here too
  // so the libcras thread can no longer reach this stream before it is freed.
  if (proxy_) {
    proxy_->Detach();
  }

  if (client_) {
    libcras_client_stop(client_);
    libcras_client_destroy(client_.ExtractAsDangling());
    client_ = nullptr;
  }

  // The libcras client (and its callback thread) has been stopped above, so no
  // callback can be using the proxy anymore; it is now safe to destroy it.
  if (proxy_) {
    proxy_.reset();
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
  static constexpr int kUnsupportedChannel = -4;
  static const int kChannelMap[] = {
      CRAS_CH_FL, CRAS_CH_FR, CRAS_CH_FC, CRAS_CH_LFE, CRAS_CH_RL, CRAS_CH_RR,
      CRAS_CH_FLC, CRAS_CH_FRC, CRAS_CH_RC, CRAS_CH_SL, CRAS_CH_SR,
      // CRAS doesn't currently define explicit mappings for all channels.
      kUnsupportedChannel, kUnsupportedChannel, kUnsupportedChannel,
      kUnsupportedChannel, kUnsupportedChannel, kUnsupportedChannel,
      kUnsupportedChannel};
  static_assert(std::size(kChannelMap) == CHANNELS_MAX + 1,
                "kChannelMap array size should match");

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

  // Register the persistent thread-safe proxy (rather than `this`) as the
  // libcras callback user argument, so that a late real-time callback cannot
  // reach this stream after it has been stopped or freed. See
  // CrasAudioInputStreamProxy for the input-stream equivalent. The proxy is
  // (re-)attached further below, only once the stream is fully initialized and
  // right before callbacks can start, so a forwarded callback never observes a
  // half-(re)initialized stream (e.g. a null `peak_detector_`).
  int rc = libcras_stream_params_set(
      stream_params, stream_direction_, frames_per_packet * 2,
      frames_per_packet, CRAS_STREAM_TYPE_DEFAULT, manager_->GetClientType(), 0,
      proxy_.get(), CrasUnifiedStream::UnifiedCallback,
      CrasUnifiedStream::StreamError, params_.sample_rate(), kSampleFormat,
      params_.channels());

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
  std::array<int8_t, CRAS_CH_MAX> layout;
  layout.fill(-1);

  // Converts to CRAS defined channels. ChannelOrder will return -1
  // for channels that does not present in params_.channel_layout().
  for (size_t i = 0; i < std::size(kChannelMap); ++i) {
    if (kChannelMap[i] != kUnsupportedChannel) {
      layout.at(kChannelMap[i]) =
          ChannelOrder(params_.channel_layout(), static_cast<Channels>(i));
    }
  }

  rc = libcras_stream_params_set_channel_layout(stream_params, CRAS_CH_MAX,
                                                layout.data());
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

  // Adding the stream will start the audio callbacks requesting data. libcras
  // assigns the new stream's id into `stream_id_` here. `peak_detector_` was
  // (re)created just above, so the stream is fully initialized before any
  // callback can be forwarded.
  if (libcras_client_add_pinned_stream(client_, pin_device_, &stream_id_,
                                       stream_params)) {
    LOG(WARNING) << "Failed to add the stream.";
    ReportStreamStartResult(
        StreamStartResult::kCallbackStartAddingStreamFailed);
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
    libcras_stream_params_destroy(stream_params);
    return;
  }

  // Attach the proxy now that the libcras stream id is known, so it forwards
  // callbacks for exactly this stream. Attaching after add_pinned_stream()
  // (rather than before) means a delayed callback from a previous run of this
  // pooled stream -- carrying the previous id -- is dropped even though the
  // stream object is reused. Any callback that arrives in the brief window
  // before this point is dropped while the proxy is still detached, costing at
  // most an initial buffer of silence.
  proxy_->Attach(this, stream_id_);

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

  // Instantly fence off the libcras real-time thread: after Detach() returns no
  // further callback can reach this stream, and Detach() blocks until any
  // in-flight callback has finished, so the teardown below is safe.
  if (proxy_) {
    proxy_->Detach();
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

// Static callback asking for samples. Runs on the libcras real-time thread.
int CrasUnifiedStream::UnifiedCallback(struct libcras_stream_cb_data* data) {
  void* usr_arg;
  libcras_stream_cb_data_get_usr_arg(data, &usr_arg);
  CrasUnifiedStreamProxy* proxy = static_cast<CrasUnifiedStreamProxy*>(usr_arg);
  return proxy->UnifiedCallback(data);
}

int CrasUnifiedStream::OnUnifiedCallback(struct libcras_stream_cb_data* data) {
  unsigned int frames;
  uint8_t* buf;
  struct timespec latency;
  struct timespec underrun_duration_ts;
  cras_stream_id_t stream_id;
  libcras_stream_cb_data_get_frames(data, &frames);
  libcras_stream_cb_data_get_buf(data, &buf);
  libcras_stream_cb_data_get_latency(data, &latency);
  libcras_stream_cb_data_get_underrun_duration(data, &underrun_duration_ts);
  libcras_stream_cb_data_get_stream_id(data, &stream_id);
  TRACE_EVENT_BEGIN(
      "audio", "CrasUnifiedStream::UnifiedCallback",
      perfetto::Flow::ProcessScoped(static_cast<uint64_t>(stream_id)));

  base::TimeDelta underrun_duration =
      base::TimeDelta::FromTimeSpec(underrun_duration_ts);
  CalculateAudioGlitches(underrun_duration);
  static_assert(kSampleFormat == SND_PCM_FORMAT_S16,
                "cras_unified.cc assumes SND_PCM_FORMAT_S16");
  // SAFETY: CRAS guarantees that `buf` points to a buffer with at least
  // `frames` capacity. Since the stream is configured with S16 format, the
  // buffer has space for `frames * channels` of `int16_t` samples.
  auto buffer_span = UNSAFE_BUFFERS(base::span<int16_t>(
      reinterpret_cast<int16_t*>(buf), frames * params_.channels()));
  uint32_t filled_frames = WriteAudio(buffer_span, &latency);
  TRACE_EVENT_END("audio", [&](perfetto::EventContext ctx) {
    auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
    auto* data = event->set_chromeos_cras_unified();
    data->set_requested_frames(frames);
    data->set_filled_frames(filled_frames);
  });
  return filled_frames;
}

// Static callback for stream errors. Runs on the libcras real-time thread.
int CrasUnifiedStream::StreamError(cras_client* client,
                                   cras_stream_id_t stream_id,
                                   int err,
                                   void* arg) {
  CrasUnifiedStreamProxy* proxy = static_cast<CrasUnifiedStreamProxy*>(arg);
  return proxy->StreamError(client, stream_id, err);
}

int CrasUnifiedStream::OnStreamError(cras_client* client,
                                     cras_stream_id_t stream_id,
                                     int err) {
  NotifyStreamError(err);
  return 0;
}

uint32_t CrasUnifiedStream::WriteAudio(base::span<int16_t> buffer,
                                       const timespec* latency_ts) {
  DCHECK_EQ(buffer.size(), static_cast<size_t>(output_bus_->frames() *
                                               output_bus_->channels()));
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
  static_assert(kSampleFormat == SND_PCM_FORMAT_S16,
                "cras_unified.cc assumes SND_PCM_FORMAT_S16");
  output_bus_->ToInterleavedPartial<SignedInt16SampleTypeTraits>(
      0, buffer.first(
             static_cast<size_t>(frames_filled * output_bus_->channels())));

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
