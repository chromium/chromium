// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/pulse/pulse_util.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <type_traits>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "build/branding_buildflags.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_timestamp_helper.h"

#if defined(DLOPEN_PULSEAUDIO)
#include "media/audio/pulse/pulse_stubs.h"

using media_audio_pulse::kModulePulse;
using media_audio_pulse::InitializeStubs;
using media_audio_pulse::StubPathMap;
#endif  // defined(DLOPEN_PULSEAUDIO)

namespace media {

namespace pulse {

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kBrowserDisplayName[] = "google-chrome";
#define PRODUCT_STRING "Google Chrome"
#else
constexpr char kBrowserDisplayName[] = "chromium-browser";
#define PRODUCT_STRING "Chromium"
#endif

#if defined(DLOPEN_PULSEAUDIO)
static const base::FilePath::CharType kPulseLib[] =
    FILE_PATH_LITERAL("libpulse.so.0");
#endif

void DestroyMainloop(pa_threaded_mainloop* mainloop) {
  pa_threaded_mainloop_stop(mainloop);
  pa_threaded_mainloop_free(mainloop);
}

void DestroyContext(pa_context* context) {
  pa_context_set_state_callback(context, nullptr, nullptr);
  pa_context_disconnect(context);
  pa_context_unref(context);
}

pa_channel_position ChromiumToPAChannelPosition(Channels channel) {
  switch (channel) {
    // PulseAudio does not differentiate between left/right and
    // stereo-left/stereo-right, both translate to front-left/front-right.
    case LEFT:
      return PA_CHANNEL_POSITION_FRONT_LEFT;
    case RIGHT:
      return PA_CHANNEL_POSITION_FRONT_RIGHT;
    case CENTER:
      return PA_CHANNEL_POSITION_FRONT_CENTER;
    case LFE:
      return PA_CHANNEL_POSITION_LFE;
    case BACK_LEFT:
      return PA_CHANNEL_POSITION_REAR_LEFT;
    case BACK_RIGHT:
      return PA_CHANNEL_POSITION_REAR_RIGHT;
    case LEFT_OF_CENTER:
      return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    case RIGHT_OF_CENTER:
      return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
    case BACK_CENTER:
      return PA_CHANNEL_POSITION_REAR_CENTER;
    case SIDE_LEFT:
      return PA_CHANNEL_POSITION_SIDE_LEFT;
    case SIDE_RIGHT:
      return PA_CHANNEL_POSITION_SIDE_RIGHT;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid channel: " << channel;
      return PA_CHANNEL_POSITION_INVALID;
  }
}

class ScopedPropertyList {
 public:
  ScopedPropertyList() : property_list_(pa_proplist_new()) {}

  ScopedPropertyList(const ScopedPropertyList&) = delete;
  ScopedPropertyList& operator=(const ScopedPropertyList&) = delete;

  pa_proplist* get() const { return property_list_.get(); }

 private:
  using deleter =
      std::integral_constant<decltype(pa_proplist_free)*, pa_proplist_free>;
  std::unique_ptr<pa_proplist, deleter> property_list_;
};

struct InputBusData {
  InputBusData(pa_threaded_mainloop* loop, const std::string& name)
      : loop_(loop), name_(name), bus_() {}

  const raw_ptr<pa_threaded_mainloop> loop_;
  const raw_ref<const std::string> name_;
  std::string bus_;
};

struct OutputBusData {
  OutputBusData(pa_threaded_mainloop* loop, const std::string& bus)
      : loop_(loop), name_(), bus_(bus) {}

