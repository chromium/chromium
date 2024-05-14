// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/cras_input.h"

#include <inttypes.h>
#include <math.h>

#include <algorithm>
#include <ctime>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/cras/audio_manager_cras_base.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/media_switches.h"

namespace media {

namespace {

// Used to log errors in `CrasInputStream::Open`.
enum class StreamOpenResult {
  kCallbackOpenSuccess = 0,
  kCallbackOpenClientAlreadyOpen = 1,
  kCallbackOpenUnsupportedAudioFrequency = 2,
  kCallbackOpenUnsupportedAudioFormat = 3,
  kCallbackOpenCrasClientCreationFailed = 4,
  kCallbackOpenCannotConnectToCrasClient = 5,
  kCallbackOpenCannotRunCrasClient = 6,
  kCallbackOpenCannotSynchronizeData = 7,
  kCallbackOpenCannotFindLoopbackDevice = 8,
  kMaxValue = kCallbackOpenCannotFindLoopbackDevice
};

// Used to log errors in `CrasInputStream::Start`.
enum class StreamStartResult {
  kCallbackStartSuccess = 0,
  kCallbackStartErrorCreatingStreamParameters = 1,
  kCallbackStartErrorSettingUpStreamParameters = 2,
  kCallbackStartErrorSettingUpChannelLayout = 3,
  kCallbackStartFailedAddingStream = 4,
  kMaxValue = kCallbackStartFailedAddingStream
};

void ReportStreamOpenResult(StreamOpenResult result) {
  base::UmaHistogramEnumeration("Media.Audio.CrasInputStreamOpenSuccess",
                                result);
}

void ReportStreamStartResult(StreamStartResult result) {
  base::UmaHistogramEnumeration("Media.Audio.CrasInputStreamStartSuccess",
                                result);
}

void ReportNotifyStreamErrors(int err) {
  base::UmaHistogramSparse("Media.Audio.CrasInputStreamNotifyStreamError", err);
}

static constexpr char kVoiceIsolationEffectStateHistogramName[] =
    "Cras.StreamEffectState.VoiceIsolation";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Used to log stream effects in `CrasInputStream::Start`.
enum class StreamEffectState {
  kForceDisable = 0,
  kForceEnable = 1,
  kPlatformDefault = 2,
  kMaxValue = kPlatformDefault
};

void RecordVoiceIsolationState(StreamEffectState state) {
  base::UmaHistogramEnumeration(kVoiceIsolationEffectStateHistogramName, state);
}

}  // namespace

CrasInputStream::CrasInputStream(const AudioParameters& params,
                                 AudioManagerCrasBase* manager,
                                 const std::string& device_id,
                                 const AudioManager::LogCallback& log_callback)
    : audio_manager_(manager),
      params_(params),
      is_loopback_(AudioDeviceDescription::IsLoopbackDevice(device_id)),
      is_loopback_without_chrome_(
          device_id == AudioDeviceDescription::kLoopbackWithoutChromeId),
      mute_system_audio_(device_id ==
                         AudioDeviceDescription::kLoopbackWithMuteDeviceId),
#if DCHECK_IS_ON()
      recording_enabled_(false),
#endif
      glitch_reporter_(SystemGlitchReporter::StreamType::kCapture),
      log_callback_(std::move(log_callback)),
      peak_detector_(base::BindRepeating(&AudioManager::TraceAmplitudePeak,
                                         base::Unretained(audio_manager_),
                                         /*trace_start=*/true)) {
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

AudioInputStream::OpenOutcome CrasInputStream::Open() {
  if (client_) {
    NOTREACHED_IN_MIGRATION() << "CrasInputStream already open";
    ReportStreamOpenResult(StreamOpenResult::kCallbackOpenClientAlreadyOpen);
    return OpenOutcome::kAlreadyOpen;
  }

  // Sanity check input values.
  if (params_.sample_rate() <= 0) {
    DLOG(WARNING) << "Unsupported audio frequency.";
    ReportStreamOpenResult(
        StreamOpenResult::kCallbackOpenUnsupportedAudioFrequency);
    return OpenOutcome::kFailed;
  }

  if (AudioParameters::AUDIO_PCM_LINEAR != params_.format() &&
      AudioParameters::AUDIO_PCM_LOW_LATENCY != params_.format()) {
    DLOG(WARNING) << "Unsupported audio format.";
    ReportStreamOpenResult(
        StreamOpenResult::kCallbackOpenUnsupportedAudioFormat);
    return OpenOutcome::kFailed;
  }

  // Create the client and connect to the CRAS server.
  client_ = libcras_client_create();
  if (!client_) {
    DLOG(WARNING) << "Couldn't create CRAS client.\n";
    ReportStreamOpenResult(
        StreamOpenResult::kCallbackOpenCrasClientCreationFailed);
    client_ = NULL;
    return OpenOutcome::kFailed;
  }

  if (libcras_client_connect(client_)) {
    DLOG(WARNING) << "Couldn't connect CRAS client.\n";
    ReportStreamOpenResult(
        StreamOpenResult::kCallbackOpenCannotConnectToCrasClient);
    libcras_client_destroy(client_);
    client_ = NULL;
    return OpenOutcome::kFailed;
  }

  // Then start running the client.
  if (libcras_client_run_thread(client_)) {
    DLOG(WARNING) << "Couldn't run CRAS client.\n";
    ReportStreamOpenResult(StreamOpenResult::kCallbackOpenCannotRunCrasClient);
    libcras_client_destroy(client_);
    client_ = NULL;
    return OpenOutcome::kFailed;
  }

  if (is_loopback_) {
    if (libcras_client_connected_wait(client_) < 0) {
      DLOG(WARNING) << "Couldn't synchronize data.";
      // TODO(chinyue): Add a DestroyClientOnError method to de-duplicate the
      // cleanup code.
      ReportStreamOpenResult(
          StreamOpenResult::kCallbackOpenCannotSynchronizeData);
      libcras_client_destroy(client_);
      client_ = NULL;
      return OpenOutcome::kFailed;
    }

    int rc;
    if (is_loopback_without_chrome_) {
      uint32_t client_types = 0;
      client_types |= 1 << CRAS_CLIENT_TYPE_CHROME;
      client_types |= 1 << CRAS_CLIENT_TYPE_LACROS;
      client_types = ~client_types;
      rc = pin_device_ = libcras_client_get_floop_dev_idx_by_client_types(
          client_, client_types);
    } else {
      if (base::FeatureList::IsEnabled(
              media::kAudioFlexibleLoopbackForSystemLoopback)) {
        rc = pin_device_ = libcras_client_get_floop_dev_idx_by_client_types(
            client_, ~(uint32_t)0);
      } else {
        rc = libcras_client_get_loopback_dev_idx(client_, &pin_device_);
      }
    }
    if (rc < 0) {
      DLOG(WARNING) << "Couldn't find CRAS loopback device "
                    << (is_loopback_without_chrome_
                            ? " for non-chrome loopback."
                            : " for full loopback.");
      ReportStreamOpenResult(
          StreamOpenResult::kCallbackOpenCannotFindLoopbackDevice);
      libcras_client_destroy(client_);
      client_ = NULL;
      return OpenOutcome::kFailed;
    }
  }

  ReportStreamOpenResult(StreamOpenResult::kCallbackOpenSuccess);
  return OpenOutcome::kSuccess;
}

void CrasInputStream::Close() {
  Stop();

  if (client_) {
    libcras_client_stop(client_);
    libcras_client_destroy(client_);
    client_ = NULL;
  }

  // Signal to the manager that we're closed and can be removed.
  // Should be last call in the method as it deletes "this".
  audio_manager_->ReleaseInputStream(this);
}

inline bool CrasInputStream::UseCrasAec() const {
  return params_.effects() & AudioParameters::ECHO_CANCELLER;
}

inline bool CrasInputStream::UseCrasNs() const {
  return params_.effects() & AudioParameters::NOISE_SUPPRESSION;
}

inline bool CrasInputStream::UseCrasAgc() const {
  return params_.effects() & AudioParameters::AUTOMATIC_GAIN_CONTROL;
}

inline bool CrasInputStream::UseClientControlledVoiceIsolation() const {
  return params_.effects() & AudioParameters::CLIENT_CONTROLLED_VOICE_ISOLATION;
}

inline bool CrasInputStream::UseCrasVoiceIsolation() const {
  return params_.effects() & AudioParameters::VOICE_ISOLATION;
}

inline bool CrasInputStream::DspBasedAecIsAllowed() const {
  return params_.effects() & AudioParameters::ALLOW_DSP_ECHO_CANCELLER;
}

inline bool CrasInputStream::DspBasedNsIsAllowed() const {
  return params_.effects() & AudioParameters::ALLOW_DSP_NOISE_SUPPRESSION;
}

inline bool CrasInputStream::DspBasedAgcIsAllowed() const {
  return params_.effects() & AudioParameters::ALLOW_DSP_AUTOMATIC_GAIN_CONTROL;
}

inline bool CrasInputStream::IgnoreUiGains() const {
  return params_.effects() & AudioParameters::IGNORE_UI_GAINS;
}

void CrasInputStream::Start(AudioInputCallback* callback) {
  DCHECK(client_);
  DCHECK(callback);

  // Channel map to CRAS_CHANNEL, values in the same order of
  // corresponding source in Chromium defined Channels.
  static const int kChannelMap[] = {
      CRAS_CH_FL,  CRAS_CH_FR,  CRAS_CH_FC, CRAS_CH_LFE, CRAS_CH_RL, CRAS_CH_RR,
      CRAS_CH_FLC, CRAS_CH_FRC, CRAS_CH_RC, CRAS_CH_SL,  CRAS_CH_SR};
  static_assert(std::size(kChannelMap) == CHANNELS_MAX + 1,
                "kChannelMap array size should match");

  // If already playing, stop before re-starting.
  if (started_) {
    return;
  }

  StartAgc();

  callback_ = callback;

  CRAS_STREAM_TYPE type = CRAS_STREAM_TYPE_DEFAULT;
  uint32_t flags = 0;
  if (params_.effects() & AudioParameters::PlatformEffectsMask::HOTWORD) {
    flags = HOTWORD_STREAM;
    type = CRAS_STREAM_TYPE_SPEECH_RECOGNITION;
  }

  unsigned int frames_per_packet = params_.frames_per_buffer();
  struct libcras_stream_params* stream_params = libcras_stream_params_create();
  if (!stream_params) {
    DLOG(ERROR) << "Error creating stream params";
    ReportStreamStartResult(
        StreamStartResult::kCallbackStartErrorCreatingStreamParameters);
    callback_->OnError();
    callback_ = NULL;
    return;
  }

  int rc = libcras_stream_params_set(
      stream_params, stream_direction_, frames_per_packet, frames_per_packet,
      type, audio_manager_->GetClientType(), flags, this,
      CrasInputStream::SamplesReady, CrasInputStream::StreamError,
      params_.sample_rate(), SND_PCM_FORMAT_S16, params_.channels());

  if (rc) {
    DLOG(WARNING) << "Error setting up stream parameters.";
    ReportStreamStartResult(
        StreamStartResult::kCallbackStartErrorSettingUpStreamParameters);
    callback_->OnError();
    callback_ = NULL;
    libcras_stream_params_destroy(stream_params);
    return;
  }

  // Initialize channel layout to all -1 to indicate that none of
  // the channels is set in the layout.
  int8_t layout[CRAS_CH_MAX];
  for (size_t i = 0; i < std::size(layout); ++i) {
    layout[i] = -1;
  }

  // Converts to CRAS defined channels. ChannelOrder will return -1
  // for channels that are not present in params_.channel_layout().
  for (size_t i = 0; i < std::size(kChannelMap); ++i) {
    layout[kChannelMap[i]] =
        ChannelOrder(params_.channel_layout(), static_cast<Channels>(i));
  }

  rc = libcras_stream_params_set_channel_layout(stream_params, CRAS_CH_MAX,
                                                layout);
  if (rc) {
    DLOG(WARNING) << "Error setting up the channel layout.";
    ReportStreamStartResult(
        StreamStartResult::kCallbackStartErrorSettingUpChannelLayout);
    callback_->OnError();
    callback_ = NULL;
    libcras_stream_params_destroy(stream_params);
    return;
  }

  if (UseCrasAec()) {
    libcras_stream_params_enable_aec(stream_params);
  }

  if (UseCrasNs()) {
    libcras_stream_params_enable_ns(stream_params);
  }

  if (UseCrasAgc()) {
    libcras_stream_params_enable_agc(stream_params);
  }

  if (base::FeatureList::IsEnabled(media::kCrOSSystemVoiceIsolationOption)) {
    if (UseClientControlledVoiceIsolation()) {
      if (UseCrasVoiceIsolation()) {
        libcras_stream_params_enable_voice_isolation(stream_params);
        RecordVoiceIsolationState(StreamEffectState::kForceEnable);
      } else {
        libcras_stream_params_disable_voice_isolation(stream_params);
        RecordVoiceIsolationState(StreamEffectState::kForceDisable);
      }
    } else {
      RecordVoiceIsolationState(StreamEffectState::kPlatformDefault);
    }
  }

  if (DspBasedAecIsAllowed()) {
    libcras_stream_params_allow_aec_on_dsp(stream_params);
  }

  if (DspBasedNsIsAllowed()) {
    libcras_stream_params_allow_ns_on_dsp(stream_params);
  }

  if (DspBasedAgcIsAllowed()) {
    libcras_stream_params_allow_agc_on_dsp(stream_params);
  }

  if (IgnoreUiGains()) {
    libcras_stream_params_ignore_ui_gains(stream_params);
  }

  // Adding the stream will start the audio callbacks.
  if (libcras_client_add_pinned_stream(client_, pin_device_, &stream_id_,
                                       stream_params)) {
    DLOG(WARNING) << "Failed to add the stream.";
    ReportStreamStartResult(
        StreamStartResult::kCallbackStartFailedAddingStream);
    callback_->OnError();
    callback_ = NULL;
  }

  // Mute system audio if requested.
  if (mute_system_audio_) {
    int muted;
    libcras_client_get_system_muted(client_, &muted);
    if (!muted) {
      libcras_client_set_system_mute(client_, 1);
    }
    mute_done_ = true;
  }

  // Done with config params.
  libcras_stream_params_destroy(stream_params);

  started_ = true;

  audio_manager_->RegisterSystemAecDumpSource(this);

  ReportStreamStartResult(StreamStartResult::kCallbackStartSuccess);
}

void CrasInputStream::Stop() {
  if (!client_) {
    return;
  }

  if (!callback_ || !started_) {
    return;
  }

  audio_manager_->DeregisterSystemAecDumpSource(this);

  if (mute_system_audio_ && mute_done_) {
    libcras_client_set_system_mute(client_, 0);
    mute_done_ = false;
  }

  StopAgc();

  // Removing the stream from the client stops audio.
  libcras_client_rm_stream(client_, stream_id_);

  ReportAndResetStats();

  started_ = false;
  callback_ = NULL;
}

// Static callback asking for samples.  Run on high priority thread.
int CrasInputStream::SamplesReady(struct libcras_stream_cb_data* data) {
  unsigned int frames;
  uint8_t* buf;
  struct timespec latency;
  void* usr_arg;
  uint32_t overrun_frames = 0;
  struct timespec dropped_samples_duration_ts;
  base::TimeDelta dropped_samples_duration;

  libcras_stream_cb_data_get_frames(data, &frames);
  libcras_stream_cb_data_get_buf(data, &buf);
  libcras_stream_cb_data_get_latency(data, &latency);
  libcras_stream_cb_data_get_usr_arg(data, &usr_arg);
  CrasInputStream* me = static_cast<CrasInputStream*>(usr_arg);
  me->ReadAudio(frames, buf, &latency);
  // Audio glitches are checked every callback.
  libcras_stream_cb_data_get_overrun_frames(data, &overrun_frames);
  libcras_stream_cb_data_get_dropped_samples_duration(
      data, &dropped_samples_duration_ts);
  dropped_samples_duration =
      base::TimeDelta::FromTimeSpec(dropped_samples_duration_ts);
  me->CalculateAudioGlitches(overrun_frames, dropped_samples_duration);
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
                                const timespec* latency_ts) {
  DCHECK(callback_);

  // Update the AGC volume level once every second. Note that, |volume| is
  // also updated each time SetVolume() is called through IPC by the
  // render-side AGC.
  double normalized_volume = 0.0;
  GetAgcVolume(&normalized_volume);

  const base::TimeDelta delay =
      std::max(base::TimeDelta::FromTimeSpec(*latency_ts), base::TimeDelta());

  // The delay says how long ago the capture was, so we subtract the delay from
  // Now() to find the capture time.
  const base::TimeTicks capture_time = base::TimeTicks::Now() - delay;

  audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(
      reinterpret_cast<int16_t*>(buffer), audio_bus_->frames());

  peak_detector_.FindPeak(audio_bus_.get());

  callback_->OnData(audio_bus_.get(), capture_time, normalized_volume,
                    glitch_info_accumulator_.GetAndReset());
}

void CrasInputStream::NotifyStreamError(int err) {
  ReportNotifyStreamErrors(err);
  if (callback_) {
    callback_->OnError();
  }
}

double CrasInputStream::GetMaxVolume() {
  return 1.0f;
}

void CrasInputStream::SetVolume(double volume) {
  DCHECK(client_);

  // Set the volume ratio to CRAS's softare and stream specific gain.
  input_volume_ = volume;
  libcras_client_set_stream_volume(client_, stream_id_, input_volume_);

  // Update the AGC volume level based on the last setting above. Note that,
  // the volume-level resolution is not infinite and it is therefore not
  // possible to assume that the volume provided as input parameter can be
  // used directly. Instead, a new query to the audio hardware is required.
  // This method does nothing if AGC is disabled.
  UpdateAgcVolume();
}

double CrasInputStream::GetVolume() {
  if (!client_) {
    return 0.0;
  }

  return input_volume_;
}

bool CrasInputStream::IsMuted() {
  int muted = 0;
  libcras_client_get_system_capture_muted(client_, &muted);
  return static_cast<bool>(muted);
}

void CrasInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  DCHECK(client_);

  int echo_ref_id;

  // Default device means to just use the system default output as AEC
  // reference. CRAS server side requires passing NO_DEVICE in that case.
  if (AudioDeviceDescription::IsDefaultDevice(output_device_id)) {
    echo_ref_id = NO_DEVICE;
  } else {
    uint64_t cras_node_id;
    base::StringToUint64(output_device_id, &cras_node_id);
    echo_ref_id = dev_index_of(cras_node_id);
  }
  libcras_client_set_aec_ref(client_, stream_id_, echo_ref_id);
}

void CrasInputStream::StartAecdump(base::File file) {
  FILE* stream = base::FileToFILE(std::move(file), "w");
  if (!client_) {
    return;
  }
#if DCHECK_IS_ON()
  DCHECK(!recording_enabled_);
  recording_enabled_ = true;
#endif

  libcras_client_set_aec_dump(client_, stream_id_, /*start=*/1, fileno(stream));
}

void CrasInputStream::StopAecdump() {
  if (!client_) {
    return;
  }
#if DCHECK_IS_ON()
  DCHECK(recording_enabled_);
  recording_enabled_ = false;
#endif
  libcras_client_set_aec_dump(client_, stream_id_, /*start=*/0, /*fd=*/-1);
}

void CrasInputStream::ReportAndResetStats() {
  SystemGlitchReporter::Stats stats =
      glitch_reporter_.GetLongTermStatsAndReset();

  std::string log_message = base::StringPrintf(
      "CRAS in: (num_glitches_detected=[%d], cumulative_audio_lost=[%" PRId64
      " ms],largest_glitch=[%" PRId64 " ms])",
      stats.glitches_detected, stats.total_glitch_duration.InMilliseconds(),
      stats.largest_glitch_duration.InMilliseconds());

  log_callback_.Run(log_message);
  if (stats.glitches_detected != 0) {
    DLOG(WARNING) << log_message;
  }
  last_overrun_frames_ = 0;
  last_dropped_samples_duration_ = base::TimeDelta();
}

void CrasInputStream::CalculateAudioGlitches(
    uint32_t overrun_frames,
    base::TimeDelta dropped_samples_duration) {
  // |overrun_frames| obtained from callback is the cumulative value of the
  // overwritten frames of the whole stream. Calculate the overrun frames this
  // callback and convert it to base::TimeDelta.
  DCHECK_GE(overrun_frames, last_overrun_frames_);
  uint32_t overrun_frames_this_callback = overrun_frames - last_overrun_frames_;
  base::TimeDelta overrun_glitch_duration = AudioTimestampHelper::FramesToTime(
      overrun_frames_this_callback, params_.sample_rate());

  // |dropped_samples_duration| obtained from callback is the cumulative value
  // of the dropped audio samples of the whole stream. Calculate the dropped
  // audio sample duration this callback.
  DCHECK_GE(dropped_samples_duration, last_dropped_samples_duration_);
  base::TimeDelta dropped_samples_glitch_duration =
      dropped_samples_duration - last_dropped_samples_duration_;

  base::TimeDelta glitch_duration =
      overrun_glitch_duration + dropped_samples_glitch_duration;
  glitch_reporter_.UpdateStats(glitch_duration);
  if (glitch_duration.is_positive()) {
    glitch_info_accumulator_.Add(AudioGlitchInfo::SingleBoundedSystemGlitch(
        glitch_duration, AudioGlitchInfo::Direction::kCapture));
  }

  last_overrun_frames_ = overrun_frames;
  last_dropped_samples_duration_ = dropped_samples_duration;
}

}  // namespace media
