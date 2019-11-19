// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_
#define MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

// Deprecated! https://crbug.com/854612. You may be looking for
// services/audio/public/cpp/device_factory.h.

// An AudioInputController controls an AudioInputStream and records data
// from this input stream. The two main methods are Record() and Close() and
// they are both executed on the audio thread which is injected by the two
// alternative factory methods, Create() or CreateForStream().
//
// All public methods of AudioInputController are synchronous if called from
// audio thread, or non-blocking if called from a different thread.
//
// Here is a state diagram for the AudioInputController:
//
//                    .-->  [ Closed / Error ]  <--.
//                    |                            |
//                    |                            |
//               [ Created ]  ---------->  [ Recording ]
//                    ^
//                    |
//              *[  Empty  ]
//
// * Initial state
//
// State sequences:
//
//  [Creating Thread]                     [Audio Thread]
//
//      User               AudioInputController               EventHandler
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Create() ==>        DoCreate()
//                   AudioManager::MakeAudioInputStream()
//                        AudioInputStream::Open()
//                                  .- - - - - - - - - - - - ->   OnError()
//                                  .------------------------->  OnCreated()
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Record() ==>                 DoRecord()
//                      AudioInputStream::Start()
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Close() ==>                  DoClose()
//                        AudioInputStream::Stop()
//                        AudioInputStream::Close()
//                          SyncWriter::Close()
// Closure::Run() <-----------------.
// (closure-task)
//
// The audio thread itself is owned by the AudioManager that the
// AudioInputController holds a reference to.  When performing tasks on the
// audio thread, the controller must not add or release references to the
// AudioManager or itself (since it in turn holds a reference to the manager).
//

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioInputStream;
class AudioManager;
class AudioBus;

// Only do power monitoring for non-mobile platforms to save resources.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
#define AUDIO_POWER_MONITORING
#endif

class UserInputMonitor;