  const raw_ptr<pa_threaded_mainloop> loop_;
  std::string name_;
  const raw_ref<const std::string> bus_;
};

void InputBusCallback(pa_context* context,
                      const pa_source_info* info,
                      int error,
                      void* user_data) {
  InputBusData* data = static_cast<InputBusData*>(user_data);

  if (error) {
    // We have checked all the devices now.
    pa_threaded_mainloop_signal(data->loop_, 0);
    return;
  }

  if (strcmp(info->name, data->name_->c_str()) == 0 &&
      pa_proplist_contains(info->proplist, PA_PROP_DEVICE_BUS_PATH)) {
    data->bus_ = pa_proplist_gets(info->proplist, PA_PROP_DEVICE_BUS_PATH);
  }
}

void OutputBusCallback(pa_context* context,
                       const pa_sink_info* info,
                       int error,
                       void* user_data) {
  OutputBusData* data = static_cast<OutputBusData*>(user_data);

  if (error) {
    // We have checked all the devices now.
    pa_threaded_mainloop_signal(data->loop_, 0);
    return;
  }

  if (pa_proplist_contains(info->proplist, PA_PROP_DEVICE_BUS_PATH) &&
      strcmp(pa_proplist_gets(info->proplist, PA_PROP_DEVICE_BUS_PATH),
             data->bus_->c_str()) == 0) {
    data->name_ = info->name;
  }
}

struct DefaultDevicesData {
  explicit DefaultDevicesData(pa_threaded_mainloop* loop) : loop_(loop) {}
  std::string input_;
  std::string output_;
  const raw_ptr<pa_threaded_mainloop> loop_;
};

void GetDefaultDeviceIdCallback(pa_context* c,
                                const pa_server_info* info,
                                void* userdata) {
  DefaultDevicesData* data = static_cast<DefaultDevicesData*>(userdata);
  if (info->default_source_name)
    data->input_ = info->default_source_name;
  if (info->default_sink_name)
    data->output_ = info->default_sink_name;
  pa_threaded_mainloop_signal(data->loop_, 0);
}

struct MonitorSourceData {
  explicit MonitorSourceData(pa_threaded_mainloop* loop) : loop_(loop) {}
  const raw_ptr<pa_threaded_mainloop> loop_;
  std::string monitor_source_name_;
};

// Callback used by GetMonitorSourceNameForSink(). `info` contains information
// about the queried sink, in particular, the name of the source which acts as a
// monitor for the sink.
void GetMonitorSourceNameForSinkCallback(pa_context* context,
                                         const pa_sink_info* info,
                                         int eol,
                                         void* userdata) {
  MonitorSourceData* data = static_cast<MonitorSourceData*>(userdata);
  if (!eol) {
    data->monitor_source_name_ = info->monitor_source_name;
  }
  pa_threaded_mainloop_signal(data->loop_, 0);
}

struct ContextStartupData {
  raw_ptr<base::WaitableEvent> context_wait;
  raw_ptr<pa_threaded_mainloop, DanglingUntriaged> pa_mainloop;
};

void SignalReadyOrErrorStateCallback(pa_context* context, void* context_data) {
  auto context_state = pa_context_get_state(context);
  auto* data = static_cast<ContextStartupData*>(context_data);
  if (!PA_CONTEXT_IS_GOOD(context_state) || context_state == PA_CONTEXT_READY)
    data->context_wait->Signal();
  pa_threaded_mainloop_signal(data->pa_mainloop, 0);
}

}  // namespace

bool InitPulse(pa_threaded_mainloop** mainloop, pa_context** context) {
#if defined(DLOPEN_PULSEAUDIO)
  StubPathMap paths;

  // Check if the pulse library is available.
  paths[kModulePulse].push_back(kPulseLib);
  if (!InitializeStubs(paths)) {
    VLOG(1) << "Failed on loading the Pulse library and symbols";
    return false;
  }
#endif  // defined(DLOPEN_PULSEAUDIO)

  // The setup order below follows the pattern used by pa_simple_new():
  // https://github.com/pulseaudio/pulseaudio/blob/master/src/pulse/simple.c

  // Create a mainloop API and connect to the default server.
  // The mainloop is the internal asynchronous API event loop.
  pa_threaded_mainloop* pa_mainloop = pa_threaded_mainloop_new();
  if (!pa_mainloop)
    return false;

  pa_mainloop_api* pa_mainloop_api = pa_threaded_mainloop_get_api(pa_mainloop);
  pa_context* pa_context =
      pa_context_new(pa_mainloop_api, PRODUCT_STRING " input");
  if (!pa_context) {
    pa_threaded_mainloop_free(pa_mainloop);
    return false;
  }

  // We can't rely on pa_threaded_mainloop_wait() for PulseAudio startup since
  // it can hang indefinitely. Instead we use a WaitableEvent to time out the
  // startup process if it takes too long.
  base::WaitableEvent context_wait;
  ContextStartupData data = {&context_wait, pa_mainloop};

  pa_context_set_state_callback(pa_context, &SignalReadyOrErrorStateCallback,
                                &data);

  if (pa_context_connect(pa_context, nullptr, PA_CONTEXT_NOAUTOSPAWN,
                         nullptr)) {
    VLOG(1) << "Failed to connect to the context.  Error: "
            << pa_strerror(pa_context_errno(pa_context));
    DestroyContext(pa_context);
    data = {nullptr, nullptr};
    pa_threaded_mainloop_free(pa_mainloop);
    return false;
  }

  // Lock the event loop object, effectively blocking the event loop thread
  // from processing events. This is necessary.
  auto mainloop_lock = std::make_unique<AutoPulseLock>(pa_mainloop);

  // Start the threaded mainloop after everything has been configured.
  if (pa_threaded_mainloop_start(pa_mainloop)) {
    DestroyContext(pa_context);
    mainloop_lock.reset();
    data = {nullptr, nullptr};
    DestroyMainloop(pa_mainloop);
    return false;
  }

  // Don't hold the mainloop lock while waiting for the context to become ready,
  // or we'll never complete since PulseAudio can't continue working.
  mainloop_lock.reset();

  // Wait for up to 5 seconds for pa_context to become ready. We'll be signaled
  // by the SignalReadyOrErrorStateCallback that we setup above.
  //
  // We've chosen a timeout value of 5 seconds because this can be executed at
  // browser startup (other times it's during audio process startup). In the
  // normal case, this should only take ~50ms, but we've seen some test bots
  // hang indefinitely when the pulse daemon can't be started.
  constexpr base::TimeDelta kStartupTimeout = base::Seconds(5);
  const bool was_signaled = context_wait.TimedWait(kStartupTimeout);

  // Require the mainloop lock before checking the context state.
  mainloop_lock = std::make_unique<AutoPulseLock>(pa_mainloop);

  auto context_state = pa_context_get_state(pa_context);
  if (context_state != PA_CONTEXT_READY) {
    if (!was_signaled)
      VLOG(1) << "Timed out trying to connect to PulseAudio.";
    else
      VLOG(1) << "Failed to connect to PulseAudio: " << context_state;
    DestroyContext(pa_context);
    mainloop_lock.reset();
    data = {nullptr, nullptr};
    DestroyMainloop(pa_mainloop);
    return false;
  }

  // Replace our function local state callback with a global appropriate one.
  pa_context_set_state_callback(pa_context, &pulse::ContextStateCallback,
                                pa_mainloop);

  *mainloop = pa_mainloop;
  *context = pa_context;
  return true;
}

void DestroyPulse(pa_threaded_mainloop* mainloop, pa_context* context) {
  DCHECK(mainloop);
  DCHECK(context);

  {
    AutoPulseLock auto_lock(mainloop);
    DestroyContext(context);
  }

  DestroyMainloop(mainloop);
}

// static, pa_stream_success_cb_t
void StreamSuccessCallback(pa_stream* s, int error, void* mainloop) {
  pa_threaded_mainloop* pa_mainloop =
      static_cast<pa_threaded_mainloop*>(mainloop);
  pa_threaded_mainloop_signal(pa_mainloop, 0);
}

// pa_context_success_cb_t
void ContextSuccessCallback(pa_context* context, int success, void* mainloop) {
  pa_threaded_mainloop* pa_mainloop =
      static_cast<pa_threaded_mainloop*>(mainloop);
  if (!success) {
    LOG(ERROR) << "Context operation failed.";
  }
  pa_threaded_mainloop_signal(pa_mainloop, 0);
}

// |pa_context| and |pa_stream| state changed cb.
void ContextStateCallback(pa_context* context, void* mainloop) {
  pa_threaded_mainloop* pa_mainloop =
      static_cast<pa_threaded_mainloop*>(mainloop);
  pa_threaded_mainloop_signal(pa_mainloop, 0);
}

pa_channel_map ChannelLayoutToPAChannelMap(ChannelLayout channel_layout) {
  pa_channel_map channel_map;
  if (channel_layout == CHANNEL_LAYOUT_MONO) {
    // CHANNEL_LAYOUT_MONO only specifies audio on the C channel, but we
    // want PulseAudio to play single-channel audio on more than just that.
    pa_channel_map_init_mono(&channel_map);
  } else {
    pa_channel_map_init(&channel_map);

    channel_map.channels = ChannelLayoutToChannelCount(channel_layout);
    for (Channels ch = LEFT; ch <= CHANNELS_MAX;
         ch = static_cast<Channels>(ch + 1)) {
      int channel_index = ChannelOrder(channel_layout, ch);
      if (channel_index < 0)
        continue;

      channel_map.map[channel_index] = ChromiumToPAChannelPosition(ch);
    }
  }

  return channel_map;
}

bool WaitForOperationCompletion(pa_threaded_mainloop* mainloop,
                                pa_operation* operation,
                                pa_context* optional_context,
                                pa_stream* optional_stream) {
  if (!operation) {
    LOG(ERROR) << "pa_operation is nullptr.";
    return false;
  }

  while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
    if (optional_context) {
      pa_context_state_t context_state = pa_context_get_state(optional_context);
      if (!PA_CONTEXT_IS_GOOD(context_state)) {
        LOG(ERROR) << "pa_context went bad while waiting: state="
                   << context_state << ", error="
                   << pa_strerror(pa_context_errno(optional_context));
        pa_operation_cancel(operation);
        pa_operation_unref(operation);
        return false;
      }
    }

    if (optional_stream) {
      pa_stream_state_t stream_state = pa_stream_get_state(optional_stream);
      if (!PA_STREAM_IS_GOOD(stream_state)) {
        LOG(ERROR) << "pa_stream went bad while waiting: " << stream_state;
        pa_operation_cancel(operation);
        pa_operation_unref(operation);
        return false;
      }
    }

    pa_threaded_mainloop_wait(mainloop);
  }

