// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/atomic_ref_count.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_power_monitor.h"
#include "media/audio/audio_source_diverter.h"
#include "media/base/media_export.h"

// An AudioOutputController controls an AudioOutputStream and provides data
// to this output stream. It has an important function that it executes
// audio operations like play, pause, stop, etc. on a separate thread,
// namely the audio manager thread.
//
// There are two ways to use this class.
// 1. Do all operations (including Create and Close) from a thread different
//    than the Audio Manager thread. In this case, the public methods are
//    non-blocking. The actual operations are performed on the audio manager
//    thread.
// 2. Do all operations (including Create and Close) on the Audio Manager
//    thread. In this case, they are completed synchronously.
// Note: it is not allowed to mix 1. and 2.!
//
// Here is a state transition diagram for the AudioOutputController:
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
// AudioOutputController may be notified of a device change via
// OnDeviceChange().  As the OnDeviceChange() is processed, state transitions
// will occur, ultimately ending up in an equivalent pre-call state.  E.g., if
// the state was Paused, the new state will be Created, since these states are
// all functionally equivalent and require a Play() call to continue to the next
// state.
//
// The AudioOutputStream can request data from the AudioOutputController via the
// AudioSourceCallback interface. AudioOutputController uses the SyncReader
// passed to it via construction to synchronously fulfill this read request.

namespace base {
class UnguessableToken;
}

namespace media {

class MEDIA_EXPORT AudioOutputController
    : public base::RefCountedThreadSafe<AudioOutputController>,
      public AudioOutputStream::AudioSourceCallback,
      public AudioSourceDiverter,
      public AudioManager::AudioDeviceListener {
 public:
  // An event handler that receives events from the AudioOutputController. The
  // following methods are called on the audio manager thread.
  class MEDIA_EXPORT EventHandler {
   public:
    virtual void OnControllerCreated() = 0;
    virtual void OnControllerPlaying() = 0;
    virtual void OnControllerPaused() = 0;
    virtual void OnControllerError() = 0;
    virtual void OnLog(base::StringPiece message) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // A synchronous reader interface used by AudioOutputController for
  // synchronous reading.
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
                                 int prior_frames_skipped) = 0;

    // Attempts to completely fill |dest|, zeroing |dest| if the request can not
    // be fulfilled (due to timeout).
    virtual void Read(AudioBus* dest) = 0;

    // Close this synchronous reader.
    virtual void Close() = 0;
  };

  // Factory method for creating an AudioOutputController.
  // This also creates and opens an AudioOutputStream on the audio manager
  // thread, and if this is successful, the |event_handler| will receive an
  // OnControllerCreated() call from the same audio manager thread.
  // |audio_manager| must outlive AudioOutputController.
  // The |output_device_id| can be either empty (default device) or specify a
  // specific hardware device for audio output.
  static scoped_refptr<AudioOutputController> Create(
      AudioManager* audio_manager,
      EventHandler* event_handler,
      const AudioParameters& params,
      const std::string& output_device_id,
      const base::UnguessableToken& group_id,
      SyncReader* sync_reader);

  // Indicates whether audio power level analysis will be performed.  If false,
  // ReadCurrentPowerAndClip() can not be called.
  static bool will_monitor_audio_levels() {
#if defined(OS_ANDROID) || defined(OS_IOS)
    return false;
#else
    return true;
#endif
  }

  // Methods to control playback of the stream.

  // Starts the playback of this audio output stream.
  void Play();

  // Pause this audio output stream.
  void Pause();

  // Closes the audio output stream. The state is changed and the resources
  // are freed on the audio manager thread. |closed_task| is executed after
  // that, on the thread on which Close was called. Callbacks (EventHandler and
  // SyncReader) must exist until |closed_task| is called, but they are safe
  // to delete after that.
  //
  // When calling this on the audio manager thread, all resources will be
  // released synchronously and there is no need for |closed_task|. In this
  // case, it must be null.
  void Close(base::OnceClosure closed_task);

  // Sets the volume of the audio output stream.
  void SetVolume(double volume);

