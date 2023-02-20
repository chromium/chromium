// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_
#define MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_

#include <cras_types.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/audio/cras/audio_manager_cras_base.h"
#include "media/audio/cras/cras_util.h"

namespace media {

class MEDIA_EXPORT AudioManagerCras : public AudioManagerCrasBase {
 public:
  AudioManagerCras(std::unique_ptr<AudioThread> audio_thread,
                   AudioLogFactory* audio_log_factory);

  AudioManagerCras(const AudioManagerCras&) = delete;
  AudioManagerCras& operator=(const AudioManagerCras&) = delete;

  ~AudioManagerCras() override;

  // AudioManager implementation.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetDefaultInputDeviceID() override;
  std::string GetDefaultOutputDeviceID() override;
  std::string GetGroupIDInput(const std::string& device_id) override;
  std::string GetGroupIDOutput(const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;

  // AudioManagerCrasBase implementation.
  bool IsDefault(const std::string& device_id, bool is_input) override;
  enum CRAS_CLIENT_TYPE GetClientType() override;

  // Produces AudioParameters for the system, including audio processing
  // capabilities tailored for the system.
  AudioParameters GetStreamParametersForSystem(int user_buffer_size);

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 protected:
  std::unique_ptr<CrasUtil> cras_util_;

 private:
  uint64_t GetPrimaryActiveInputNode();
  uint64_t GetPrimaryActiveOutputNode();
  void GetPrimaryActiveInputNodeOnMainThread(uint64_t* active_input_node_id,
                                             base::WaitableEvent* event);
  void GetPrimaryActiveOutputNodeOnMainThread(uint64_t* active_output_node_id,
                                              base::WaitableEvent* event);
  void GetDefaultOutputBufferSizeOnMainThread(int32_t* buffer_size,
                                              base::WaitableEvent* event);

  // Task runner of browser main thread. CrasAudioHandler should be only
  // accessed on this thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // For posting tasks from audio thread to |main_task_runner_|.
  base::WeakPtr<AudioManagerCras> weak_this_;

  base::WeakPtrFactory<AudioManagerCras> weak_ptr_factory_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_