  pa_operation_unref(operation);
  return true;
}

base::TimeDelta GetHardwareLatency(pa_stream* stream) {
  DCHECK(stream);
  int negative = 0;
  pa_usec_t latency_micros = 0;
  if (pa_stream_get_latency(stream, &latency_micros, &negative) != 0)
    return base::TimeDelta();

  if (negative)
    return base::TimeDelta();

  return base::Microseconds(latency_micros);
}

// Helper macro for CreateInput/OutputStream() to avoid code spam and
// string bloat.
#define RETURN_ON_FAILURE(expression, message) do { \
  if (!(expression)) { \
    DLOG(ERROR) << message; \
    return false; \
  } \
} while (0)

bool CreateInputStream(pa_threaded_mainloop* mainloop,
                       pa_context* context,
                       raw_ptr<pa_stream>* stream,
                       const AudioParameters& params,
                       const std::string& device_id,
                       pa_stream_notify_cb_t stream_callback,
                       void* user_data) {
  DCHECK(mainloop);
  DCHECK(context);

  // Set sample specifications.
  pa_sample_spec sample_specifications;

  // FIXME: This should be PA_SAMPLE_FLOAT32, but there is more work needed in
  // PulseAudioInputStream to support this.
  static_assert(kInputSampleFormat == kSampleFormatS16,
                "Only 16-bit input supported.");
  sample_specifications.format = PA_SAMPLE_S16LE;
  sample_specifications.rate = params.sample_rate();
  sample_specifications.channels = params.channels();

  // Get channel mapping and open recording stream.
  pa_channel_map source_channel_map = ChannelLayoutToPAChannelMap(
      params.channel_layout());
  pa_channel_map* map =
      (source_channel_map.channels != 0) ? &source_channel_map : nullptr;

  // Create a new recording stream and
  // tells PulseAudio what the stream icon should be.
  ScopedPropertyList property_list;
  pa_proplist_sets(property_list.get(), PA_PROP_APPLICATION_ICON_NAME,
                   kBrowserDisplayName);
  *stream = pa_stream_new_with_proplist(context, "RecordStream",
                                        &sample_specifications, map,
                                        property_list.get());
  RETURN_ON_FAILURE(*stream, "failed to create PA recording stream");

  pa_stream_set_state_callback(*stream, stream_callback, user_data);

  // Set server-side capture buffer metrics. Detailed documentation on what
  // values should be chosen can be found at
  // freedesktop.org/software/pulseaudio/doxygen/structpa__buffer__attr.html.
  pa_buffer_attr buffer_attributes;
  const unsigned int buffer_size = params.GetBytesPerBuffer(kInputSampleFormat);
  buffer_attributes.maxlength = static_cast<uint32_t>(-1);
  buffer_attributes.tlength = buffer_size;
  buffer_attributes.minreq = buffer_size;
  buffer_attributes.prebuf = static_cast<uint32_t>(-1);
  buffer_attributes.fragsize = buffer_size;
  int flags = PA_STREAM_AUTO_TIMING_UPDATE |
              PA_STREAM_INTERPOLATE_TIMING |
              PA_STREAM_ADJUST_LATENCY |
              PA_STREAM_START_CORKED;
  RETURN_ON_FAILURE(
      pa_stream_connect_record(
          *stream,
          device_id == AudioDeviceDescription::kDefaultDeviceId
              ? nullptr
              : device_id.c_str(),
          &buffer_attributes, static_cast<pa_stream_flags_t>(flags)) == 0,
      "pa_stream_connect_record FAILED ");

  // Wait for the stream to be ready.
  while (true) {
    pa_stream_state_t stream_state = pa_stream_get_state(*stream);
    RETURN_ON_FAILURE(
        PA_STREAM_IS_GOOD(stream_state), "Invalid PulseAudio stream state");
    if (stream_state == PA_STREAM_READY)
        break;
    pa_threaded_mainloop_wait(mainloop);
  }

  return true;
}

