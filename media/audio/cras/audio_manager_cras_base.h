// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_BASE_H_
#define MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_BASE_H_

#include <cras_types.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class MEDIA_EXPORT AudioManagerCrasBase : public AudioManagerBase {
 public:
  AudioManagerCrasBase(std::unique_ptr<AudioThread> audio_thread,
                       AudioLogFactory* audio_log_factory);

  AudioManagerCrasBase(const AudioManagerCrasBase&) = delete;
  AudioManagerCrasBase& operator=(const AudioManagerCrasBase&) = delete;

  ~AudioManagerCrasBase() override;

  // AudioManager implementation.
  const char* GetName() override;

  // AudioManagerBase implementation.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  // Checks if |device_id| corresponds to the default device.
  // Set |is_input| to true for capture devices, false for output.
  virtual bool IsDefault(const std::string& device_id, bool is_input) = 0;

  // Returns CRAS client type.
  virtual enum CRAS_CLIENT_TYPE GetClientType() = 0;

  // Registers a CrasInputStream as the source of system AEC dump.
  virtual void RegisterSystemAecDumpSource(AecdumpRecordingSource* stream);

  // Unregisters system AEC dump. Virtual to mock in unittest.
  virtual void DeregisterSystemAecDumpSource(AecdumpRecordingSource* stream);

  virtual void SetAecDumpRecordingManager(
      base::WeakPtr<AecdumpRecordingManager> aecdump_recording_manager)
      override;

 protected:
  // Called by MakeLinearOutputStream and MakeLowLatencyOutputStream.
  AudioOutputStream* MakeOutputStream(const AudioParameters& params,
                                      const std::string& device_id,
                                      const LogCallback& log_callback);

  // Called by MakeLinearInputStream and MakeLowLatencyInputStream.
  AudioInputStream* MakeInputStream(const AudioParameters& params,
                                    const std::string& device_id,
                                    const LogCallback& log_callback);

 private:
  // Manages starting / stopping of aecdump recording.
  base::WeakPtr<AecdumpRecordingManager> aecdump_recording_manager_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_BASE_H_
