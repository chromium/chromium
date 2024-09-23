// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_CONTROLLER_H_
#define SERVICES_AUDIO_OUTPUT_CONTROLLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/atomic_ref_count.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_power_monitor.h"
#include "services/audio/loopback_group_member.h"

// An OutputController controls an AudioOutputStream and provides data to this
// output stream. It executes audio operations like play, pause, stop, etc. on
// the audio manager thread, while the audio data flow occurs on the platform's
// realtime audio thread.
//
// Here is a state transition diagram for the OutputController:
//
//   *[ Empty ]  -->  [ Created ]  -->  [ Playing ]  -------.
//        |                |               |    ^           |
//        |                |               |    |           |
//        |                |               |    |           v
//        |                |               |    `-----  [ Paused ]
//        |                |               |                |
//        |                v               v                |
//        `----------->  [      Closed       ]  <-----------'
//
// * Initial state
//
// At any time after reaching the Created state but before Closed, the
// OutputController may be notified of a device change via OnDeviceChange().  As
// the OnDeviceChange() is processed, state transitions will occur, ultimately
// ending up in an equivalent pre-call state.  E.g., if the state was Paused,
// the new state will be Created, since these states are all functionally
// equivalent and require a Play() call to continue to the next state.
//
// The AudioOutputStream can request data from the OutputController via the
// AudioSourceCallback interface. OutputController uses the SyncReader passed to
// it via construction to synchronously fulfill this read request.