bool CreateOutputStream(raw_ptr<pa_threaded_mainloop>* mainloop,
                        raw_ptr<pa_context>* context,
                        raw_ptr<pa_stream>* stream,
                        const AudioParameters& params,
                        const std::string& device_id,
                        const std::string& app_name,
                        pa_stream_notify_cb_t stream_callback,
                        pa_stream_request_cb_t write_callback,
                        void* user_data) {
  DCHECK(!*mainloop);
  DCHECK(!*context);

  *mainloop = pa_threaded_mainloop_new();
  RETURN_ON_FAILURE(*mainloop, "Failed to create PulseAudio main loop.");

  pa_mainloop_api* pa_mainloop_api = pa_threaded_mainloop_get_api(*mainloop);
  *context = pa_context_new(
      pa_mainloop_api, app_name.empty() ? PRODUCT_STRING : app_name.c_str());
  RETURN_ON_FAILURE(*context, "Failed to create PulseAudio context.");

  // A state callback must be set before calling pa_threaded_mainloop_lock() or
  // pa_threaded_mainloop_wait() calls may lead to dead lock.
  pa_context_set_state_callback(*context, &ContextStateCallback, *mainloop);

  // Lock the main loop while setting up the context.  Failure to do so may lead
  // to crashes as the PulseAudio thread tries to run before things are ready.
  AutoPulseLock auto_lock(*mainloop);

  RETURN_ON_FAILURE(pa_threaded_mainloop_start(*mainloop) == 0,
                    "Failed to start PulseAudio main loop.");
  RETURN_ON_FAILURE(pa_context_connect(*context, nullptr,
                                       PA_CONTEXT_NOAUTOSPAWN, nullptr) == 0,
                    "Failed to connect PulseAudio context.");

  // Wait until |pa_context_| is ready.  pa_threaded_mainloop_wait() must be
  // called after pa_context_get_state() in case the context is already ready,
  // otherwise pa_threaded_mainloop_wait() will hang indefinitely.
  while (true) {
    pa_context_state_t context_state = pa_context_get_state(*context);
    RETURN_ON_FAILURE(PA_CONTEXT_IS_GOOD(context_state),
                      "Invalid PulseAudio context state.");
    if (context_state == PA_CONTEXT_READY)
      break;
    pa_threaded_mainloop_wait(*mainloop);
  }

  // Set sample specifications.
  pa_sample_spec sample_specifications;
  sample_specifications.format = PA_SAMPLE_FLOAT32;
  sample_specifications.rate = params.sample_rate();
  sample_specifications.channels = params.channels();

  // Get channel mapping.
  pa_channel_map* map = nullptr;
  pa_channel_map source_channel_map = ChannelLayoutToPAChannelMap(
      params.channel_layout());
  if (source_channel_map.channels != 0) {
    // The source data uses a supported channel map so we will use it rather
    // than the default channel map (nullptr).
    map = &source_channel_map;
  }

  // Open playback stream and
  // tell PulseAudio what the stream icon should be.
  ScopedPropertyList property_list;
  pa_proplist_sets(property_list.get(), PA_PROP_APPLICATION_ICON_NAME,
                   kBrowserDisplayName);
  *stream = pa_stream_new_with_proplist(
      *context, "Playback", &sample_specifications, map, property_list.get());
  RETURN_ON_FAILURE(*stream, "failed to create PA playback stream");

  pa_stream_set_state_callback(*stream, stream_callback, user_data);

  // Even though we start the stream corked above, PulseAudio will issue one
  // stream request after setup.  write_callback() must fulfill the write.
  pa_stream_set_write_callback(*stream, write_callback, user_data);

  // Pulse is very finicky with the small buffer sizes used by Chrome.  The
  // settings below are mostly found through trial and error.  Essentially we
  // want Pulse to auto size its internal buffers, but call us back nearly every
  // |minreq| bytes.  |tlength| should be a multiple of |minreq|; too low and
  // Pulse will issue callbacks way too fast, too high and we don't get
  // callbacks frequently enough.
  //
  // Setting |minreq| to the exact buffer size leads to more callbacks than
  // necessary, so we've clipped it to half the buffer size.  Regardless of the
  // requested amount, we'll always fill |params.GetBytesPerBuffer()| though.
  size_t buffer_size = params.GetBytesPerBuffer(kSampleFormatF32);
  pa_buffer_attr pa_buffer_attributes;
  pa_buffer_attributes.maxlength = static_cast<uint32_t>(-1);
  pa_buffer_attributes.minreq = buffer_size / 2;
  pa_buffer_attributes.prebuf = static_cast<uint32_t>(-1);
  pa_buffer_attributes.tlength = buffer_size * 3;
  pa_buffer_attributes.fragsize = static_cast<uint32_t>(-1);

  // Connect playback stream.  Like pa_buffer_attr, the pa_stream_flags have a
  // huge impact on the performance of the stream and were chosen through trial
  // and error.
  RETURN_ON_FAILURE(
      pa_stream_connect_playback(
          *stream,
          device_id == AudioDeviceDescription::kDefaultDeviceId
              ? nullptr
              : device_id.c_str(),
          &pa_buffer_attributes,
          static_cast<pa_stream_flags_t>(
              PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY |
              PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_NOT_MONOTONIC |
              PA_STREAM_START_CORKED),
          nullptr, nullptr) == 0,
      "pa_stream_connect_playback FAILED ");

  // Wait for the stream to be ready.
  while (true) {
    pa_stream_state_t stream_state = pa_stream_get_state(*stream);
    RETURN_ON_FAILURE(
        PA_STREAM_IS_GOOD(stream_state), "Invalid PulseAudio stream state");
    if (stream_state == PA_STREAM_READY)
      break;
    pa_threaded_mainloop_wait(*mainloop);
  }

  return true;
}

