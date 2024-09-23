// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager_base.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_input_stream_data_interceptor.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_output_resampler.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/base/media_switches.h"

namespace media {

namespace {

const int kStreamCloseDelaySeconds = 5;

// Default maximum number of output streams that can be open simultaneously
// for all platforms.
const int kDefaultMaxOutputStreams = 16;

// Default maximum number of input streams that can be open simultaneously
// for all platforms.
const int kMaxInputStreams = 16;

const int kMaxInputChannels = 3;

// Helper function to pass as callback when the audio debug recording is not
// enabled.
std::unique_ptr<AudioDebugRecorder> GetNullptrAudioDebugRecorder(
    const AudioParameters& params) {
  return nullptr;
}

// This enum must match the numbering for AudioOutputProxyStreamFormat in
// enums.xml. Do not reorder or remove items, only add new items before
// STREAM_FORMAT_MAX.
enum StreamFormat {
  STREAM_FORMAT_BITSTREAM = 0,
  STREAM_FORMAT_PCM_LINEAR = 1,
  STREAM_FORMAT_PCM_LOW_LATENCY = 2,
  STREAM_FORMAT_PCM_LOW_LATENCY_FALLBACK_TO_FAKE = 3,
  STREAM_FORMAT_FAKE = 4,
  STREAM_FORMAT_MAX = 4,
};

// Used to log errors in `AudioManagerBase::MakeAudioInputStream`.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MakeAudioInputStreamResult {
  kNoError = 0,
  kErrorSwitchFailAudioStreamCreation = 1,
  kErrorInvalidParams = 2,
  kErrorExcessiveInputStreams = 3,
  kErrorCreateStream = 4,
  kMaxValue = kErrorCreateStream
};

void LogMakeAudioInputStreamResult(MakeAudioInputStreamResult result) {
  base::UmaHistogramEnumeration("Media.Audio.MakeAudioInputStreamStatus",
                                result);
}

PRINTF_FORMAT(2, 3)
void SendLogMessage(const AudioManagerBase::LogCallback& callback,
                    const char* format,
                    ...) {
  if (callback.is_null())
    return;
  va_list args;
  va_start(args, format);
  callback.Run("AMB::" + base::StringPrintV(format, args));
  va_end(args);
}

}  // namespace

struct AudioManagerBase::DispatcherParams {
  DispatcherParams(const AudioParameters& input,
                   const AudioParameters& output,
                   const std::string& output_device_id)
      : input_params(input),
        output_params(output),
        output_device_id(output_device_id) {}

  DispatcherParams(const DispatcherParams&) = delete;
  DispatcherParams& operator=(const DispatcherParams&) = delete;

  ~DispatcherParams() = default;

  const AudioParameters input_params;
  const AudioParameters output_params;
  const std::string output_device_id;
  std::unique_ptr<AudioOutputDispatcher> dispatcher;
};

AudioManagerBase::AudioManagerBase(std::unique_ptr<AudioThread> audio_thread,
                                   AudioLogFactory* audio_log_factory)
    : AudioManager(std::move(audio_thread)),
      max_num_output_streams_(kDefaultMaxOutputStreams),
      num_output_streams_(0),
      // TODO(dalecurtis): Switch this to an base::ObserverListThreadSafe, so we
      // don't block the UI thread when swapping devices.
      output_listeners_(base::ObserverListPolicy::EXISTING_ONLY),
      audio_log_factory_(audio_log_factory) {}

AudioManagerBase::~AudioManagerBase() {
  // All the output streams should have been deleted.
  CHECK_EQ(0, num_output_streams_);
  // All the input streams should have been deleted.
  CHECK(input_streams_.empty());
}

void AudioManagerBase::GetAudioInputDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions) {
  CHECK(GetTaskRunner()->BelongsToCurrentThread());
  GetAudioDeviceDescriptions(device_descriptions,
                             &AudioManagerBase::GetAudioInputDeviceNames,
                             &AudioManagerBase::GetDefaultInputDeviceID,
                             &AudioManagerBase::GetCommunicationsInputDeviceID,
                             &AudioManagerBase::GetGroupIDInput);
}

void AudioManagerBase::GetAudioOutputDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions) {
  CHECK(GetTaskRunner()->BelongsToCurrentThread());
  GetAudioDeviceDescriptions(device_descriptions,
                             &AudioManagerBase::GetAudioOutputDeviceNames,
                             &AudioManagerBase::GetDefaultOutputDeviceID,
                             &AudioManagerBase::GetCommunicationsOutputDeviceID,
                             &AudioManagerBase::GetGroupIDOutput);
}

