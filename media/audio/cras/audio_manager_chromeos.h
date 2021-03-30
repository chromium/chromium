// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CHROMEOS_H_
#define MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CHROMEOS_H_

#include <cras_types.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/components/audio/audio_device.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/cras/audio_manager_cras_base.h"

namespace media {

class MEDIA_EXPORT AudioManagerChromeOS : public AudioManagerCrasBase {
 public:
  AudioManagerChromeOS(std::unique_ptr<AudioThread> audio_thread,
                   AudioLogFactory* audio_log_factory);
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

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  // Get default output buffer size for this board.
  int GetDefaultOutputBufferSizePerBoard();

  // Get if system AEC is supported or not for this board.
  bool GetSystemAecSupportedPerBoard();

  // Get what the system AEC group ID is for this board.
  int32_t GetSystemAecGroupIdPerBoard();

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
  void GetSystemAecSupportedOnMainThread(bool* system_aec_supported,
                                         base::WaitableEvent* event);
  void GetSystemAecGroupIdOnMainThread(int32_t* group_id,
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

  DISALLOW_COPY_AND_ASSIGN(AudioManagerChromeOS);
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CHROMEOS_H_