// Mutes all audio output sinks except the specified sink.
void MuteAllSinksExcept(pa_threaded_mainloop* mainloop,
                        pa_context* context,
                        const std::string& exclude_sink_name) {
  CHECK(mainloop);
  CHECK(context);
  AutoPulseLock lock(mainloop);

  // Retrieve a list of all sinks from the PulseAudio context
  pa_operation* op = pa_context_get_sink_info_list(
      context,
      // Define the callback to process each sink information received
      [](pa_context* c, const pa_sink_info* i, int eol, void* userdata) {
        if (eol != 0) {
          return;  // Handle end of list or error
        }
        if (!eol) {
          std::string* exclude_sink_name = static_cast<std::string*>(userdata);
          // Check if current sink's name matches the exclude_sink_name
          if (i->name != *exclude_sink_name) {
            pa_context_set_sink_mute_by_index(
                c, i->index, 1, /*callback=*/nullptr,
                /*userdata=*/nullptr);  // Mute the sink
          }
        }
      },
      (void*)&exclude_sink_name);

  WaitForOperationCompletion(mainloop, op, context);
  // Clean up the operation after completion
  if (op) {
    pa_operation_unref(op);
  }
}

// Unmutes all audio output sinks in the system.
void UnmuteAllSinks(pa_threaded_mainloop* mainloop, pa_context* context) {
  CHECK(mainloop);
  CHECK(context);
  // Lock the mainloop to ensure thread safety when accessing the context.
  AutoPulseLock lock(mainloop);

  // Request a list of all sinks from the PulseAudio context.
  pa_operation* op = pa_context_get_sink_info_list(
      context,
      [](pa_context* c, const pa_sink_info* i, int eol, void* userdata) {
        // eol != 0 indicates the end of list or an error. We return early.
        if (eol != 0) {
          return;
        }

        pa_operation* unmute_op = pa_context_set_sink_mute_by_index(
            c, i->index, 0,  // 0 means unmute
            [](pa_context* c, int success, void* userdata) {
              // This callback ensures the operation completes
              pa_threaded_mainloop_signal((pa_threaded_mainloop*)userdata, 0);
            },
            userdata  // Pass the mainloop as userdata
        );

        if (unmute_op) {
          pa_operation_unref(unmute_op);
        }
      },
      mainloop  // Pass mainloop as userdata
  );

  WaitForOperationCompletion(mainloop, op, context);
  // Wait for the operation to complete to ensure all sinks are unmuted.
  if (op) {
    pa_operation_unref(op);
  }
}

