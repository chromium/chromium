// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// THREAD SAFETY
//
// AlsaPcmOutputStream object is *not* thread-safe and should only be used
// from the audio thread.  We DCHECK on this assumption whenever we can.
//
// SEMANTICS OF Close()
//
// Close() is responsible for cleaning up any resources that were acquired after
// a successful Open().  Close() will nullify any scheduled outstanding runnable
// methods.
//
//
// SEMANTICS OF ERROR STATES
//
// The object has two distinct error states: |state_| == kInError
// and |stop_stream_|.  The |stop_stream_| variable is used to indicate
// that the playback_handle should no longer be used either because of a
// hardware/low-level event.
//
// When |state_| == kInError, all public API functions will fail with an error
// (Start() will call the OnError() function on the callback immediately), or
// no-op themselves with the exception of Close().  Even if an error state has
// been entered, if Open() has previously returned successfully, Close() must be
// called to cleanup the ALSA devices and release resources.
//
// When |stop_stream_| is set, no more commands will be made against the
// ALSA device, and playback will effectively stop.  From the client's point of
// view, it will seem that the device has just clogged and stopped requesting
// data.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/alsa/alsa_output.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/typed_macros.h"
#include "media/audio/alsa/alsa_util.h"
#include "media/audio/alsa/alsa_wrapper.h"
#include "media/audio/alsa/audio_manager_alsa.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_mixer.h"
#include "media/base/data_buffer.h"
#include "media/base/seekable_buffer.h"

namespace media {

// Set to 0 during debugging if you want error messages due to underrun
// events or other recoverable errors.
#if defined(NDEBUG)
static const int kPcmRecoverIsSilent = 1;
#else
static const int kPcmRecoverIsSilent = 0;
#endif

// The output channel layout if we set up downmixing for the kDefaultDevice
// device.
static const ChannelLayout kDefaultOutputChannelLayout = CHANNEL_LAYOUT_STEREO;

// While the "default" device may support multi-channel audio, in Alsa, only
// the device names surround40, surround41, surround50, etc, have a defined
// channel mapping according to Lennart:
//
// http://0pointer.de/blog/projects/guide-to-sound-apis.html
//
// This function makes a best guess at the specific > 2 channel device name
// based on the number of channels requested.  nullptr is returned if no device
// can be found to match the channel numbers.  In this case, using
// kDefaultDevice is probably the best bet.
//
// A five channel source is assumed to be surround50 instead of surround41
// (which is also 5 channels).
//
// TODO(ajwong): The source data should have enough info to tell us if we want
// surround41 versus surround51, etc., instead of needing us to guess based on
// channel number.  Fix API to pass that data down.
static const char* GuessSpecificDeviceName(uint32_t channels) {
  switch (channels) {
    case 8:
      return "surround71";

    case 7:
      return "surround70";

    case 6:
      return "surround51";

    case 5:
      return "surround50";

    case 4:
      return "surround40";

    default:
      return nullptr;
  }
}

std::ostream& operator<<(std::ostream& os,
                         AlsaPcmOutputStream::InternalState state) {
  switch (state) {
    case AlsaPcmOutputStream::kInError:
      os << "kInError";
      break;
    case AlsaPcmOutputStream::kCreated:
      os << "kCreated";
      break;
    case AlsaPcmOutputStream::kIsOpened:
      os << "kIsOpened";
      break;
    case AlsaPcmOutputStream::kIsPlaying:
      os << "kIsPlaying";
      break;
    case AlsaPcmOutputStream::kIsStopped:
      os << "kIsStopped";
      break;
    case AlsaPcmOutputStream::kIsClosed:
      os << "kIsClosed";
      break;
  }
  return os;
}

static const SampleFormat kSampleFormat = kSampleFormatS16;
static const snd_pcm_format_t kAlsaSampleFormat = SND_PCM_FORMAT_S16;

const char AlsaPcmOutputStream::kDefaultDevice[] = "default";
const char AlsaPcmOutputStream::kAutoSelectDevice[] = "";
const char AlsaPcmOutputStream::kPlugPrefix[] = "plug:";

// We use 40ms as our minimum required latency. If it is needed, we may be able
// to get it down to 20ms.
const uint32_t AlsaPcmOutputStream::kMinLatencyMicros = 40 * 1000;

AlsaPcmOutputStream::AlsaPcmOutputStream(const std::string& device_name,
                                         const AudioParameters& params,
                                         AlsaWrapper* wrapper,
                                         AudioManagerBase* manager)
    : requested_device_name_(device_name),
      pcm_format_(kAlsaSampleFormat),
      channels_(params.channels()),
      channel_layout_(params.channel_layout()),
      sample_rate_(params.sample_rate()),
      bytes_per_sample_(SampleFormatToBytesPerChannel(kSampleFormat)),
      bytes_per_frame_(params.GetBytesPerFrame(kSampleFormat)),
      packet_size_(params.GetBytesPerBuffer(kSampleFormat)),
      latency_(std::max(
          base::Microseconds(kMinLatencyMicros),
          AudioTimestampHelper::FramesToTime(params.frames_per_buffer() * 2,
                                             sample_rate_))),
      bytes_per_output_frame_(bytes_per_frame_),
      alsa_buffer_frames_(0),
      stop_stream_(false),
      wrapper_(wrapper),
      manager_(manager),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      playback_handle_(nullptr),
      frames_per_packet_(packet_size_ / bytes_per_frame_),
      state_(kCreated),
      volume_(1.0f),
      source_callback_(nullptr),
      audio_bus_(AudioBus::Create(params)),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK(manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(audio_bus_->frames() * bytes_per_frame_, packet_size_);

  // Sanity check input values.
  if (!params.IsValid()) {
    LOG(WARNING) << "Unsupported audio parameters.";
    TransitionTo(kInError);
  }
}

AlsaPcmOutputStream::~AlsaPcmOutputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InternalState current_state = state();
  DCHECK(current_state == kCreated ||
         current_state == kIsClosed ||
         current_state == kInError);
  DCHECK(!playback_handle_);
}