void AudioManagerBase::GetAudioDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions,
    void (AudioManagerBase::*get_device_names)(AudioDeviceNames*),
    std::string (AudioManagerBase::*get_default_device_id)(),
    std::string (AudioManagerBase::*get_communications_device_id)(),
    std::string (AudioManagerBase::*get_group_id)(const std::string&)) {
  CHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioDeviceNames device_names;
  (this->*get_device_names)(&device_names);
  std::string real_default_device_id = (this->*get_default_device_id)();
  std::string real_communications_device_id =
      (this->*get_communications_device_id)();
  std::string real_default_name;
  std::string real_communications_name;

  // Find the names for the real devices that are mapped to the default and
  // communications devices.
  for (const auto& name : device_names) {
    if (name.unique_id == real_default_device_id)
      real_default_name = name.device_name;
    if (name.unique_id == real_communications_device_id)
      real_communications_name = name.device_name;
  }

  for (auto& name : device_names) {
    // Checks whether `name.unique_id` is the id of the real device that is
    // mapped to the virtual default and/or communications devices.
    bool is_real_system_default = name.unique_id == real_default_device_id;
    bool is_real_communications_device =
        name.unique_id == real_communications_device_id;

    bool is_virtual_system_default = false;
    bool is_virtual_communications_device = false;
    if (AudioDeviceDescription::IsDefaultDevice(name.unique_id)) {
      // Virtual default device.
      name.device_name = real_default_name;
      is_virtual_system_default = true;
    } else if (AudioDeviceDescription::IsCommunicationsDevice(name.unique_id)) {
      // Virtual communications device.
      name.device_name = real_communications_name;
      is_virtual_communications_device = true;
    }

    std::string group_id = (this->*get_group_id)(name.unique_id);
    device_descriptions->emplace_back(
        std::move(name.device_name), std::move(name.unique_id),
        std::move(group_id),
        is_virtual_system_default || is_real_system_default,
        is_virtual_communications_device || is_real_communications_device);
  }
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  CHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(params.IsValid());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFailAudioStreamCreation)) {
    return nullptr;
  }

  SendLogMessage(log_callback, "%s({device_id=%s}, {params=[%s]})", __func__,
                 device_id.c_str(), params.AsHumanReadableString().c_str());

  // Limit the number of audio streams opened. This is to prevent using
  // excessive resources for a large number of audio streams. More
  // importantly it prevents instability on certain systems.
  // See bug: http://crbug.com/30242.
  if (num_output_streams_ >= max_num_output_streams_) {
    LOG(ERROR) << "Number of opened output audio streams "
               << num_output_streams_ << " exceed the max allowed number "
               << max_num_output_streams_;
    return nullptr;
  }

  AudioOutputStream* stream;
  switch (params.format()) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      DCHECK(AudioDeviceDescription::IsDefaultDevice(device_id))
          << "AUDIO_PCM_LINEAR supports only the default device.";
      stream = MakeLinearOutputStream(params, log_callback);
      break;
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      stream = MakeLowLatencyOutputStream(params, device_id, log_callback);
      break;
    case AudioParameters::AUDIO_BITSTREAM_AC3:
    case AudioParameters::AUDIO_BITSTREAM_EAC3:
    case AudioParameters::AUDIO_BITSTREAM_DTS:
    case AudioParameters::AUDIO_BITSTREAM_DTS_HD:
    case AudioParameters::AUDIO_BITSTREAM_DTSX_P2:
    case AudioParameters::AUDIO_BITSTREAM_IEC61937:
      stream = MakeBitstreamOutputStream(params, device_id, log_callback);
      break;
    case AudioParameters::AUDIO_FAKE:
      stream = FakeAudioOutputStream::MakeFakeStream(this, params);
      break;
    default:
      stream = nullptr;
      break;
  }

  if (stream) {
    ++num_output_streams_;
    SendLogMessage(log_callback, "%s => (number of streams=%d)", __func__,
                   output_stream_count());
  }

  return stream;
}

AudioOutputStream* AudioManagerBase::MakeBitstreamOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return nullptr;
}

