// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_INPUT_CONTROLLER_H_
#define SERVICES_AUDIO_INPUT_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/snoopable.h"
#include "services/audio/stream_monitor.h"

namespace media {
class AudioBus;
class AudioInputStream;
class AudioManager;
class UserInputMonitor;
}  // namespace media

namespace audio {

// Only do power monitoring for non-mobile platforms to save resources.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
#define AUDIO_POWER_MONITORING
#endif

// All public methods of InputController must be called from the audio thread.
class InputController final : public StreamMonitor {
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

  // An event handler that receives events from the InputController. The
  // following methods are all called on the audio thread.
  class EventHandler {
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

  // A synchronous writer interface used by InputController for
  // synchronous writing.
  class SyncWriter {
   public:
    virtual ~SyncWriter() {}

    // Write certain amount of data from |data|.
    virtual void Write(const media::AudioBus* data,
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

  ~InputController() final;

  media::AudioInputStream* stream_for_testing() { return stream_; }

  // |user_input_monitor| is used for typing detection and can be NULL.
  static std::unique_ptr<InputController> Create(
      media::AudioManager* audio_manager,
      EventHandler* event_handler,
      SyncWriter* sync_writer,
      media::UserInputMonitor* user_input_monitor,
      const media::AudioParameters& params,
      const std::string& device_id,
      bool agc_is_enabled);

  // Starts recording using the created audio input stream.
  void Record();

  // Closes the audio input stream, freeing the associated resources. Must be
  // called before destruction.
  void Close();

  // Sets the capture volume of the input stream. The value 0.0 corresponds
  // to muted and 1.0 to maximum volume.
  void SetVolume(double volume);

  // Sets the output device which will be used to cancel audio from, if this
  // input device supports echo cancellation.
  void SetOutputDeviceForAec(const std::string& output_device_id);

  // StreamMonitor implementation
  void OnStreamActive(Snoopable* snoopable) override;
  void OnStreamInactive(Snoopable* snoopable) override;

 private:
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

  InputController(EventHandler* handler,
                  SyncWriter* sync_writer,
                  media::UserInputMonitor* user_input_monitor,
                  const media::AudioParameters& params,
                  StreamType type);

  void DoCreate(media::AudioManager* audio_manager,
                const media::AudioParameters& params,
                const std::string& device_id,
                bool enable_agc);
  void DoReportError();
  void DoLogAudioLevels(float level_dbfs, int microphone_volume_percent);

#if defined(AUDIO_POWER_MONITORING)
  // Updates the silence state, see enum SilenceState above for state
  // transitions.
  void UpdateSilenceState(bool silence);

  // Logs the silence state as UMA stat.
  void LogSilenceState(SilenceState value);
#endif

  // Logs the result of creating an InputController.
  void LogCaptureStartupResult(CaptureStartupResult result);

  // Logs whether an error was encountered suring the stream.
  void LogCallbackError();

  // Called by the stream with log messages.
  void LogMessage(const std::string& message);

  // Called on the hw callback thread. Checks for keyboard input if
  // |user_input_monitor_| is set otherwise returns false.
  bool CheckForKeyboardInput();

  // Does power monitoring on supported platforms.
  // Called on the hw callback thread.
  // Returns true iff average power and mic volume was returned and should
  // be posted to DoLogAudioLevels on the audio thread.
  // Returns false if either power measurements are disabled or aren't needed
  // right now (they're done periodically).
  bool CheckAudioPower(const media::AudioBus* source,
                       double volume,
                       float* average_power_dbfs,
                       int* mic_volume_percent);

  void CheckMutedState();

  // Called once at first audio callback.
  void ReportIsAlive();

  static StreamType ParamsToStreamType(const media::AudioParameters& params);

  // This class must be used on the audio manager thread.
  THREAD_CHECKER(owning_thread_);

  // Contains the InputController::EventHandler which receives state
  // notifications from this class.
  EventHandler* const handler_;

  // Pointer to the audio input stream object.
  // Only used on the audio thread.
  media::AudioInputStream* stream_ = nullptr;

  // SyncWriter is used only in low-latency mode for synchronous writing.
  SyncWriter* const sync_writer_;

  StreamType type_;

  double max_volume_ = 0.0;

  media::UserInputMonitor* const user_input_monitor_;

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
  // and that we do not want to keep the InputController instance alive
  // beyond what is desired by the user of the instance. An example of where
  // this is important is when we fire error notifications from the hw callback
  // thread, post them to the audio thread. In that case, we do not want the
  // error notification to keep the InputController alive for as long as
  // the error notification is pending and then make a callback from an
  // InputController that has already been closed.
  // All outstanding weak pointers, are invalidated at the end of DoClose.
  base::WeakPtrFactory<InputController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InputController);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_INPUT_CONTROLLER_H_
