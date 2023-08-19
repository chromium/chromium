// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_

#include <memory>
#include <string>

#include "media/audio/audio_manager_base.h"
#include "media/media_buildflags.h"

namespace media {

class AudioDeviceListenerWin;

// Windows implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class MEDIA_EXPORT AudioManagerWin : public AudioManagerBase {
 public:
  AudioManagerWin(std::unique_ptr<AudioThread> audio_thread,
                  AudioLogFactory* audio_log_factory);

  AudioManagerWin(const AudioManagerWin&) = delete;
  AudioManagerWin& operator=(const AudioManagerWin&) = delete;

  ~AudioManagerWin() override;

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;
  const char* GetName() override;

  // Implementation of AudioManagerBase.
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
  AudioOutputStream* MakeBitstreamOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  std::string GetDefaultInputDeviceID() override;
  std::string GetDefaultOutputDeviceID() override;
  std::string GetCommunicationsInputDeviceID() override;
  std::string GetCommunicationsOutputDeviceID() override;

 protected:
  void ShutdownOnAudioThread() override;
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  // Allow unit test to modify the utilized enumeration API.
  friend class AudioManagerTest;

  // Helper methods for performing expensive initialization tasks on the audio
  // thread instead of on the UI thread which AudioManager is constructed on.
  void InitializeOnAudioThread();

  void GetAudioDeviceNamesImpl(bool input, AudioDeviceNames* device_names);

  AudioOutputStream* MakeOutputStream(const AudioParameters& params,
                                      const std::string& device_id,
                                      const LogCallback& log_callback);

  // Listen for output device changes.
  std::unique_ptr<AudioDeviceListenerWin> output_device_listener_;

  // Used to invalidate pending `output_device_listener_` callbacks on shutdow.
  // `audio_weak_factory_` must be invalidated on the audio thread as part of
  // shutdown, before it is destroyed on whichever thread owns `this`.
  base::WeakPtr<AudioManagerWin> weak_this_on_audio_thread_;
  base::WeakPtrFactory<AudioManagerWin> weak_factory_on_audio_thread_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_MANAGER_WIN_H_
