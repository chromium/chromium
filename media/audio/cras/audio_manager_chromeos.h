// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CHROMEOS_H_
#define MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CHROMEOS_H_

#include <cras_types.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/cras/audio_manager_cras_base.h"

namespace media {

class MEDIA_EXPORT AudioManagerChromeOS : public AudioManagerCrasBase {
 public:
  AudioManagerChromeOS(std::unique_ptr<AudioThread> audio_thread,
                   AudioLogFactory* audio_log_factory);

  AudioManagerChromeOS(const AudioManagerChromeOS&) = delete;
  AudioManagerChromeOS& operator=(const AudioManagerChromeOS&) = delete;

  ~AudioManagerChromeOS() override;

  // AudioManager implementation.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;
  std::string GetDefaultInputDeviceID() override;
  std::string GetDefaultOutputDeviceID() override;
  std::string GetGroupIDOutput(const std::string& output_device_id) override;
  std::string GetGroupIDInput(const std::string& input_device_id) override;
  bool Shutdown() override;

  // AudioManagerCrasBase implementation.
  bool IsDefault(const std::string& device_id, bool is_input) override;
  enum CRAS_CLIENT_TYPE GetClientType() override;

  // Stores information about the system audio processing effects and
  // properties that are provided by the system audio processing module (APM).
  struct SystemAudioProcessingInfo {
    bool aec_supported = false;
    int32_t aec_group_id = ash::CrasAudioHandler::kSystemAecGroupIdNotAvailable;
    bool ns_supported = false;
    bool agc_supported = false;
  };

  // Produces AudioParameters for the system, including audio processing
  // capabilities tailored for the system,
  static AudioParameters GetStreamParametersForSystem(
      int user_buffer_size,
      const AudioManagerChromeOS::SystemAudioProcessingInfo& system_apm_info);

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  // Get default output buffer size for this board.
  int GetDefaultOutputBufferSizePerBoard();

  // Get any system APM effects that are supported for this board.
  SystemAudioProcessingInfo GetSystemApmEffectsSupportedPerBoard();

  void GetAudioDeviceNamesImpl(bool is_input, AudioDeviceNames* device_names);

  std::string GetHardwareDeviceFromDeviceId(const ash::AudioDeviceList& devices,
                                            bool is_input,
                                            const std::string& device_id);

  void GetAudioDevices(ash::AudioDeviceList* devices);
  void GetAudioDevicesOnMainThread(ash::AudioDeviceList* devices,
                                   base::WaitableEvent* event);
  uint64_t GetPrimaryActiveInputNode();
  uint64_t GetPrimaryActiveOutputNode();
  void GetPrimaryActiveInputNodeOnMainThread(uint64_t* active_input_node_id,
                                             base::WaitableEvent* event);
  void GetPrimaryActiveOutputNodeOnMainThread(uint64_t* active_output_node_id,
                                              base::WaitableEvent* event);
  void GetDefaultOutputBufferSizeOnMainThread(int32_t* buffer_size,
                                              base::WaitableEvent* event);
  void GetSystemApmEffectsSupportedOnMainThread(
      SystemAudioProcessingInfo* system_apm_info,
      base::WaitableEvent* event);

  void WaitEventOrShutdown(base::WaitableEvent* event);

  // Signaled if AudioManagerCras is shutting down.
  base::WaitableEvent on_shutdown_;

  // Task runner of browser main thread. CrasAudioHandler should be only
  // accessed on this thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // For posting tasks from audio thread to |main_task_runner_|.
  base::WeakPtr<AudioManagerChromeOS> weak_this_;

  base::WeakPtrFactory<AudioManagerChromeOS> weak_ptr_factory_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CHROMEOS_H_