std::string GetBusOfInput(pa_threaded_mainloop* mainloop,
                          pa_context* context,
                          const std::string& name) {
  DCHECK(mainloop);
  DCHECK(context);
  AutoPulseLock auto_lock(mainloop);
  InputBusData data(mainloop, name);
  pa_operation* operation =
      pa_context_get_source_info_list(context, InputBusCallback, &data);
  WaitForOperationCompletion(mainloop, operation, context);
  return data.bus_;
}

std::string GetOutputCorrespondingTo(pa_threaded_mainloop* mainloop,
                                     pa_context* context,
                                     const std::string& bus) {
  DCHECK(mainloop);
  DCHECK(context);
  AutoPulseLock auto_lock(mainloop);
  OutputBusData data(mainloop, bus);
  pa_operation* operation =
      pa_context_get_sink_info_list(context, OutputBusCallback, &data);
  WaitForOperationCompletion(mainloop, operation, context);
  return data.name_;
}

std::string GetRealDefaultDeviceId(pa_threaded_mainloop* mainloop,
                                   pa_context* context,
                                   RequestType type) {
  DCHECK(mainloop);
  DCHECK(context);
  AutoPulseLock auto_lock(mainloop);
  DefaultDevicesData data(mainloop);
  pa_operation* operation =
      pa_context_get_server_info(context, &GetDefaultDeviceIdCallback, &data);
  WaitForOperationCompletion(mainloop, operation, context);
  return (type == RequestType::INPUT) ? data.input_ : data.output_;
}

std::string GetMonitorSourceNameForSink(pa_threaded_mainloop* mainloop,
                                        pa_context* context,
                                        const std::string& sink_name) {
  CHECK(mainloop);
  CHECK(context);
  CHECK(!sink_name.empty());
  AutoPulseLock auto_lock(mainloop);
  MonitorSourceData data(mainloop);
  pa_operation* operation = pa_context_get_sink_info_by_name(
      context, sink_name.c_str(), &GetMonitorSourceNameForSinkCallback, &data);
  WaitForOperationCompletion(mainloop, operation, context);
  return data.monitor_source_name_;
}

#undef RETURN_ON_FAILURE

}  // namespace pulse

}  // namespace media
