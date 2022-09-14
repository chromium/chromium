// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_IPC_H_
#define MEDIA_AUDIO_AUDIO_INPUT_IPC_H_

#include <stdint.h>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/sync_socket.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioProcessorControls;

// Contains IPC notifications for the state of the server side
// (AudioInputController) audio state changes and when an AudioInputController
// has been created.  Implemented by AudioInputDevice.
class MEDIA_EXPORT AudioInputIPCDelegate {
 public:
  // Called when an AudioInputController has been created.
  // See media/mojo/mojom/audio_data_pipe.mojom for documentation of
  // |handle| and |socket_handle|.
  virtual void OnStreamCreated(
      base::ReadOnlySharedMemoryRegion shared_memory_region,
      base::SyncSocket::ScopedHandle socket_handle,
      bool initially_muted) = 0;

  // Called when state of an audio stream has changed.
  virtual void OnError(AudioCapturerSource::ErrorCode code) = 0;

  // Called when an audio stream is muted or unmuted.
  virtual void OnMuted(bool is_muted) = 0;

  // Called when the AudioInputIPC object is going away and/or when the
  // IPC channel has been closed and no more IPC requests can be made.
  // Implementations should delete their owned AudioInputIPC instance
  // immediately.
  virtual void OnIPCClosed() = 0;

 protected:
  virtual ~AudioInputIPCDelegate();
};

// Provides IPC functionality for an AudioInputIPCDelegate (e.g., an
// AudioInputDevice).  The implementation should asynchronously deliver the
// messages to an AudioInputController object (or create one in the case of
// CreateStream()), that may live in a separate process.
class MEDIA_EXPORT AudioInputIPC {
 public:
  virtual ~AudioInputIPC();

  // Sends a request to create an AudioInputController object in the peer
  // process, and configures it to use the specified audio |params|.  The
  // |total_segments| indidates number of equal-lengthed segments in the shared
  // memory buffer.  Once the stream has been created, the implementation will
  // notify |delegate| by calling OnStreamCreated().
  virtual void CreateStream(AudioInputIPCDelegate* delegate,
                            const AudioParameters& params,
                            bool automatic_gain_control,
                            uint32_t total_segments) = 0;

  // Corresponds to a call to AudioInputController::Record() on the server side.
  virtual void RecordStream() = 0;

  // Sets the volume of the audio stream.
  virtual void SetVolume(double volume) = 0;

  // Sets the output device from which to cancel echo, if supported. The
  // |output_device_id| can be gotten from a device enumeration. Must not be
  // called before the stream has been successfully created.
  virtual void SetOutputDeviceForAec(const std::string& output_device_id) = 0;

  // If the input has built-in processing, returns a pointer to processing
  // controls. Valid after the stream has been created.
  virtual AudioProcessorControls* GetProcessorControls();

  // Closes the audio stream, which should shut down the corresponding
  // AudioInputController in the peer process.
  virtual void CloseStream() = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_INPUT_IPC_H_