bool AlsaPcmOutputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state() == kInError)
    return false;

  if (!CanTransitionTo(kIsOpened)) {
    NOTREACHED_IN_MIGRATION() << "Invalid state: " << state();
    return false;
  }

  // We do not need to check if the transition was successful because
  // CanTransitionTo() was checked above, and it is assumed that this
  // object's public API is only called on one thread so the state cannot
  // transition out from under us.
  TransitionTo(kIsOpened);

  // Try to open the device.
  if (requested_device_name_ == kAutoSelectDevice) {
    playback_handle_ = AutoSelectDevice(latency_.InMicroseconds());
    if (playback_handle_)
      DVLOG(1) << "Auto-selected device: " << device_name_;
  } else {
    device_name_ = requested_device_name_;
    playback_handle_ = alsa_util::OpenPlaybackDevice(
        wrapper_, device_name_.c_str(), channels_, sample_rate_,
        pcm_format_, latency_.InMicroseconds());
  }

  // Finish initializing the stream if the device was opened successfully.
  if (playback_handle_ == nullptr) {
    stop_stream_ = true;
    TransitionTo(kInError);
    return false;
  }
  bytes_per_output_frame_ =
      channel_mixer_ ? mixed_audio_bus_->channels() * bytes_per_sample_
                     : bytes_per_frame_;
  uint32_t output_packet_size = frames_per_packet_ * bytes_per_output_frame_;
  buffer_ = std::make_unique<SeekableBuffer>(0, output_packet_size);

  // Get alsa buffer size.
  snd_pcm_uframes_t buffer_size;
  snd_pcm_uframes_t period_size;
  int error =
      wrapper_->PcmGetParams(playback_handle_, &buffer_size, &period_size);
  if (error < 0) {
    LOG(ERROR) << "Failed to get playback buffer size from ALSA: "
               << wrapper_->StrError(error);
    // Buffer size is at least twice of packet size.
    alsa_buffer_frames_ = frames_per_packet_ * 2;
  } else {
    alsa_buffer_frames_ = buffer_size;
  }

  return true;
}

void AlsaPcmOutputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state() != kIsClosed)
    TransitionTo(kIsClosed);

  // Shutdown the audio device.
  if (playback_handle_) {
    int res =
        alsa_util::CloseDevice(wrapper_, playback_handle_.ExtractAsDangling());
    if (res < 0) {
      LOG(WARNING) << "Unable to close audio device. Leaking handle.";
    }

    // Release the buffer.
    buffer_.reset();

    // Signal anything that might already be scheduled to stop.
    stop_stream_ = true;  // Not necessary in production, but unit tests
                          // uses the flag to verify that stream was closed.
  }

  weak_factory_.InvalidateWeakPtrs();

  // Signal to the manager that we're closed and can be removed.
  // Should be last call in the method as it deletes "this".
  manager_->ReleaseOutputStream(this);
}

void AlsaPcmOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(callback);

  if (stop_stream_)
    return;

  // Only post the task if we can enter the playing state.
  if (TransitionTo(kIsPlaying) != kIsPlaying)
    return;

  // Before starting, the buffer might have audio from previous user of this
  // device.
  buffer_->Clear();

  // When starting again, drop all packets in the device and prepare it again
  // in case we are restarting from a pause state and need to flush old data.
  int error = wrapper_->PcmDrop(playback_handle_);
  if (error < 0 && error != -EAGAIN) {
    LOG(ERROR) << "Failure clearing playback device ("
               << wrapper_->PcmName(playback_handle_) << "): "
               << wrapper_->StrError(error);
    stop_stream_ = true;
    return;
  }

  error = wrapper_->PcmPrepare(playback_handle_);
  if (error < 0 && error != -EAGAIN) {
    LOG(ERROR) << "Failure preparing stream ("
               << wrapper_->PcmName(playback_handle_) << "): "
               << wrapper_->StrError(error);
    stop_stream_ = true;
    return;
  }

  // Ensure the first buffer is silence to avoid startup glitches.
  int buffer_size = GetAvailableFrames() * bytes_per_output_frame_;
  scoped_refptr<DataBuffer> silent_packet = new DataBuffer(buffer_size);
  silent_packet->set_data_size(buffer_size);
  memset(silent_packet->writable_data(), 0, silent_packet->data_size());
  buffer_->Append(silent_packet);
  WritePacket();

  // Start the callback chain.
  set_source_callback(callback);
  WriteTask();
}

void AlsaPcmOutputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reset the callback, so that it is not called anymore.
  set_source_callback(nullptr);
  weak_factory_.InvalidateWeakPtrs();

  TransitionTo(kIsStopped);
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void AlsaPcmOutputStream::Flush() {}

void AlsaPcmOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  volume_ = static_cast<float>(volume);
}

void AlsaPcmOutputStream::GetVolume(double* volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  *volume = volume_;
}

void AlsaPcmOutputStream::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  DCHECK(tick_clock);
  tick_clock_ = tick_clock;
}

