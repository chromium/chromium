// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_IPC_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_IPC_H_

#include <string>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "base/unguessable_token.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"
#include "media/base/output_device_info.h"

namespace media {

// Contains IPC notifications for the state of the server side
// (AudioOutputController) audio state changes and when an AudioOutputController
// has been created.  Implemented by AudioOutputDevice.
class MEDIA_EXPORT AudioOutputIPCDelegate {
 public:
  // Called when state of an audio stream has changed.
  virtual void OnError() = 0;

  // Called when an authorization request for an output device has been
  // completed. The AudioOutputIPCDelegate will delete the AudioOutputIPC, if
  // |device_status| is not OUTPUT_DEVICE_STATUS_OK.
  virtual void OnDeviceAuthorized(OutputDeviceStatus device_status,
                                  const media::AudioParameters& output_params,
                                  const std::string& matched_device_id) = 0;

  // Called when an audio stream has been created.
  // See media/mojo/mojom/audio_data_pipe.mojom for documentation of
  // |handle| and |socket_handle|. |playing_automatically| indicates if the
  // AudioOutputIPCDelegate is playing right away due to an earlier call to
  // Play();
  virtual void OnStreamCreated(
      base::UnsafeSharedMemoryRegion shared_memory_region,
      base::SyncSocket::Handle socket_handle,
      bool playing_automatically) = 0;

  // Called when the AudioOutputIPC object is going away and/or when the IPC
  // channel has been closed and no more ipc requests can be made.
  // Implementations should delete their owned AudioOutputIPC instance
  // immediately.
  virtual void OnIPCClosed() = 0;

 protected:
  virtual ~AudioOutputIPCDelegate();
};

// Provides the IPC functionality for an AudioOutputIPCDelegate (e.g., an
// AudioOutputDevice).  The implementation should asynchronously deliver the
// messages to an AudioOutputController object (or create one in the case of
// CreateStream()), that may live in a separate process.
class MEDIA_EXPORT AudioOutputIPC {
 public:
  virtual ~AudioOutputIPC();

  // Sends a request to authorize the use of a specific audio output device
  // in the peer process.
  // If |device_id| is nonempty, the browser selects the device
  // indicated by |device_id|, regardless of the value of |session_id|.
  // If |device_id| is empty and |session_id| is nonzero, the browser selects
  // the output device associated with an opened input device indicated by
  // |session_id|. If no such device is found, the default device will be
  // selected.
  // If |device_id| is empty and |session_id| is zero, the browser selects
  // the default device.
  // Once the authorization process is complete, the implementation will
  // notify |delegate| by calling OnDeviceAuthorized().
  virtual void RequestDeviceAuthorization(
      AudioOutputIPCDelegate* delegate,
      const base::UnguessableToken& session_id,
      const std::string& device_id) = 0;

  // Sends a request to create an AudioOutputController object in the peer
  // process and configures it to use the specified audio |params| including
  // number of synchronized input channels.
  // If no authorization for an output device has been previously requested,
  // the default device will be used.
  // Once the stream has been created, the implementation will notify
  // |delegate| by calling OnStreamCreated().
  virtual void CreateStream(
      AudioOutputIPCDelegate* delegate,
      const AudioParameters& params,
      const base::Optional<base::UnguessableToken>& processing_id) = 0;

  // Starts playing the stream.  This should generate a call to
  // AudioOutputController::Play().
  virtual void PlayStream() = 0;

  // Pauses an audio stream.  This should generate a call to
  // AudioOutputController::Pause().
  virtual void PauseStream() = 0;

  // Flushes an audio stream. This should only be called when the stream is
  // paused.
  virtual void FlushStream() = 0;

  // Closes the audio stream which should shut down the corresponding
  // AudioOutputController in the peer process. Usage of an AudioOutputIPC must
  // always end with a call to CloseStream(), and the |delegate| passed to other
  // method must remain valid until then. An exception is if OnIPCClosed is
  // called first.
  virtual void CloseStream() = 0;

  // Sets the volume of the audio stream.
  virtual void SetVolume(double volume) = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_IPC_H_
