// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_AUDIO_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_AUDIO_SHARED_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "base/threading/simple_thread.h"
#include "media/base/audio_bus.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_audio_api.h"

struct PP_ThreadFunctions;

namespace ppapi {

class PPAPI_SHARED_EXPORT AudioCallbackCombined {
 public:
  AudioCallbackCombined();
  explicit AudioCallbackCombined(PPB_Audio_Callback_1_0 callback_1_0);
  explicit AudioCallbackCombined(PPB_Audio_Callback callback);

  ~AudioCallbackCombined();

  bool IsValid() const;

  void Run(void* sample_buffer,
           uint32_t buffer_size_in_bytes,
           PP_TimeDelta latency,
           void* user_data) const;

 private:
  PPB_Audio_Callback_1_0 callback_1_0_;
  PPB_Audio_Callback callback_;
};

// Implements the logic to map shared memory and run the audio thread signaled
// from the sync socket. Both the proxy and the renderer implementation use
// this code.
class PPAPI_SHARED_EXPORT PPB_Audio_Shared
    : public thunk::PPB_Audio_API,
      public base::DelegateSimpleThread::Delegate {
 public:
  PPB_Audio_Shared();

  PPB_Audio_Shared(const PPB_Audio_Shared&) = delete;
  PPB_Audio_Shared& operator=(const PPB_Audio_Shared&) = delete;

  virtual ~PPB_Audio_Shared();

  bool playing() const { return playing_; }

  // Sets the callback information that the background thread will use. This
  // is optional. Without a callback, the thread will not be run. This
  // non-callback mode is used in the renderer with the proxy, since the proxy
  // handles the callback entirely within the plugin process.
  void SetCallback(const AudioCallbackCombined& callback, void* user_data);

  // Configures the current state to be playing or not. The caller is
  // responsible for ensuring the new state is the opposite of the current one.
  //
  // This is the implementation for PPB_Audio.Start/StopPlayback, except that
  // it does not actually notify the audio system to stop playback, it just
  // configures our object to stop generating callbacks. The actual stop
  // playback request will be done in the derived classes and will be different
  // from the proxy and the renderer.
  void SetStartPlaybackState();
  void SetStopPlaybackState();

  // Sets the shared memory and socket handles. This will automatically start
  // playback if we're currently set to play.
  void SetStreamInfo(PP_Instance instance,
                     base::UnsafeSharedMemoryRegion shared_memory_region,
                     base::SyncSocket::ScopedHandle socket_handle,
                     PP_AudioSampleRate sample_rate,
                     int sample_frame_count);

  // Returns whether a thread can be created on the client context.
  // In trusted plugin, this should always return true, as it uses Chrome's
  // thread library. In NaCl plugin, this returns whether SetThreadFunctions
  // was invoked properly.
  static bool IsThreadFunctionReady();

  // Configures this class to run in a NaCl plugin.
  // If called, SetThreadFunctions() must be called before calling
  // SetStartPlaybackState() on any instance of this class.
  static void SetNaClMode();

  // NaCl has a special API for IRT code to create threads that can call back
  // into user code.
  static void SetThreadFunctions(const struct PP_ThreadFunctions* functions);

 private:
  // Starts execution of the audio thread.
  void StartThread();

  // Stop execution of the audio thread.
  void StopThread();

  // DelegateSimpleThread::Delegate implementation. Run on the audio thread.
  virtual void Run();

  // True if playing the stream.
  bool playing_;

  // Socket used to notify us when audio is ready to accept new samples. This
  // pointer is created in StreamCreated().
  std::unique_ptr<base::CancelableSyncSocket> socket_;

  // Sample buffer in shared memory. This pointer is created in
  // StreamCreated(). The memory is only mapped when the audio thread is
  // created.
  base::WritableSharedMemoryMapping shared_memory_;

  // The size of the sample buffer in bytes.
  size_t shared_memory_size_;

  // When the callback is set, this thread is spawned for calling it.
  std::unique_ptr<base::DelegateSimpleThread> audio_thread_;
  uintptr_t nacl_thread_id_;
  bool nacl_thread_active_;

  static void CallRun(void* self);

  // Callback to call when audio is ready to accept new samples.
  AudioCallbackCombined callback_;

  // User data pointer passed verbatim to the callback function.
  void* user_data_;

  // AudioBus for shuttling data across the shared memory.
  std::unique_ptr<media::AudioBus> audio_bus_;

  // Internal buffer for client's integer audio data.
  int client_buffer_size_bytes_;
  std::unique_ptr<uint8_t[]> client_buffer_;

  // The size (in bytes) of one second of audio data. Used to calculate latency.
  size_t bytes_per_second_;

  // Buffer index used to coordinate with the browser side audio receiver.
  uint32_t buffer_index_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_AUDIO_SHARED_H_
