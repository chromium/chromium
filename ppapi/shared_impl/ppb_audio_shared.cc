// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_audio_shared.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_parameters.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {

namespace {
bool g_nacl_mode = false;
// Because this is static, the function pointers will be NULL initially.
PP_ThreadFunctions g_thread_functions;
}

AudioCallbackCombined::AudioCallbackCombined()
    : callback_1_0_(NULL), callback_(NULL) {}

AudioCallbackCombined::AudioCallbackCombined(
    PPB_Audio_Callback_1_0 callback_1_0)
    : callback_1_0_(callback_1_0), callback_(NULL) {}

AudioCallbackCombined::AudioCallbackCombined(PPB_Audio_Callback callback)
    : callback_1_0_(NULL), callback_(callback) {}

AudioCallbackCombined::~AudioCallbackCombined() {}

bool AudioCallbackCombined::IsValid() const {
  return callback_1_0_ || callback_;
}

void AudioCallbackCombined::Run(void* sample_buffer,
                                uint32_t buffer_size_in_bytes,
                                PP_TimeDelta latency,
                                void* user_data) const {
  if (callback_) {
    callback_(sample_buffer, buffer_size_in_bytes, latency, user_data);
  } else if (callback_1_0_) {
    callback_1_0_(sample_buffer, buffer_size_in_bytes, user_data);
  } else {
    NOTREACHED();
  }
}

PPB_Audio_Shared::PPB_Audio_Shared()
    : playing_(false),
      shared_memory_size_(0),
      nacl_thread_id_(0),
      nacl_thread_active_(false),
      user_data_(NULL),
      client_buffer_size_bytes_(0),
      bytes_per_second_(0),
      buffer_index_(0) {
}

PPB_Audio_Shared::~PPB_Audio_Shared() {
  // Shut down the socket to escape any hanging |Receive|s.
  if (socket_.get())
    socket_->Shutdown();
  StopThread();
}

void PPB_Audio_Shared::SetCallback(const AudioCallbackCombined& callback,
                                   void* user_data) {
  callback_ = callback;
  user_data_ = user_data;
}

void PPB_Audio_Shared::SetStartPlaybackState() {
  DCHECK(!playing_);
  DCHECK(!audio_thread_.get());
  DCHECK(!nacl_thread_active_);
  // If the socket doesn't exist, that means that the plugin has started before
  // the browser has had a chance to create all the shared memory info and
  // notify us. This is a common case. In this case, we just set the playing_
  // flag and the playback will automatically start when that data is available
  // in SetStreamInfo.
  playing_ = true;
  StartThread();
}

void PPB_Audio_Shared::SetStopPlaybackState() {
  DCHECK(playing_);
  StopThread();
  playing_ = false;
}

void PPB_Audio_Shared::SetStreamInfo(
    PP_Instance instance,
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::SyncSocket::ScopedHandle socket_handle,
    PP_AudioSampleRate sample_rate,
    int sample_frame_count) {
  socket_ =
      std::make_unique<base::CancelableSyncSocket>(std::move(socket_handle));
  shared_memory_size_ = media::ComputeAudioOutputBufferSize(
      kAudioOutputChannels, sample_frame_count);
  DCHECK_GE(shared_memory_region.GetSize(), shared_memory_size_);
  bytes_per_second_ =
      kAudioOutputChannels * (kBitsPerAudioOutputSample / 8) * sample_rate;
  buffer_index_ = 0;

  shared_memory_ = shared_memory_region.MapAt(0, shared_memory_size_);
  if (!shared_memory_.IsValid()) {
    PpapiGlobals::Get()->LogWithSource(
        instance,
        PP_LOGLEVEL_WARNING,
        std::string(),
        "Failed to map shared memory for PPB_Audio_Shared.");
  } else {
    media::AudioOutputBuffer* buffer =
        reinterpret_cast<media::AudioOutputBuffer*>(shared_memory_.memory());
    audio_bus_ = media::AudioBus::WrapMemory(kAudioOutputChannels,
                                             sample_frame_count, buffer->audio);
    // Setup integer audio buffer for user audio data.
    client_buffer_size_bytes_ = audio_bus_->frames() * audio_bus_->channels() *
                                kBitsPerAudioOutputSample / 8;
    client_buffer_.reset(new uint8_t[client_buffer_size_bytes_]);
  }

  StartThread();
}