// Deprecated! https://crbug.com/854612. You may be looking for
// services/audio/public/cpp/device_factory.h.
class MEDIA_EXPORT AudioInputController final
    : public base::RefCountedThreadSafe<AudioInputController> {
 public:
  // Error codes to make native logging more clear. These error codes are added
  // to generic error strings to provide a higher degree of details.
  // Changing these values can lead to problems when matching native debug
  // logs with the actual cause of error.
  enum ErrorCode {
    // An unspecified error occured.
    UNKNOWN_ERROR = 0,

    // Failed to create an audio input stream.
    STREAM_CREATE_ERROR,  // = 1

    // Failed to open an audio input stream.
    STREAM_OPEN_ERROR,  // = 2

    // Native input stream reports an error. Exact reason differs between
    // platforms.
    STREAM_ERROR,  // = 3
  };

  // An event handler that receives events from the AudioInputController. The
  // following methods are all called on the audio thread.
  class MEDIA_EXPORT EventHandler {
   public:
    // The initial "muted" state of the underlying stream is sent along with the
    // OnCreated callback, to avoid the stream being treated as unmuted until an
    // OnMuted callback has had time to be processed.
    virtual void OnCreated(bool initially_muted) = 0;
    virtual void OnError(ErrorCode error_code) = 0;
    virtual void OnLog(base::StringPiece) = 0;
    // Called whenever the muted state of the underlying stream changes.
    virtual void OnMuted(bool is_muted) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // A synchronous writer interface used by AudioInputController for
  // synchronous writing.
  class MEDIA_EXPORT SyncWriter {
   public:
    virtual ~SyncWriter() {}

    // Write certain amount of data from |data|.
    virtual void Write(const AudioBus* data,
                       double volume,
                       bool key_pressed,
                       base::TimeTicks capture_time) = 0;

    // Close this synchronous writer.
    virtual void Close() = 0;
  };

  // enum used for determining what UMA stats to report.
  enum StreamType {
    VIRTUAL = 0,
    HIGH_LATENCY = 1,
    LOW_LATENCY = 2,
    FAKE = 3,
  };

  // AudioInputController::Create() can use the currently registered Factory
  // to create the AudioInputController. Factory is intended for testing only.
  // |user_input_monitor| is used for typing detection and can be NULL.
  class Factory {
   public:
    virtual AudioInputController* Create(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        SyncWriter* sync_writer,
        AudioManager* audio_manager,
        EventHandler* event_handler,
        AudioParameters params,
        UserInputMonitor* user_input_monitor,
        StreamType type) = 0;

   protected:
    virtual ~Factory() {}
  };

  // Sets the factory used by the static method Create(). AudioInputController
  // does not take ownership of |factory|. A value of NULL results in an
  // AudioInputController being created directly.
  static void set_factory_for_testing(Factory* factory) { factory_ = factory; }
  AudioInputStream* stream_for_testing() { return stream_; }

  // The audio device will be created on the audio thread, and when that is
  // done, the event handler will receive an OnCreated() call from that same
  // thread. |user_input_monitor| is used for typing detection and can be NULL.
  static scoped_refptr<AudioInputController> Create(
      AudioManager* audio_manager,
      EventHandler* event_handler,
      SyncWriter* sync_writer,
      UserInputMonitor* user_input_monitor,
      const AudioParameters& params,
      const std::string& device_id,
      // External synchronous writer for audio controller.
      bool agc_is_enabled);

  // Factory method for creating an AudioInputController with an existing
  // |stream|. The stream will be opened on the audio thread, and when that is
  // done, the event  handler will receive an OnCreated() call from that same
  // thread. |user_input_monitor| is used for typing detection and can be NULL.
  static scoped_refptr<AudioInputController> CreateForStream(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      EventHandler* event_handler,
      AudioInputStream* stream,
      // External synchronous writer for audio controller.
      SyncWriter* sync_writer,
      UserInputMonitor* user_input_monitor);

  // Starts recording using the created audio input stream.
  // This method is called on the creator thread.
  virtual void Record();

  // Closes the audio input stream. The state is changed and the resources
  // are freed on the audio thread. |closed_task| is then executed on the thread
  // that called Close().
  // Callbacks (EventHandler and SyncWriter) must exist until |closed_task|
  // is called.
  // It is safe to call this method more than once. Calls after the first one
  // will have no effect.
  // This method trampolines to the audio thread.
  virtual void Close(base::OnceClosure closed_task);

  // Sets the capture volume of the input stream. The value 0.0 corresponds
  // to muted and 1.0 to maximum volume.
  virtual void SetVolume(double volume);

  // Sets the output device which will be used to cancel audio from, if this
  // input device supports echo cancellation.
  virtual void SetOutputDeviceForAec(const std::string& output_device_id);

 protected:
  friend class base::RefCountedThreadSafe<AudioInputController>;

  // Used to log the result of capture startup.
  // This was previously logged as a boolean with only the no callback and OK
  // options. The enum order is kept to ensure backwards compatibility.
  // Elements in this enum should not be deleted or rearranged; the only
  // permitted operation is to add new elements before
  // CAPTURE_STARTUP_RESULT_MAX and update CAPTURE_STARTUP_RESULT_MAX.
  //
  // The NO_DATA_CALLBACK enum has been replaced with NEVER_GOT_DATA,
  // and there are also other histograms such as
  // Media.Audio.InputStartupSuccessMac to cover issues similar
  // to the ones the NO_DATA_CALLBACK was intended for.
  enum CaptureStartupResult {
    CAPTURE_STARTUP_OK = 0,
    CAPTURE_STARTUP_CREATE_STREAM_FAILED = 1,
    CAPTURE_STARTUP_OPEN_STREAM_FAILED = 2,
    CAPTURE_STARTUP_NEVER_GOT_DATA = 3,
    CAPTURE_STARTUP_STOPPED_EARLY = 4,
    CAPTURE_STARTUP_RESULT_MAX = CAPTURE_STARTUP_STOPPED_EARLY,
  };

#if defined(AUDIO_POWER_MONITORING)
  // Used to log a silence report (see OnData).
  // Elements in this enum should not be deleted or rearranged; the only
  // permitted operation is to add new elements before SILENCE_STATE_MAX and
  // update SILENCE_STATE_MAX.
  // Possible silence state transitions:
  //           SILENCE_STATE_AUDIO_AND_SILENCE
  //               ^                  ^
  // SILENCE_STATE_ONLY_AUDIO   SILENCE_STATE_ONLY_SILENCE
  //               ^                  ^
  //            SILENCE_STATE_NO_MEASUREMENT
  enum SilenceState {
    SILENCE_STATE_NO_MEASUREMENT = 0,
    SILENCE_STATE_ONLY_AUDIO = 1,
    SILENCE_STATE_ONLY_SILENCE = 2,
    SILENCE_STATE_AUDIO_AND_SILENCE = 3,
    SILENCE_STATE_MAX = SILENCE_STATE_AUDIO_AND_SILENCE
  };
#endif

  AudioInputController(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       EventHandler* handler,
                       SyncWriter* sync_writer,
                       UserInputMonitor* user_input_monitor,
                       const AudioParameters& params,
                       StreamType type);
  virtual ~AudioInputController();

  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunnerForTesting()
      const {
    return task_runner_;
  }

  EventHandler* GetHandlerForTesting() const { return handler_; }

 private:
  // Methods called on the audio thread (owned by the AudioManager).
  void DoCreate(AudioManager* audio_manager,
                const AudioParameters& params,
                const std::string& device_id,
                bool enable_agc);
  void DoCreateForStream(AudioInputStream* stream_to_control, bool enable_agc);
  void DoRecord();
  void DoClose();
  void DoReportError();
  void DoSetVolume(double volume);
  void DoLogAudioLevels(float level_dbfs, int microphone_volume_percent);
  void DoSetOutputDeviceForAec(const std::string& output_device_id);

#if defined(AUDIO_POWER_MONITORING)
  // Updates the silence state, see enum SilenceState above for state
  // transitions.
  void UpdateSilenceState(bool silence);

  // Logs the silence state as UMA stat.
  void LogSilenceState(SilenceState value);
#endif

  // Logs the result of creating an AudioInputController.
  void LogCaptureStartupResult(CaptureStartupResult result);

  // Logs whether an error was encountered suring the stream.
  void LogCallbackError();

  // Called by the stream with log messages.
  void LogMessage(const std::string& message);

  // Called on the hw callback thread. Checks for keyboard input if
  // user_input_monitor_ is set otherwise returns false.
  bool CheckForKeyboardInput();

  // Does power monitoring on supported platforms.
  // Called on the hw callback thread.
  // Returns true iff average power and mic volume was returned and should
  // be posted to DoLogAudioLevels on the audio thread.
  // Returns false if either power measurements are disabled or aren't needed
  // right now (they're done periodically).
  bool CheckAudioPower(const AudioBus* source,
                       double volume,
                       float* average_power_dbfs,
                       int* mic_volume_percent);

  void CheckMutedState();

  static StreamType ParamsToStreamType(const AudioParameters& params);

  SEQUENCE_CHECKER(owning_sequence_);

  // The task runner of audio-manager thread that this object runs on.
  scoped_refptr<base::SingleThreadTaskRunner> const task_runner_;

  // Contains the AudioInputController::EventHandler which receives state
  // notifications from this class.
  EventHandler* const handler_;

  // Pointer to the audio input stream object.
  // Only used on the audio thread.
  AudioInputStream* stream_ = nullptr;

  // SyncWriter is used only in low-latency mode for synchronous writing.
  SyncWriter* const sync_writer_;

  StreamType type_;

  static Factory* factory_;

  double max_volume_ = 0.0;

  UserInputMonitor* const user_input_monitor_;

#if defined(AUDIO_POWER_MONITORING)
  // Whether the silence state and microphone levels should be checked and sent
  // as UMA stats.
  bool power_measurement_is_enabled_ = false;

  // Updated each time a power measurement is performed.
  base::TimeTicks last_audio_level_log_time_;

  // The silence report sent as UMA stat at the end of a session.
  SilenceState silence_state_ = SILENCE_STATE_NO_MEASUREMENT;
#endif

  size_t prev_key_down_count_ = 0;

  // Time when the stream started recording.
  base::TimeTicks stream_create_time_;

  bool is_muted_ = false;
  base::RepeatingTimer check_muted_state_timer_;

  class AudioCallback;
  // Holds a pointer to the callback object that receives audio data from
  // the lower audio layer. Valid only while 'recording' (between calls to
  // stream_->Start() and stream_->Stop()).
  // The value of this pointer is only set and read on the audio thread while
  // the callbacks themselves occur on the hw callback thread. More details
  // in the AudioCallback class in the cc file.
  std::unique_ptr<AudioCallback> audio_callback_;

  // A weak pointer factory that we use when posting tasks to the audio thread
  // that we want to be automatically discarded after Close() has been called
  // and that we do not want to keep the AudioInputController instance alive
  // beyond what is desired by the user of the instance. An example of where
  // this is important is when we fire error notifications from the hw callback
  // thread, post them to the audio thread. In that case, we do not want the
  // error notification to keep the AudioInputController alive for as long as
  // the error notification is pending and then make a callback from an
  // AudioInputController that has already been closed.
  // All outstanding weak pointers, are invalidated at the end of DoClose.
  base::WeakPtrFactory<AudioInputController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioInputController);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_