void AlsaPcmOutputStream::BufferPacket(bool* source_exhausted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("audio", "AlsaPcmOutputStream::BufferPacket",
              [&](perfetto::EventContext ctx) {
                auto* event =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                auto* data = event->set_linux_alsa_output();
                data->set_forward_bytes(buffer_->forward_bytes());
                data->set_sample_rate(sample_rate_);
              });

  // If stopped, simulate a 0-length packet.
  if (stop_stream_) {
    buffer_->Clear();
    *source_exhausted = true;
    return;
  }

  *source_exhausted = false;

  // Request more data only when we run out of data in the buffer, because
  // WritePacket() consumes only the current chunk of data.
  if (!buffer_->forward_bytes()) {
    // Before making a request to source for data we need to determine the
    // delay for the requested data to be played.
    const base::TimeDelta delay =
        AudioTimestampHelper::FramesToTime(GetCurrentDelay(), sample_rate_);

    scoped_refptr<DataBuffer> packet = new DataBuffer(packet_size_);
    int frames_filled =
        RunDataCallback(delay, tick_clock_->NowTicks(), audio_bus_.get());

    size_t packet_size = frames_filled * bytes_per_frame_;
    DCHECK_LE(packet_size, packet_size_);

    // TODO(dalecurtis): Channel downmixing, upmixing, should be done in mixer;
    // volume adjust should use SSE optimized vector_fmul() prior to interleave.
    AudioBus* output_bus = audio_bus_.get();
    ChannelLayout output_channel_layout = channel_layout_;
    if (channel_mixer_) {
      output_bus = mixed_audio_bus_.get();
      channel_mixer_->Transform(audio_bus_.get(), output_bus);
      output_channel_layout = kDefaultOutputChannelLayout;
      // Adjust packet size for downmix.
      packet_size = packet_size / bytes_per_frame_ * bytes_per_output_frame_;
    }

    // Reorder channels for 5.0, 5.1, and 7.1 to match ALSA's channel order,
    // which has front center at channel index 4 and LFE at channel index 5.
    // See http://ffmpeg.org/pipermail/ffmpeg-cvslog/2011-June/038454.html.
    switch (output_channel_layout) {
      case CHANNEL_LAYOUT_5_0:
      case CHANNEL_LAYOUT_5_0_BACK:
        output_bus->SwapChannels(2, 3);
        output_bus->SwapChannels(3, 4);
        break;
      case CHANNEL_LAYOUT_5_1:
      case CHANNEL_LAYOUT_5_1_BACK:
      case CHANNEL_LAYOUT_7_1:
        output_bus->SwapChannels(2, 4);
        output_bus->SwapChannels(3, 5);
        break;
      default:
        break;
    }

    // Note: If this ever changes to output raw float the data must be clipped
    // and sanitized since it may come from an untrusted source such as NaCl.
    output_bus->Scale(volume_);
    output_bus->ToInterleaved<SignedInt16SampleTypeTraits>(
        frames_filled, reinterpret_cast<int16_t*>(packet->writable_data()));

    if (packet_size > 0) {
      packet->set_data_size(packet_size);
      // Add the packet to the buffer.
      buffer_->Append(packet);
    } else {
      *source_exhausted = true;
    }
  }
}

void AlsaPcmOutputStream::WritePacket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("audio", "AlsaPcmOutputStream::WritePacket");

  // If the device is in error, just eat the bytes.
  if (stop_stream_) {
    buffer_->Clear();
    return;
  }

  if (state() != kIsPlaying)
    return;

  CHECK_EQ(buffer_->forward_bytes() % bytes_per_output_frame_, 0u);

  const uint8_t* buffer_data;
  int buffer_size;
  if (buffer_->GetCurrentChunk(&buffer_data, &buffer_size)) {
    snd_pcm_sframes_t frames = std::min(
        static_cast<snd_pcm_sframes_t>(buffer_size / bytes_per_output_frame_),
        GetAvailableFrames());

    if (!frames)
      return;

    snd_pcm_sframes_t frames_written =
        wrapper_->PcmWritei(playback_handle_, buffer_data, frames);
    if (frames_written < 0) {
      // Attempt once to immediately recover from EINTR,
      // EPIPE (overrun/underrun), ESTRPIPE (stream suspended).  WritePacket
      // will eventually be called again, so eventual recovery will happen if
      // muliple retries are required.
      frames_written = wrapper_->PcmRecover(playback_handle_,
                                            frames_written,
                                            kPcmRecoverIsSilent);
      if (frames_written < 0) {
        if (frames_written != -EAGAIN) {
          LOG(ERROR) << "Failed to write to pcm device: "
                     << wrapper_->StrError(frames_written);
          RunErrorCallback(frames_written);
          stop_stream_ = true;
        }
      }
    } else {
      DCHECK_EQ(frames_written, frames);

      // Seek forward in the buffer after we've written some data to ALSA.
      buffer_->Seek(frames_written * bytes_per_output_frame_);
    }
  } else {
    // If nothing left to write and playback hasn't started yet, start it now.
    // This ensures that shorter sounds will still play.
    if (playback_handle_ &&
        (wrapper_->PcmState(playback_handle_) == SND_PCM_STATE_PREPARED) &&
        GetCurrentDelay() > 0) {
      wrapper_->PcmStart(playback_handle_);
    }
  }
}