void PPB_Audio_Shared::StartThread() {
  // Don't start the thread unless all our state is set up correctly.
  if (!playing_ || !callback_.IsValid() || !socket_.get() ||
      !shared_memory_.memory() || !audio_bus_.get() || !client_buffer_.get() ||
      bytes_per_second_ == 0)
    return;
  // Clear contents of shm buffer before starting audio thread. This will
  // prevent a burst of static if for some reason the audio thread doesn't
  // start up quickly enough.
  memset(shared_memory_.memory(), 0, shared_memory_size_);
  memset(client_buffer_.get(), 0, client_buffer_size_bytes_);

  if (g_nacl_mode) {
    // Use NaCl's special API for IRT code that creates threads that call back
    // into user code.
    if (!IsThreadFunctionReady())
      return;

    DCHECK(!nacl_thread_active_);
    int result =
        g_thread_functions.thread_create(&nacl_thread_id_, CallRun, this);
    DCHECK_EQ(0, result);
    nacl_thread_active_ = true;
  } else {
    DCHECK(!audio_thread_.get());
    audio_thread_ = std::make_unique<base::DelegateSimpleThread>(
        this, "plugin_audio_thread");
    audio_thread_->Start();
  }
}

void PPB_Audio_Shared::StopThread() {
  // In general, the audio thread should not do Pepper calls, but it might
  // anyway (for example, our Audio test does CallOnMainThread). If it did a
  // pepper call which acquires the lock (most of them do), and we try to shut
  // down the thread and Join it while holding the lock, we would deadlock. So
  // we give up the lock here so that the thread at least _can_ make Pepper
  // calls without causing deadlock.
  // IMPORTANT: This instance's thread state should be reset to uninitialized
  // before we release the proxy lock, so any calls from the plugin while we're
  // unlocked can't access the joined thread.
  if (g_nacl_mode) {
    if (nacl_thread_active_) {
      nacl_thread_active_ = false;
      int result =
          CallWhileUnlocked(g_thread_functions.thread_join, nacl_thread_id_);
      DCHECK_EQ(0, result);
    }
  } else {
    if (audio_thread_.get()) {
      auto local_audio_thread(std::move(audio_thread_));
      CallWhileUnlocked(
          base::BindOnce(&base::DelegateSimpleThread::Join,
                         base::Unretained(local_audio_thread.get())));
    }
  }
}

// static
bool PPB_Audio_Shared::IsThreadFunctionReady() {
  if (!g_nacl_mode)
    return true;

  return (g_thread_functions.thread_create != NULL &&
          g_thread_functions.thread_join != NULL);
}

// static
void PPB_Audio_Shared::SetNaClMode() {
  g_nacl_mode = true;
}

// static
void PPB_Audio_Shared::SetThreadFunctions(
    const struct PP_ThreadFunctions* functions) {
  DCHECK(g_nacl_mode);
  g_thread_functions = *functions;
}

// static
void PPB_Audio_Shared::CallRun(void* self) {
  PPB_Audio_Shared* audio = static_cast<PPB_Audio_Shared*>(self);
  audio->Run();
}

void PPB_Audio_Shared::Run() {
  int control_signal = 0;
  while (sizeof(control_signal) ==
         socket_->Receive(base::byte_span_from_ref(control_signal))) {
    // |buffer_index_| must track the number of Receive() calls.  See the Send()
    // call below for why this is important.
    ++buffer_index_;
    if (control_signal < 0)
      break;

    {
      TRACE_EVENT0("audio", "PPB_Audio_Shared::FireRenderCallback");
      media::AudioOutputBuffer* buffer =
          reinterpret_cast<media::AudioOutputBuffer*>(shared_memory_.memory());
      base::TimeDelta delay = base::Microseconds(buffer->params.delay_us);

      callback_.Run(client_buffer_.get(), client_buffer_size_bytes_,
                    delay.InSecondsF(), user_data_);
    }

    // Deinterleave the audio data into the shared memory as floats.
    static_assert(kBitsPerAudioOutputSample == 16,
                  "FromInterleaved expects 2 bytes.");
    audio_bus_->FromInterleaved<media::SignedInt16SampleTypeTraits>(
        reinterpret_cast<int16_t*>(client_buffer_.get()), audio_bus_->frames());

    // Let the other end know which buffer we just filled.  The buffer index is
    // used to ensure the other end is getting the buffer it expects.  For more
    // details on how this works see AudioSyncReader::WaitUntilDataIsReady().
    size_t bytes_sent = socket_->Send(base::byte_span_from_ref(buffer_index_));
    if (bytes_sent != sizeof(buffer_index_))
      break;
  }
}

}  // namespace ppapi