namespace audio {
class OutputController : public media::AudioOutputStream::AudioSourceCallback,
                         public LoopbackGroupMember {
 public:
  // An event handler that receives events from the OutputController. The
  // following methods are called on the audio manager thread.
  class EventHandler {
   public:
    virtual void OnControllerPlaying() = 0;
    virtual void OnControllerPaused() = 0;
    virtual void OnControllerError() = 0;
    virtual void OnLog(std::string_view message) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // A synchronous reader interface used by OutputController for synchronous
  // reading.
  // TODO(crogers): find a better name for this class and the Read() method
  // now that it can handle synchronized I/O.
  class SyncReader {
   public:
    virtual ~SyncReader() {}

    // This is used by SyncReader to prepare more data and perform
    // synchronization. Also inform about output delay at a certain moment and
    // if any frames have been skipped by the renderer (typically the OS). The
    // renderer source can handle this appropriately depending on the type of
    // source. An ordinary file playout would ignore this.
    virtual void RequestMoreData(base::TimeDelta delay,
                                 base::TimeTicks delay_timestamp,
                                 const media::AudioGlitchInfo& glitch_info) = 0;

    // Attempts to completely fill `dest`, zeroing `dest` if the request can not
    // be fulfilled (due to timeout). If `is_mixing` is set, the SyncReader
    // might use a mixing-specific timeout.
    // Returns true if data was read, false if there was a timeout. This helps
    // distinguish between `dest` being zero'ed due to timeout, and `dest` being
    // successfully filled with zero'ed audio data.
    virtual bool Read(media::AudioBus* dest, bool is_mixing) = 0;

    // Close this synchronous reader.
    virtual void Close() = 0;
  };

  // Internal state of the source.
  enum State {
    kEmpty,
    kCreated,
    kPlaying,
    kPaused,
    kClosed,
    kError,
  };

  // OutputController guarantees that |on_device_change_callback| will
  // synchronously close the stream received in
  // ManagedDeviceOutputStreamCreateCallback.
  using ManagedDeviceOutputStreamCreateCallback =
      base::RepeatingCallback<media::AudioOutputStream*(
          const std::string&,
          const media::AudioParameters&,
          base::OnceClosure on_device_change_callback)>;

  // `audio_manager` and `handler` must outlive OutputController.  The
  // `output_device_id` can be either empty (default device) or specify a
  // specific hardware device for audio output.
  // If `managed_device_output_stream_create_callback` is provided, it will be
  // used to create a device stream under control; otherwise the stream will be
  // created using `audio_manager`.
  OutputController(media::AudioManager* audio_manager,
                   EventHandler* handler,
                   const media::AudioParameters& params,
                   const std::string& output_device_id,
                   SyncReader* sync_reader,
                   ManagedDeviceOutputStreamCreateCallback
                       managed_device_output_stream_create_callback =
                           ManagedDeviceOutputStreamCreateCallback());

  OutputController(const OutputController&) = delete;
  OutputController& operator=(const OutputController&) = delete;

  ~OutputController() override;

  // Indicates whether audio power level analysis will be performed.  If false,
  // ReadCurrentPowerAndClip() can not be called.
  static constexpr bool will_monitor_audio_levels() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return false;
#else
    return true;
#endif
  }

  // Methods to control playback of the stream.

  // Creates the audio output stream. This must be called before Play(). Returns
  // true if successful, and Play() may commence.
  bool CreateStream();

  // Starts the playback of this audio output stream.
  void Play();

  // Pause this audio output stream.
  void Pause();

  // Flushes the audio output stream.
  // This should only be called if the audio output stream is not playing.
  void Flush();

  // Closes the audio output stream synchronously. Stops the stream first, if
  // necessary. After this method returns, this OutputController can be
  // destroyed by its owner.
  void Close();

  // Sets the volume of the audio output stream.
  void SetVolume(double volume);

  // AudioSourceCallback implementation.
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest) override;
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest,
                 bool is_mixing) override;
  void OnError(ErrorType type) override;

  // LoopbackGroupMember implementation.
  const media::AudioParameters& GetAudioParameters() const override;
  void StartSnooping(Snooper* snooper) override;
  void StopSnooping(Snooper* snooper) override;
  void StartMuting() override;
  void StopMuting() override;

  // Accessor for AudioPowerMonitor::ReadCurrentPowerAndClip().  See comments in
  // audio_power_monitor.h for usage.  This may be called on any thread.
  std::pair<float, bool> ReadCurrentPowerAndClip();

  // Recreates the output stream to play audio to specified device.
  void SwitchAudioOutputDeviceId(const std::string& new_output_device_id);

 protected:
  // Time constant for AudioPowerMonitor.  See AudioPowerMonitor ctor comments
  // for semantics.  This value was arbitrarily chosen, but seems to work well.
  enum { kPowerMeasurementTimeConstantMillis = 10 };

 private:
  // Possible reasons for calling RecreateStream().
  enum class RecreateReason : int8_t {
    INITIAL_STREAM = 0,
    DEVICE_CHANGE = 1,
    LOCAL_OUTPUT_TOGGLE = 2,
  };

  // Used to log the result of rendering startup.
  // Elements in this enum should not be deleted or rearranged; the only
  // permitted operation is to add new elements before kMaxValue and update
  // kMaxValue.
  enum class StreamCreationResult {
    kOk = 0,
    kCreateFailed = 1,
    kOpenFailed = 2,
    kMaxValue = kOpenFailed,
  };

  // Used to store various stats about a stream. The lifetime of this object is
  // from play until pause. The underlying physical stream may be changed when
  // resuming playback, hence separate stats are logged for each play/pause
  // cycle.
  class ErrorStatisticsTracker {
   public:
    explicit ErrorStatisticsTracker(OutputController* controller);

    // Note: the destructor takes care of logging all of the stats.
    ~ErrorStatisticsTracker();

    // Called to indicate an error callback was fired for the stream.
    void RegisterError();

    // This function should be called from the stream callback thread.
    void OnMoreDataCalled();

   private:
    void WedgeCheck();

    // RAW_PTR_EXCLUSION: OutputController object will outlive the
    // ErrorStatisticsTracker object.
    RAW_PTR_EXCLUSION OutputController* const controller_;

    const base::TimeTicks start_time_;

    bool error_during_callback_ = false;

    // Flags when we've asked for a stream to start but it never did.
    base::AtomicRefCount on_more_io_data_called_;
    base::OneShotTimer wedge_timer_;
  };

  // Reports UMA statistics for stream creation.
  static void ReportStreamCreationUma(RecreateReason reason,
                                      StreamCreationResult result);

  static const char* RecreateReasonToString(RecreateReason reason);

  // Closes the current stream and re-creates a new one via the AudioManager. If
  // reason is LOCAL_OUTPUT_TOGGLE, the new stream will be a fake one and UMA
  // counts will not be incremented.
  void RecreateStream(RecreateReason reason);

  // Notifies the EventHandler that an error has occurred.
  void ReportError();

  // Helper method that starts the physical stream. Must only be called in state
  // kCreated or kPaused.
  void StartStream();

  // Helper method that stops the physical stream.
  void StopStream();

  // Helper method that stops, closes, and NULLs |*stream_|.
  void StopCloseAndClearStream();

  // Helper method which delivers a log string to the event handler.
  PRINTF_FORMAT(2, 3) void SendLogMessage(const char* fmt, ...);

  // Log the current average power level measured by power_monitor_.
  void LogAudioPowerLevel(const char* call_name);

  // Helper called by StartMuting() and StopMuting() to execute the stream
  // change.
  void ToggleLocalOutput();

  // When called, OutputController will shutdown the existing |stream_|, create
  // a new stream, and then transition back to an equivalent state prior to
  // being called.
  void ProcessDeviceChange();

  const raw_ptr<media::AudioManager> audio_manager_;
  const media::AudioParameters params_;

  // Callback to create a device output stream; if not specified -
  // |audio_manager_| will be used to create a device output stream.
  ManagedDeviceOutputStreamCreateCallback
      managed_device_output_stream_create_callback_;

  // This object (OC) is owned by an OutputStream (OS) object which is an
  // EventHandler. |handler_| is set at construction by the OS (using this).
  // RAW_PTR_EXCLUSION: OS will always outlive the OC object.
  RAW_PTR_EXCLUSION EventHandler* const handler_;

  // The task runner for the audio manager. All control methods should be called
  // via tasks run by this TaskRunner.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Time when the controller is constructed. Used to record its lifetime on
  // destruction.
  const base::TimeTicks construction_time_;

  // Specifies the device id of the output device to open or empty for the
  // default output device. The device id can be updated by the
  // `SwitchAudioOutputDeviceId()`, which will recreate the stream.
  std::string output_device_id_;

  raw_ptr<media::AudioOutputStream, DanglingUntriaged> stream_;

  // When true, local audio output should be muted; either by having audio
  // diverted to |diverting_to_stream_|, or a fake AudioOutputStream.
  bool disable_local_output_;

  // The snoopers examining or grabbing a copy of the audio data from the
  // OnMoreData() calls.
  base::Lock snooper_lock_;
  std::vector<raw_ptr<Snooper>> snoopers_;

  // The current volume of the audio stream.
  double volume_;

  State state_;

  // SyncReader is used only in low latency mode for synchronous reading.
  const raw_ptr<SyncReader> sync_reader_;

  // Scans audio samples from OnMoreData() as input to compute power levels.
  media::AudioPowerMonitor power_monitor_;

  // Updated each time a power measurement is logged.
  base::TimeTicks last_audio_level_log_time_;

  // Used for keeping track of and logging stats. Created when a stream starts
  // and destroyed when a stream stops. Also reset every time there is a stream
  // being created due to device changes.
  std::optional<ErrorStatisticsTracker> stats_tracker_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_CONTROLLER_H_