void AlsaPcmOutputStream::WriteTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("audio", "AlsaPcmOutputStream::WriteTask");

  if (stop_stream_)
    return;

  if (state() == kIsStopped)
    return;

  bool source_exhausted;
  BufferPacket(&source_exhausted);
  WritePacket();

  ScheduleNextWrite(source_exhausted);
}

void AlsaPcmOutputStream::ScheduleNextWrite(bool source_exhausted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (stop_stream_ || state() != kIsPlaying)
    return;

  const uint32_t kTargetFramesAvailable = alsa_buffer_frames_ / 2;
  uint32_t available_frames = GetAvailableFrames();

  base::TimeDelta next_fill_time;
  if (buffer_->forward_bytes() && available_frames) {
    // If we've got data available and ALSA has room, deliver it immediately.
    next_fill_time = base::TimeDelta();
  } else if (buffer_->forward_bytes()) {
    // If we've got data available and no room, poll until room is available.
    // Polling in this manner allows us to ensure a more consistent callback
    // schedule.  In testing this yields a variance of +/- 5ms versus the non-
    // polling strategy which is around +/- 30ms and bimodal.
    next_fill_time = base::Milliseconds(5);
  } else if (available_frames < kTargetFramesAvailable) {
    // Schedule the next write for the moment when the available buffer of the
    // sound card hits |kTargetFramesAvailable|.
    next_fill_time = AudioTimestampHelper::FramesToTime(
        kTargetFramesAvailable - available_frames, sample_rate_);
  } else if (!source_exhausted) {
    // The sound card has |kTargetFramesAvailable| or more frames available.
    // Invoke the next write immediately to avoid underrun.
    next_fill_time = base::TimeDelta();
  } else {
    // The sound card has frames available, but our source is exhausted, so
    // avoid busy looping by delaying a bit.
    next_fill_time = base::Milliseconds(10);
  }

  task_runner_->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&AlsaPcmOutputStream::WriteTask,
                     weak_factory_.GetWeakPtr()),
      next_fill_time.is_zero() ? base::TimeTicks()
                               : base::TimeTicks::Now() + next_fill_time,
      base::subtle::DelayPolicy::kPrecise);
}

std::string AlsaPcmOutputStream::FindDeviceForChannels(uint32_t channels) {
  // Constants specified by the ALSA API for device hints.
  static const int kGetAllDevices = -1;
  static const char kPcmInterfaceName[] = "pcm";
  static const char kIoHintName[] = "IOID";
  static const char kNameHintName[] = "NAME";

  const char* wanted_device = GuessSpecificDeviceName(channels);
  if (!wanted_device)
    return std::string();

  std::string guessed_device;
  void** hints = nullptr;
  int error = wrapper_->DeviceNameHint(kGetAllDevices,
                                       kPcmInterfaceName,
                                       &hints);
  if (error == 0) {
    // NOTE: Do not early return from inside this if statement.  The
    // hints above need to be freed.
    for (void** hint_iter = hints; *hint_iter != nullptr; hint_iter++) {
      // Only examine devices that are output capable..  Valid values are
      // "Input", "Output", and nullptr which means both input and output.
      std::unique_ptr<char, base::FreeDeleter> io(
          wrapper_->DeviceNameGetHint(*hint_iter, kIoHintName));
      if (io != nullptr && strcmp(io.get(), "Input") == 0)
        continue;

      // Attempt to select the closest device for number of channels.
      std::unique_ptr<char, base::FreeDeleter> name(
          wrapper_->DeviceNameGetHint(*hint_iter, kNameHintName));
      if (strncmp(wanted_device, name.get(), strlen(wanted_device)) == 0) {
        guessed_device = name.get();
        break;
      }
    }

    // Destroy the hint now that we're done with it.
    wrapper_->DeviceNameFreeHint(hints);
    hints = nullptr;
  } else {
    LOG(ERROR) << "Unable to get hints for devices: "
               << wrapper_->StrError(error);
  }

  return guessed_device;
}