AudioInputStream* AudioManagerBase::MakeAudioInputStream(
    const AudioParameters& input_params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  CHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFailAudioStreamCreation)) {
    LogMakeAudioInputStreamResult(
        MakeAudioInputStreamResult::kErrorSwitchFailAudioStreamCreation);
    return nullptr;
  }

  // If audio has been disabled force usage of a fake audio stream.
  auto params = input_params;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioInput)) {
    params.set_format(AudioParameters::AUDIO_FAKE);
  }

  SendLogMessage(log_callback, "%s({device_id=%s}, {params=[%s]})", __func__,
                 device_id.c_str(), params.AsHumanReadableString().c_str());

  if (!params.IsValid() || (params.channels() > kMaxInputChannels) ||
      device_id.empty()) {
    DLOG(ERROR) << "Audio parameters are invalid for device " << device_id
                << ", params: " << params.AsHumanReadableString();
    LogMakeAudioInputStreamResult(
        MakeAudioInputStreamResult::kErrorInvalidParams);
    return nullptr;
  }

  if (input_stream_count() >= kMaxInputStreams) {
    LOG(ERROR) << "Number of opened input audio streams "
               << input_stream_count() << " exceed the max allowed number "
               << kMaxInputStreams;
    LogMakeAudioInputStreamResult(
        MakeAudioInputStreamResult::kErrorExcessiveInputStreams);
    return nullptr;
  }

  DVLOG(2) << "Creating a new AudioInputStream with buffer size = "
           << params.frames_per_buffer();

  AudioInputStream* stream;
  switch (params.format()) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      stream = MakeLinearInputStream(params, device_id, log_callback);
      break;
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      stream = MakeLowLatencyInputStream(params, device_id, log_callback);
      break;
    case AudioParameters::AUDIO_FAKE:
      stream = FakeAudioInputStream::MakeFakeStream(this, params);
      break;
    default:
      stream = nullptr;
      break;
  }

  if (stream) {
    input_streams_.insert(stream);
    if (!log_callback.is_null()) {
      SendLogMessage(log_callback, "%s => (number of streams=%d)", __func__,
                     input_stream_count());
    }

    if (!params.IsBitstreamFormat() && debug_recording_manager_) {
      // Using unretained for |debug_recording_manager_| is safe since it
      // outlives the audio thread, on which streams are operated.
      // Note: The AudioInputStreamDataInterceptor takes ownership of the
      // created stream and cleans it up when it is Close()d, transparently to
      // the user of the stream. I the case where the audio manager closes the
      // stream (Mac), this will result in a dangling pointer.
      stream = new AudioInputStreamDataInterceptor(
          base::BindRepeating(
              &AudioDebugRecordingManager::RegisterDebugRecordingSource,
              base::Unretained(debug_recording_manager_.get()),
              AudioDebugRecordingStreamType::kInput, params),
          stream);
    }
  }

  LogMakeAudioInputStreamResult(
      stream ? MakeAudioInputStreamResult::kNoError
             : MakeAudioInputStreamResult::kErrorCreateStream);

  return stream;
}