  // AudioSourceCallback implementation.
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 int prior_frames_skipped,
                 AudioBus* dest) override;
  void OnError() override;

  // AudioDeviceListener implementation.  When called AudioOutputController will
  // shutdown the existing |stream_|, transition to the kRecreating state,
  // create a new stream, and then transition back to an equivalent state prior
  // to being called.
  void OnDeviceChange() override;

  // AudioSourceDiverter implementation.
  const AudioParameters& GetAudioParameters() override;
  void StartDiverting(AudioOutputStream* to_stream) override;
  void StopDiverting() override;
  void StartDuplicating(AudioPushSink* sink) override;
  void StopDuplicating(AudioPushSink* sink) override;

  // Accessor for AudioPowerMonitor::ReadCurrentPowerAndClip().  See comments in
  // audio_power_monitor.h for usage.  This may be called on any thread.
  std::pair<float, bool> ReadCurrentPowerAndClip();

 protected:
  // Internal state of the source.
  enum State {
    kEmpty,
    kCreated,
    kPlaying,
    kPaused,
    kClosed,
    kError,
  };

  // Time constant for AudioPowerMonitor.  See AudioPowerMonitor ctor comments
  // for semantics.  This value was arbitrarily chosen, but seems to work well.
  enum { kPowerMeasurementTimeConstantMillis = 10 };

  friend class base::RefCountedThreadSafe<AudioOutputController>;
  ~AudioOutputController() override;

 private:
  // Used to store various stats about a stream. The lifetime of this object is
  // from play until pause. The underlying physical stream may be changed when
  // resuming playback, hence separate stats are logged for each play/pause
  // cycle.
  class ErrorStatisticsTracker {
   public:
    ErrorStatisticsTracker();

    // Note: the destructor takes care of logging all of the stats.
    ~ErrorStatisticsTracker();

    // Called to indicate an error callback was fired for the stream.
    void RegisterError();

    // This function should be called from the stream callback thread.
    void OnMoreDataCalled();

   private:
    void WedgeCheck();

    const base::TimeTicks start_time_;

    bool error_during_callback_ = false;

    // Flags when we've asked for a stream to start but it never did.
    base::AtomicRefCount on_more_io_data_called_;
    base::OneShotTimer wedge_timer_;
  };

  AudioOutputController(AudioManager* audio_manager, EventHandler* handler,
                        const AudioParameters& params,
                        const std::string& output_device_id,
                        SyncReader* sync_reader);

  // The following methods are executed on the audio manager thread.
  void DoCreate(bool is_for_device_change);
  void DoPlay();
  void DoPause();
  void DoClose();
  void DoSetVolume(double volume);
  void DoReportError();
  void DoStartDiverting(AudioOutputStream* to_stream);
  void DoStopDiverting();
  void DoStartDuplicating(AudioPushSink* sink);
  void DoStopDuplicating(AudioPushSink* sink);

  // Helper method that stops the physical stream.
  void StopStream();

  // Helper method that stops, closes, and NULLs |*stream_|.
  void DoStopCloseAndClearStream();

  // Equivalent to OnDeviceChange(), but without any UMA events recorded. This
  // is necessary for proper apples-to-apples comparison with the new Audio
  // Service code paths. http://crbug.com/866455
  void DoStartOrStopDivertingInternal();

  // Send audio data to each duplication target.
  void BroadcastDataToDuplicationTargets(std::unique_ptr<AudioBus> audio_bus,
                                         base::TimeTicks reference_time);

  // Log the current average power level measured by power_monitor_.
  void LogAudioPowerLevel(const std::string& call_name);

  SEQUENCE_CHECKER(owning_sequence_);

  AudioManager* const audio_manager_;
  const AudioParameters params_;
  EventHandler* const handler_;

  // The message loop of audio manager thread that this object runs on.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Time when the controller is constructed. Used to record its lifetime on
  // destruction.
  const base::TimeTicks construction_time_;

  // Specifies the device id of the output device to open or empty for the
  // default output device.
  std::string output_device_id_;

  AudioOutputStream* stream_;

  // When non-NULL, audio is being diverted to this stream.
  AudioOutputStream* diverting_to_stream_;

  // The targets for audio stream to be copied to. |should_duplicate_| is set to
  // 1 when the OnMoreData() call should proxy the data to
  // BroadcastDataToDuplicationTargets().
  base::flat_set<AudioPushSink*> duplication_targets_;
  base::AtomicRefCount should_duplicate_;

  // The current volume of the audio stream.
  double volume_;

  // |state_| may only be used on the audio manager thread.
  State state_;

  // SyncReader is used only in low latency mode for synchronous reading.
  SyncReader* const sync_reader_;

  // Scans audio samples from OnMoreData() as input to compute power levels.
  AudioPowerMonitor power_monitor_;

  // Updated each time a power measurement is logged.
  base::TimeTicks last_audio_level_log_time_;

  // Used for keeping track of and logging stats. Created when a stream starts
  // and destroyed when a stream stops. Also reset every time there is a stream
  // being created due to device changes.
  base::Optional<ErrorStatisticsTracker> stats_tracker_;

  // WeakPtrFactory and WeakPtr for ignoring errors which occur arround a
  // Stop/Close cycle; e.g., device changes. These errors are generally harmless
  // since a fresh stream is about to be recreated, but if forwarded, renderer
  // side clients may consider them catastrophic and abort their operations.
  //
  // |weak_this_for_errors_| must not be reassigned while a stream is active or
  // we'll have concurrent access from different threads. Only the factory may
  // be used to invalidate WeakPtrs while the stream is active.
  base::WeakPtr<AudioOutputController> weak_this_for_errors_;
  base::WeakPtrFactory<AudioOutputController> weak_factory_for_errors_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputController);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_
