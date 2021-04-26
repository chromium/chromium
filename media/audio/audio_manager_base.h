// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/audio/audio_debug_recording_manager.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace media {

class AudioOutputDispatcher;

// AudioManagerBase provides AudioManager functions common for all platforms.
class MEDIA_EXPORT AudioManagerBase : public AudioManager {
 public:
  enum class VoiceProcessingMode { kDisabled = 0, kEnabled = 1 };

  ~AudioManagerBase() override;

  AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params,
      const std::string& device_id) override;

  // Listeners will be notified on the GetTaskRunner() task runner.
  void AddOutputDeviceChangeListener(AudioDeviceListener* listener) override;
  void RemoveOutputDeviceChangeListener(AudioDeviceListener* listener) override;

  std::unique_ptr<AudioLog> CreateAudioLog(
      AudioLogFactory::AudioComponent component,
      int component_id) override;

  // AudioManagerBase:

  // Called internally by the audio stream when it has been closed.
  virtual void ReleaseOutputStream(AudioOutputStream* stream);
  virtual void ReleaseInputStream(AudioInputStream* stream);

  // Creates the output stream for the |AUDIO_PCM_LINEAR| format. The legacy
  // name is also from |AUDIO_PCM_LINEAR|.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) = 0;

  // Creates the output stream for the |AUDIO_PCM_LOW_LATENCY| format.
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) = 0;

  // Creates the output stream for the |AUDIO_BITSTREAM_XXX| format.
  virtual AudioOutputStream* MakeBitstreamOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback);

  // Creates the input stream for the |AUDIO_PCM_LINEAR| format. The legacy
  // name is also from |AUDIO_PCM_LINEAR|.
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) = 0;

  // Creates the input stream for the |AUDIO_PCM_LOW_LATENCY| format.
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) = 0;

  // Get number of input or output streams.
  int input_stream_count() const {
    return static_cast<int>(input_streams_.size());
  }
  int output_stream_count() const { return num_output_streams_; }

 protected:
  AudioManagerBase(std::unique_ptr<AudioThread> audio_thread,
                   AudioLogFactory* audio_log_factory);

  // AudioManager:
  void ShutdownOnAudioThread() override;

  void GetAudioInputDeviceDescriptions(
      AudioDeviceDescriptions* device_descriptions) final;
  void GetAudioOutputDeviceDescriptions(
      AudioDeviceDescriptions* device_descriptions) final;

  AudioParameters GetDefaultOutputStreamParameters() override;
  AudioParameters GetOutputStreamParameters(
      const std::string& device_id) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;

  void SetMaxOutputStreamsAllowed(int max) { max_num_output_streams_ = max; }

  // Called by each platform specific AudioManager to notify output state change
  // listeners that a state change has occurred.  Must be called from the audio
  // thread.
  void NotifyAllOutputDeviceChangeListeners();

  // Returns user buffer size as specified on the command line or 0 if no buffer
  // size has been specified.
  static int GetUserBufferSize();

  // Returns the preferred hardware audio output parameters for opening output
  // streams. If the users inject a valid |input_params|, each AudioManager
  // will decide if they should return the values from |input_params| or the
  // default hardware values. If the |input_params| is invalid, it will return
  // the default hardware audio parameters.
  // If |output_device_id| is empty, the implementation must treat that as
  // a request for the default output device.
  virtual AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) = 0;

  // Appends a list of available input devices to |device_names|,
  // which must initially be empty.
  virtual void GetAudioInputDeviceNames(AudioDeviceNames* device_names);

  // Appends a list of available output devices to |device_names|,
  // which must initially be empty.
  virtual void GetAudioOutputDeviceNames(AudioDeviceNames* device_names);

  std::string GetDefaultInputDeviceID() override;
  std::string GetDefaultOutputDeviceID() override;
  std::string GetCommunicationsInputDeviceID() override;
  std::string GetCommunicationsOutputDeviceID() override;

  virtual std::unique_ptr<AudioDebugRecordingManager>
  CreateAudioDebugRecordingManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  AudioDebugRecordingManager* GetAudioDebugRecordingManager() final;

  // These functions assign group ids to devices based on their device ids. The
  // default implementation is an attempt to do this based on
  // GetAssociatedOutputDeviceID. They may be overridden by subclasses that want
  // a different logic for assigning group ids. Must be called on the audio
  // worker thread (see GetTaskRunner()).
  virtual std::string GetGroupIDOutput(const std::string& output_device_id);
  virtual std::string GetGroupIDInput(const std::string& input_device_id);

  // Closes all currently open input streams.
  void CloseAllInputStreams();

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioManagerTest, AudioDebugRecording);

  struct DispatcherParams;
  typedef std::vector<std::unique_ptr<DispatcherParams>> AudioOutputDispatchers;

  class CompareByParams;

  // AudioManager:
  void InitializeDebugRecording() final;

  void GetAudioDeviceDescriptions(
      AudioDeviceDescriptions* descriptions,
      void (AudioManagerBase::*get_device_names)(AudioDeviceNames*),
      std::string (AudioManagerBase::*get_default_device_id)(),
      std::string (AudioManagerBase::*get_communications_device_id)(),
      std::string (AudioManagerBase::*get_group_id)(const std::string&));

  // Max number of open output streams, modified by
  // SetMaxOutputStreamsAllowed().
  int max_num_output_streams_;

  // Number of currently open output streams.
  int num_output_streams_;

  // Track output state change listeners.
  base::ObserverList<AudioDeviceListener>::Unchecked output_listeners_;

  // Contains currently open input streams.
  std::unordered_set<AudioInputStream*> input_streams_;

  // Map of cached AudioOutputDispatcher instances.  Must only be touched
  // from the audio thread (no locking).
  AudioOutputDispatchers output_dispatchers_;

  // Proxy for creating AudioLog objects.
  AudioLogFactory* const audio_log_factory_;

  // Debug recording manager.
  std::unique_ptr<AudioDebugRecordingManager> debug_recording_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerBase);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
