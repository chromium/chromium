// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AECDUMP_RECORDING_MANAGER_H_
#define MEDIA_AUDIO_AECDUMP_RECORDING_MANAGER_H_

#include <map>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AecdumpRecordingSource {
 public:
  // Starts recording an aecdump to |aecdump_file|. If a recording is already
  // ongoing, then that recording is stopped and and a new recording is started
  // to |aecdump_file|.
  virtual void StartAecdump(base::File aecdump_file) = 0;

  // Stops recording an aecdump and closes the file. Does nothing if no
  // recording is ongoing.
  virtual void StopAecdump() = 0;
};

// Manages diagnostic audio processing recordings (so-called aecdumps).
// Aecdump recording sources implement the AecdumpRecordingSource interface and
// register/deregister with the AecdumpRecordingManager.
// All operations, including creation and destruction, must happen on the same
// thread as the |task_runner| provided in the constructor.
class MEDIA_EXPORT AecdumpRecordingManager {
 public:
  using CreateFileCallback = base::RepeatingCallback<
      void(uint32_t id, base::OnceCallback<void(base::File)> reply_callback)>;

  explicit AecdumpRecordingManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  AecdumpRecordingManager(const AecdumpRecordingManager&) = delete;
  AecdumpRecordingManager& operator=(const AecdumpRecordingManager&) = delete;

  virtual ~AecdumpRecordingManager();

  // Starts and stops aecdump recording. Overridden by tests.
  virtual void EnableDebugRecording(CreateFileCallback create_file_callback);
  virtual void DisableDebugRecording();

  // Registers an aecdump recording source. Overridden by tests.
  virtual void RegisterAecdumpSource(AecdumpRecordingSource* source);
  // Registers an aecdump recording source. If aecdump recording is currently
  // enabled, then StopAecdump will be called on the source. Overridden by
  // tests.
  virtual void DeregisterAecdumpSource(AecdumpRecordingSource* source);

  base::WeakPtr<AecdumpRecordingManager> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Forwards to StartRecording() if |manager| is valid, otherwise closes |file|
  // without blocking the thread.
  static void StartRecordingIfValidPointer(
      base::WeakPtr<AecdumpRecordingManager> manager,
      AecdumpRecordingSource* source,
      base::File file);

  // Used as callback for |create_file_callback_|, to ensure the recording
  // source has not been deregistered during file creation.
  void StartRecording(AecdumpRecordingSource* source, base::File file);

  bool IsDebugRecordingEnabled() const;

  // Counter for recording source IDs.
  uint32_t recording_id_counter_ = 1;

  // The task runner this class lives on. Also used for file creation callbacks.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Recorders, one per source. Maps pointer to source id.
  std::map<AecdumpRecordingSource*, uint32_t> aecdump_recording_sources_;

  // Callback for creating aecdump files. When this is not null, debug
  // recording is enabled.
  CreateFileCallback create_file_callback_;

  // For managing debug recording cycles.
  base::WeakPtrFactory<AecdumpRecordingManager> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_AECDUMP_RECORDING_MANAGER_H_
