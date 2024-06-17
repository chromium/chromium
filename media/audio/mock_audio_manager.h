// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "media/audio/audio_debug_recording_manager.h"
#include "media/audio/audio_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// This class is a simple mock around AudioManager, used exclusively for tests,
// which avoids to use the actual (system and platform dependent) AudioManager.
// Some bots do not have input devices, thus using the actual AudioManager
// would causing failures on classes which expect that.
class MockAudioManager : public AudioManager {
 public:
  using GetDeviceDescriptionsCallback =
      base::RepeatingCallback<void(AudioDeviceDescriptions*)>;
  using GetAssociatedOutputDeviceIDCallback =
      base::RepeatingCallback<std::string(const std::string&)>;
  using MakeOutputStreamCallback =
      base::RepeatingCallback<media::AudioOutputStream*(
          const media::AudioParameters& params,
          const std::string& device_id)>;
  using MakeInputStreamCallback =
      base::RepeatingCallback<media::AudioInputStream*(
          const media::AudioParameters& params,
          const std::string& device_id)>;

  explicit MockAudioManager(std::unique_ptr<AudioThread> audio_thread);

  MockAudioManager(const MockAudioManager&) = delete;
  MockAudioManager& operator=(const MockAudioManager&) = delete;

  ~MockAudioManager() override;

  AudioOutputStream* MakeAudioOutputStream(
      const media::AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  AudioOutputStream* MakeAudioOutputStreamProxy(
      const media::AudioParameters& params,
      const std::string& device_id) override;

  AudioInputStream* MakeAudioInputStream(
      const media::AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  void AddOutputDeviceChangeListener(AudioDeviceListener* listener) override;
  void RemoveOutputDeviceChangeListener(AudioDeviceListener* listener) override;

  std::unique_ptr<AudioLog> CreateAudioLog(
      AudioLogFactory::AudioComponent component,
      int component_id) override;

  void InitializeDebugRecording() override;
  AudioDebugRecordingManager* GetAudioDebugRecordingManager() override;

  void SetAecDumpRecordingManager(base::WeakPtr<AecdumpRecordingManager>
                                      aecdump_recording_manager) override;

  const char* GetName() override;

  // Setters to emulate desired in-test behavior.
  void SetMakeOutputStreamCB(MakeOutputStreamCallback cb);
  void SetMakeInputStreamCB(MakeInputStreamCallback cb);
  void SetInputStreamParameters(const AudioParameters& params);
  void SetOutputStreamParameters(const AudioParameters& params);
  void SetDefaultOutputStreamParameters(const AudioParameters& params);
  void SetHasInputDevices(bool has_input_devices);
  void SetHasOutputDevices(bool has_output_devices);
  void SetInputDeviceDescriptionsCallback(
      GetDeviceDescriptionsCallback callback);
  void SetOutputDeviceDescriptionsCallback(
      GetDeviceDescriptionsCallback callback);
  void SetAssociatedOutputDeviceIDCallback(
      GetAssociatedOutputDeviceIDCallback callback);

 protected:
  void ShutdownOnAudioThread() override;

  bool HasAudioOutputDevices() override;

  bool HasAudioInputDevices() override;

  void GetAudioInputDeviceDescriptions(
      media::AudioDeviceDescriptions* device_descriptions) override;

  void GetAudioOutputDeviceDescriptions(
      media::AudioDeviceDescriptions* device_descriptions) override;

  AudioParameters GetOutputStreamParameters(
      const std::string& device_id) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;
  std::string GetDefaultInputDeviceID() override;
  std::string GetDefaultOutputDeviceID() override;
  std::string GetCommunicationsInputDeviceID() override;
  std::string GetCommunicationsOutputDeviceID() override;

 private:
  AudioParameters input_params_;
  AudioParameters output_params_;
  AudioParameters default_output_params_;
  bool has_input_devices_ = true;
  bool has_output_devices_ = true;
  MakeOutputStreamCallback make_output_stream_cb_;
  MakeInputStreamCallback make_input_stream_cb_;
  GetDeviceDescriptionsCallback get_input_device_descriptions_cb_;
  GetDeviceDescriptionsCallback get_output_device_descriptions_cb_;
  GetAssociatedOutputDeviceIDCallback get_associated_output_device_id_cb_;
  std::unique_ptr<AudioDebugRecordingManager> debug_recording_manager_;
};

}  // namespace media.

#endif  // MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_