AudioOutputStream* AudioManagerBase::MakeAudioOutputStreamProxy(
    const AudioParameters& params,
    const std::string& device_id) {
  CHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(params.IsValid());
  std::optional<StreamFormat> uma_stream_format;

  // If the caller supplied an empty device id to select the default device,
  // we fetch the actual device id of the default device so that the lookup
  // will find the correct device regardless of whether it was opened as
  // "default" or via the specific id.
  // NOTE: Implementations that don't yet support opening non-default output
  // devices may return an empty string from GetDefaultOutputDeviceID().
  std::string output_device_id =
      AudioDeviceDescription::IsDefaultDevice(device_id)
          ?
#if BUILDFLAG(IS_CHROMEOS)
          // On ChromeOS, it is expected that, if the default device is given,
          // no specific device ID should be used since the actual output device
          // should change dynamically if the system default device changes.
          // See http://crbug.com/750614.
          std::string()
#else
          GetDefaultOutputDeviceID()
#endif
          : device_id;

  // If we're not using AudioOutputResampler our output parameters are the same
  // as our input parameters.
  AudioParameters output_params = params;

  // If audio has been disabled force usage of a fake audio stream.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioOutput)) {
    output_params.set_format(AudioParameters::AUDIO_FAKE);
  }

  if (params.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY &&
      output_params.format() != AudioParameters::AUDIO_FAKE) {
    output_params =
        GetPreferredOutputStreamParameters(output_device_id, params);

    // Ensure we only pass on valid output parameters.
    if (output_params.IsValid()) {
      if (params.effects() & AudioParameters::MULTIZONE) {
        // Never turn off the multizone effect even if it is not preferred.
        output_params.set_effects(output_params.effects() |
                                  AudioParameters::MULTIZONE);
      }
      if (params.effects() != output_params.effects()) {
        // Turn off effects that weren't requested.
        output_params.set_effects(params.effects() & output_params.effects());
      }

      uma_stream_format = STREAM_FORMAT_PCM_LOW_LATENCY;
    } else {
      // We've received invalid audio output parameters, so switch to a mock
      // output device based on the input parameters.  This may happen if the OS
      // provided us junk values for the hardware configuration.
      LOG(ERROR) << "Invalid audio output parameters received; using fake "
                 << "audio path: " << output_params.AsHumanReadableString();

      // Tell the AudioManager to create a fake output device.
      output_params = params;
      output_params.set_format(AudioParameters::AUDIO_FAKE);
      uma_stream_format = STREAM_FORMAT_PCM_LOW_LATENCY_FALLBACK_TO_FAKE;
    }

    output_params.set_latency_tag(params.latency_tag());
  } else {
    switch (output_params.format()) {
      case AudioParameters::AUDIO_PCM_LINEAR:
        uma_stream_format = STREAM_FORMAT_PCM_LINEAR;
        break;
      case AudioParameters::AUDIO_FAKE:
        uma_stream_format = STREAM_FORMAT_FAKE;
        break;
      default:
        if (output_params.IsBitstreamFormat())
          uma_stream_format = STREAM_FORMAT_BITSTREAM;
        else
          NOTREACHED_IN_MIGRATION();
    }
  }

  if (uma_stream_format) {
    UMA_HISTOGRAM_ENUMERATION("Media.AudioOutputStreamProxy.StreamFormat",
                              *uma_stream_format, STREAM_FORMAT_MAX + 1);
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  auto dispatcher_params = std::make_unique<DispatcherParams>(
      params, output_params, output_device_id);

  // Do not reuse the output dispatcher if audio offload is requested.
  // Their underlying audio client is configured differently to make
  // it work with expected buffer size according to requested output
  // param.
  if (!output_params.RequireOffload()) {
    auto it = base::ranges::find_if(
        output_dispatchers_,
        [&](const std::unique_ptr<DispatcherParams>& dispatcher) {
          // We will reuse the existing dispatcher when:
          // 1) Unified IO is not used, input_params and output_params of the
          //    existing dispatcher are the same as the requested dispatcher.
          // 2) Unified IO is used, input_params and output_params of the
          // existing
          //    dispatcher are the same as the request dispatcher.
          return params.Equals(dispatcher->input_params) &&
                 output_params.Equals(dispatcher->output_params) &&
                 output_device_id == dispatcher->output_device_id;
        });
    if (it != output_dispatchers_.end()) {
      return (*it)->dispatcher->CreateStreamProxy();
    }
  }

  const base::TimeDelta kCloseDelay = base::Seconds(kStreamCloseDelaySeconds);
  std::unique_ptr<AudioOutputDispatcher> dispatcher;
  if (output_params.format() != AudioParameters::AUDIO_FAKE &&
      !output_params.IsBitstreamFormat()) {
    // Using unretained for |debug_recording_manager_| is safe since it
    // outlives the dispatchers (cleared in ShutdownOnAudioThread()).
    dispatcher = std::make_unique<AudioOutputResampler>(
        this, params, output_params, output_device_id, kCloseDelay,
        debug_recording_manager_
            ? base::BindRepeating(
                  &AudioDebugRecordingManager::RegisterDebugRecordingSource,
                  base::Unretained(debug_recording_manager_.get()),
                  AudioDebugRecordingStreamType::kOutput)
            : base::BindRepeating(&GetNullptrAudioDebugRecorder));
  } else {
    dispatcher = std::make_unique<AudioOutputDispatcherImpl>(
        this, output_params, output_device_id, kCloseDelay);
  }

  dispatcher_params->dispatcher = std::move(dispatcher);
  output_dispatchers_.push_back(std::move(dispatcher_params));
  return output_dispatchers_.back()->dispatcher->CreateStreamProxy();
}

void AudioManagerBase::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
}

void AudioManagerBase::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
}

void AudioManagerBase::ReleaseOutputStream(AudioOutputStream* stream) {
  CHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(stream);
  CHECK_GT(num_output_streams_, 0);
  // TODO(xians) : Have a clearer destruction path for the AudioOutputStream.
  // For example, pass the ownership to AudioManager so it can delete the
  // streams.
  --num_output_streams_;
  delete stream;
}

void AudioManagerBase::ReleaseInputStream(AudioInputStream* stream) {
  CHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(stream);
  // TODO(xians) : Have a clearer destruction path for the AudioInputStream.
  CHECK_EQ(1u, input_streams_.erase(stream));
  delete stream;
}