snd_pcm_sframes_t AlsaPcmOutputStream::GetCurrentDelay() {
  TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("audio"),
                    "AlsaPcmOutputStream::GetCurrentDelay");
  // Intermediate values saved for tracing.
  std::optional<snd_pcm_sframes_t> pcm_delay;
  std::optional<snd_pcm_sframes_t> available_frames;

  snd_pcm_sframes_t delay = -1;
  // Don't query ALSA's delay if we have underrun since it'll be jammed at some
  // non-zero value and potentially even negative!
  //
  // Also, if we're in the prepared state, don't query because that seems to
  // cause an I/O error when we do query the delay.
  snd_pcm_state_t pcm_state = wrapper_->PcmState(playback_handle_);
  if (pcm_state != SND_PCM_STATE_XRUN &&
      pcm_state != SND_PCM_STATE_PREPARED) {
    int error = wrapper_->PcmDelay(playback_handle_, &delay);
    if (error < 0) {
      // Assume a delay of zero and attempt to recover the device.
      delay = -1;
      error = wrapper_->PcmRecover(playback_handle_,
                                   error,
                                   kPcmRecoverIsSilent);
      if (error < 0) {
        LOG(ERROR) << "Failed querying delay: " << wrapper_->StrError(error);
      }
    }
    pcm_delay = delay;
  }

  // snd_pcm_delay() sometimes returns crazy values.  In this case return delay
  // of data we know currently is in ALSA's buffer.  Note: When the underlying
  // driver is PulseAudio based, certain configuration settings (e.g., tsched=1)
  // will generate much larger delay values than |alsa_buffer_frames_|, so only
  // clip if delay is truly crazy (> 10x expected).
  if (delay < 0 ||
      static_cast<snd_pcm_uframes_t>(delay) > alsa_buffer_frames_ * 10) {
    available_frames = GetAvailableFrames();
    delay = alsa_buffer_frames_ - *available_frames;
  }

  if (delay < 0) {
    delay = 0;
  }
  TRACE_EVENT_END(
      TRACE_DISABLED_BY_DEFAULT("audio"), [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_linux_alsa_output();
        data->set_getcurrentdelay_alsa_buffer_frames(alsa_buffer_frames_);
        data->set_getcurrentdelay_final_delay_frames(delay);
        if (pcm_delay) {
          data->set_getcurrentdelay_pcm_delay_frames(*pcm_delay);
        }
        if (available_frames) {
          data->set_getcurrentdelay_available_frames(*available_frames);
        }
      });
  return delay;
}

snd_pcm_sframes_t AlsaPcmOutputStream::GetAvailableFrames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (stop_stream_)
    return 0;

  // Find the number of frames queued in the sound device.
  snd_pcm_sframes_t available_frames =
      wrapper_->PcmAvailUpdate(playback_handle_);
  if (available_frames < 0) {
    available_frames = wrapper_->PcmRecover(playback_handle_,
                                            available_frames,
                                            kPcmRecoverIsSilent);
  }
  if (available_frames < 0) {
    LOG(ERROR) << "Failed querying available frames. Assuming 0: "
               << wrapper_->StrError(available_frames);
    return 0;
  }
  if (static_cast<uint32_t>(available_frames) > alsa_buffer_frames_ * 2) {
    LOG(ERROR) << "ALSA returned " << available_frames << " of "
               << alsa_buffer_frames_ << " frames available.";
    return alsa_buffer_frames_;
  }

  return available_frames;
}

