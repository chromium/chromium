// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEVICE_THREAD_H_
#define MEDIA_AUDIO_AUDIO_DEVICE_THREAD_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/sync_socket.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "build/buildflag.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

// Data transfer between browser and render process uses a combination
// of sync sockets and shared memory. To read from the socket and render
// data, we use a worker thread, a.k.a. the AudioDeviceThread, which reads
// data from the browser via the socket and fills the shared memory from the
// audio thread via the AudioDeviceThread::Callback interface/class.
class MEDIA_EXPORT AudioDeviceThread : public base::PlatformThread::Delegate {
 public:
  // This is the callback interface/base class that Audio[Output|Input]Device
  // implements to render input/output data. The callbacks run on the
  // thread owned by AudioDeviceThread.
  class Callback {
   public:
    Callback(const AudioParameters& audio_parameters,
             uint32_t segment_length,
             uint32_t total_segments);

    Callback(const Callback&) = delete;
    Callback& operator=(const Callback&) = delete;

    // One time initialization for the callback object on the audio thread.
    void InitializeOnAudioThread();

    // Derived implementations must map shared memory appropriately before
    // Process can be called.
    virtual void MapSharedMemory() = 0;

    // Called whenever we receive notifications about pending input data.
    virtual void Process(uint32_t pending_data) = 0;

    // Called if the socket closes outside of destruction.
    virtual void OnSocketError() = 0;

    base::TimeDelta buffer_duration() const {
      return audio_parameters_.GetBufferDuration();
    }

   protected:
    virtual ~Callback();

    // Protected so that derived classes can access directly.
    // The variables are 'const' since values are calculated/set in the
    // constructor and must never change.
    const AudioParameters audio_parameters_;

    const uint32_t memory_length_;
    const uint32_t total_segments_;
    const uint32_t segment_length_;

    // Detached in constructor and attached in InitializeOnAudioThread() which
    // is called on the audio device thread. Sub-classes can then use it for
    // various thread checking purposes.
    base::ThreadChecker thread_checker_;
  };

  // Creates and automatically starts the audio thread.
  AudioDeviceThread(Callback* callback,
                    base::SyncSocket::ScopedHandle socket,
                    const char* thread_name,
                    base::ThreadType thread_type);

  AudioDeviceThread(const AudioDeviceThread&) = delete;
  AudioDeviceThread& operator=(const AudioDeviceThread&) = delete;

  // This tells the audio thread to stop and clean up the data; this is a
  // synchronous process and the thread will stop before the method returns.
  // Blocking call, see base/threading/thread_restrictions.h.
  ~AudioDeviceThread() override;

 private:
#if BUILDFLAG(IS_APPLE)
  base::TimeDelta GetRealtimePeriod() final;
#endif
  void ThreadMain() final;

  // Set to true in destruction, but before closing the socket.
  base::AtomicFlag in_shutdown_;

  const raw_ptr<Callback> callback_;
  const char* thread_name_;
  base::CancelableSyncSocket socket_;
  base::PlatformThreadHandle thread_handle_;
};

}  // namespace media.

#endif  // MEDIA_AUDIO_AUDIO_DEVICE_THREAD_H_