void AudioManagerBase::ShutdownOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Close all output streams.
  output_dispatchers_.clear();
}

void AudioManagerBase::AddOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_listeners_.AddObserver(listener);
}

void AudioManagerBase::RemoveOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_listeners_.RemoveObserver(listener);
}

void AudioManagerBase::NotifyAllOutputDeviceChangeListeners() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "Firing OnDeviceChange() notifications.";
  for (auto& observer : output_listeners_)
    observer.OnDeviceChange();
}

AudioParameters AudioManagerBase::GetOutputStreamParameters(
    const std::string& device_id) {
  return GetPreferredOutputStreamParameters(device_id,
      AudioParameters());
}

AudioParameters AudioManagerBase::GetInputStreamParameters(
    const std::string& device_id) {
  NOTREACHED();
}

std::string AudioManagerBase::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  return std::string();
}

std::string AudioManagerBase::GetGroupIDOutput(
    const std::string& output_device_id) {
  if (output_device_id == AudioDeviceDescription::kDefaultDeviceId) {
    std::string real_device_id = GetDefaultOutputDeviceID();
    if (!real_device_id.empty())
      return real_device_id;
  } else if (output_device_id ==
             AudioDeviceDescription::kCommunicationsDeviceId) {
    std::string real_device_id = GetCommunicationsOutputDeviceID();
    if (!real_device_id.empty())
      return real_device_id;
  }
  return output_device_id;
}

std::string AudioManagerBase::GetGroupIDInput(
    const std::string& input_device_id) {
  const std::string& real_input_device_id =
      input_device_id == AudioDeviceDescription::kDefaultDeviceId
          ? GetDefaultInputDeviceID()
          : input_device_id == AudioDeviceDescription::kCommunicationsDeviceId
                ? GetCommunicationsInputDeviceID()
                : input_device_id;
  std::string output_device_id =
      GetAssociatedOutputDeviceID(real_input_device_id);
  if (output_device_id.empty()) {
    // Some characters are added to avoid accidentally
    // giving the input the same group id as an output.
    return real_input_device_id + "input";
  }
  return GetGroupIDOutput(output_device_id);
}

void AudioManagerBase::CloseAllInputStreams() {
  for (auto iter = input_streams_.begin(); iter != input_streams_.end();) {
    // Note: Closing the stream will invalidate the iterator.
    // Increment the iterator before closing the stream.
    AudioInputStream* stream = *iter++;
    stream->Close();
  }
  CHECK(input_streams_.empty());
}

std::string AudioManagerBase::GetDefaultInputDeviceID() {
  return std::string();
}

std::string AudioManagerBase::GetDefaultOutputDeviceID() {
  return std::string();
}

std::string AudioManagerBase::GetCommunicationsInputDeviceID() {
  return std::string();
}

std::string AudioManagerBase::GetCommunicationsOutputDeviceID() {
  return std::string();
}

// static
int AudioManagerBase::GetUserBufferSize() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  int buffer_size = 0;
  std::string buffer_size_str(cmd_line->GetSwitchValueASCII(
      switches::kAudioBufferSize));
  if (base::StringToInt(buffer_size_str, &buffer_size) && buffer_size > 0)
    return buffer_size;

  return 0;
}

std::unique_ptr<AudioLog> AudioManagerBase::CreateAudioLog(
    AudioLogFactory::AudioComponent component,
    int component_id) {
  return audio_log_factory_->CreateAudioLog(component, component_id);
}

void AudioManagerBase::InitializeDebugRecording() {
  if (!GetTaskRunner()->BelongsToCurrentThread()) {
    // AudioManager is deleted on the audio thread, so it's safe to post
    // unretained.
    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AudioManagerBase::InitializeDebugRecording,
                                  base::Unretained(this)));
    return;
  }

  DCHECK(!debug_recording_manager_);
  debug_recording_manager_ = CreateAudioDebugRecordingManager();
}

std::unique_ptr<AudioDebugRecordingManager>
AudioManagerBase::CreateAudioDebugRecordingManager() {
  return std::make_unique<AudioDebugRecordingManager>();
}

AudioDebugRecordingManager* AudioManagerBase::GetAudioDebugRecordingManager() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return debug_recording_manager_.get();
}

void AudioManagerBase::SetAecDumpRecordingManager(
    base::WeakPtr<AecdumpRecordingManager>) {
  // This is no-op by default.
}

}  // namespace media
