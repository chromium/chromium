// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_thread.h"

#include <limits>
#include <ostream>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"

namespace media {

// AudioDeviceThread::Callback implementation

AudioDeviceThread::Callback::Callback(const AudioParameters& audio_parameters,
                                      uint32_t segment_length,
                                      uint32_t total_segments)
    : audio_parameters_(audio_parameters),
      memory_length_(
          base::CheckMul(segment_length, total_segments).ValueOrDie()),
      total_segments_(total_segments),
      segment_length_(segment_length) {
  CHECK_GT(total_segments_, 0u);
  thread_checker_.DetachFromThread();
}

AudioDeviceThread::Callback::~Callback() = default;

void AudioDeviceThread::Callback::InitializeOnAudioThread() {
  // Normally this function is called before the thread checker is used
  // elsewhere, but it's not guaranteed. DCHECK to ensure it was not used on
  // another thread before we get here.
  DCHECK(thread_checker_.CalledOnValidThread())
      << "Thread checker was attached on the wrong thread";
  MapSharedMemory();
}

bool AudioDeviceThread::Callback::WillConfirmReadsViaShmem() const {
  return false;
}

// AudioDeviceThread implementation

AudioDeviceThread::AudioDeviceThread(Callback* callback,
                                     base::SyncSocket::ScopedHandle socket,
                                     const char* thread_name,
                                     base::ThreadType thread_type)
    : callback_(callback),
      thread_name_(thread_name),
      socket_(std::move(socket)),
      send_socket_messages_(!callback_->WillConfirmReadsViaShmem()) {
#if defined(ARCH_CPU_X86)
  // Audio threads don't need a huge stack, they don't have a message loop and
  // they are used exclusively for polling the next frame of audio. See
  // https://crbug.com/1141563 for discussion.
  constexpr size_t kStackSize = 256 * 1024;
#else
  constexpr size_t kStackSize = 0;  // Default.
#endif

  CHECK(base::PlatformThread::CreateWithType(kStackSize, this, &thread_handle_,
                                             thread_type));

  DCHECK(!thread_handle_.is_null());
}

AudioDeviceThread::~AudioDeviceThread() {
  in_shutdown_ = true;
  socket_.Shutdown();
  if (thread_handle_.is_null())
    return;
  base::PlatformThread::Join(thread_handle_);
}

#if BUILDFLAG(IS_APPLE)
base::TimeDelta AudioDeviceThread::GetRealtimePeriod() {
  return callback_->buffer_duration();
}
#endif

void AudioDeviceThread::ThreadMain() {
  base::PlatformThread::SetName(thread_name_);
  callback_->InitializeOnAudioThread();

  uint32_t buffer_index = 0;
  while (true) {
    uint32_t pending_data = 0;
    size_t bytes_read = socket_.Receive(base::byte_span_from_ref(pending_data));
    if (bytes_read != sizeof(pending_data))
      break;

    // std::numeric_limits<uint32_t>::max() - 1 is a special signal that
    // tells us that the writer is done and will be closing the connection.
    // Nothing that happens after that is considered an error.
    if (pending_data == std::numeric_limits<uint32_t>::max() - 1) {
      in_shutdown_ = true;
      break;
    }

    // std::numeric_limits<uint32_t>::max() is a special signal which is
    // returned after the browser stops the output device in response to a
    // renderer side request.
    //
    // Avoid running Process() for the paused signal, we still need to update
    // the buffer index for synchronized buffers though.
    //
    // See comments in AudioOutputController::DoPause() for details on why.
    if (pending_data != std::numeric_limits<uint32_t>::max())
      callback_->Process(pending_data);

    // If `send_socket_messages_` is false, the socket messages are replaced by
    // a flag in shared memory which is handled in `callback_->Process()`.
    if (send_socket_messages_) {
      // The usage of the socket messages differs between input and output
      // cases.
      //
      // Input: We send a socket message to let the other end know that we have
      // read data, so that it can verify that it doesn't overwrite any data
      // before read.
      //
      // Output: Let the other end know which buffer we just filled. The
      // `buffer_index` is used to ensure the other end is getting the buffer it
      // expects. For more details on how this works see
      // `SyncReader::WaitUntilDataIsReady()`.
      ++buffer_index;
      size_t bytes_sent = socket_.Send(base::byte_span_from_ref(buffer_index));
      if (bytes_sent != sizeof(buffer_index)) {
        break;
      }
    }
  }

  if (!in_shutdown_.load()) {
    callback_->OnSocketError();
  }
}

}  // namespace media