snd_pcm_t* AlsaPcmOutputStream::AutoSelectDevice(unsigned int latency) {
  // For auto-selection:
  //   1) Attempt to open a device that best matches the number of channels
  //      requested.
  //   2) If that fails, attempt the "plug:" version of it in case ALSA can
  //      remap and do some software conversion to make it work.
  //   3) If that fails, attempt the "plug:" version of the guessed name in
  //      case ALSA can remap and do some software conversion to make it work.
  //   4) Fallback to kDefaultDevice.
  //   5) If that fails too, try the "plug:" version of kDefaultDevice.
  //   6) Give up.
  snd_pcm_t* handle = nullptr;
  device_name_ = FindDeviceForChannels(channels_);

  // Step 1.
  if (!device_name_.empty()) {
    if ((handle = alsa_util::OpenPlaybackDevice(
             wrapper_, device_name_.c_str(), channels_, sample_rate_,
             pcm_format_, latency)) != nullptr) {
      return handle;
    }

    // Step 2.
    device_name_ = kPlugPrefix + device_name_;
    if ((handle = alsa_util::OpenPlaybackDevice(
             wrapper_, device_name_.c_str(), channels_, sample_rate_,
             pcm_format_, latency)) != nullptr) {
      return handle;
    }

    // Step 3.
    device_name_ = GuessSpecificDeviceName(channels_);
    if (!device_name_.empty()) {
      device_name_ = kPlugPrefix + device_name_;
      if ((handle = alsa_util::OpenPlaybackDevice(
               wrapper_, device_name_.c_str(), channels_, sample_rate_,
               pcm_format_, latency)) != nullptr) {
        return handle;
      }
    }
  }

  // For the kDefaultDevice device, we can only reliably depend on 2-channel
  // output to have the correct ordering according to Lennart.  For the channel
  // formats that we know how to downmix from (3 channel to 8 channel), setup
  // downmixing.
  uint32_t default_channels = channels_;
  if (default_channels > 2) {
    default_channels = 2;
    channel_mixer_ = std::make_unique<ChannelMixer>(channel_layout_, channels_,
                                                    kDefaultOutputChannelLayout,
                                                    default_channels);
    mixed_audio_bus_ = AudioBus::Create(
        default_channels, audio_bus_->frames());
  }

  // Step 4.
  device_name_ = kDefaultDevice;
  if ((handle = alsa_util::OpenPlaybackDevice(
           wrapper_, device_name_.c_str(), default_channels, sample_rate_,
           pcm_format_, latency)) != nullptr) {
    return handle;
  }

  // Step 5.
  device_name_ = kPlugPrefix + device_name_;
  if ((handle = alsa_util::OpenPlaybackDevice(
           wrapper_, device_name_.c_str(), default_channels, sample_rate_,
           pcm_format_, latency)) != nullptr) {
    return handle;
  }

  // Unable to open any device.
  device_name_.clear();
  return nullptr;
}

bool AlsaPcmOutputStream::CanTransitionTo(InternalState to) {
  switch (state_) {
    case kCreated:
      return to == kIsOpened || to == kIsClosed || to == kInError;

    case kIsOpened:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kIsPlaying:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kIsStopped:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kInError:
      return to == kIsClosed || to == kInError;

    case kIsClosed:
    default:
      return false;
  }
}

AlsaPcmOutputStream::InternalState
AlsaPcmOutputStream::TransitionTo(InternalState to) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanTransitionTo(to)) {
    NOTREACHED_IN_MIGRATION()
        << "Cannot transition from: " << state_ << " to: " << to;
    state_ = kInError;
  } else {
    state_ = to;
  }
  return state_;
}

AlsaPcmOutputStream::InternalState AlsaPcmOutputStream::state() {
  return state_;
}

int AlsaPcmOutputStream::RunDataCallback(base::TimeDelta delay,
                                         base::TimeTicks delay_timestamp,
                                         AudioBus* audio_bus) {
  TRACE_EVENT(
      "audio", "AlsaPcmOutputStream::RunDataCallback",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_linux_alsa_output();
        data->set_source_request_playout_delay_us(delay.InMicroseconds());
      });

  if (source_callback_) {
    UMA_HISTOGRAM_COUNTS_1000("Media.Audio.Render.SystemDelay",
                              delay.InMilliseconds());
    return source_callback_->OnMoreData(BoundedDelay(delay), delay_timestamp,
                                        {}, audio_bus);
  }

  return 0;
}

void AlsaPcmOutputStream::RunErrorCallback(int code) {
  // TODO(dalecurtis): Consider sending a translated |code| value.
  if (source_callback_)
    source_callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
}

// Changes the AudioSourceCallback to proxy calls to.  Pass in nullptr to
// release ownership of the currently registered callback.
void AlsaPcmOutputStream::set_source_callback(AudioSourceCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_callback_ = callback;
}

}  // namespace media
