// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AGC_AUDIO_STREAM_H_
#define MEDIA_AUDIO_AGC_AUDIO_STREAM_H_

#include <atomic>

#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "media/audio/audio_io.h"

// The template based AgcAudioStream implements platform-independent parts
// of the AudioInterface interface. Supported interfaces to pass as
// AudioInterface are AudioIntputStream and AudioOutputStream. Each platform-
// dependent implementation should derive from this class.
//
// Usage example (on Windows):
//
//  class WASAPIAudioInputStream : public AgcAudioStream<AudioInputStream> {
//   public:
//    WASAPIAudioInputStream();
//    ...
//  };
//
// Call flow example:
//
//   1) User creates AgcAudioStream<AudioInputStream>
//   2) User calls AudioInputStream::SetAutomaticGainControl(true) =>
//      AGC usage is now initialized but not yet started.
//   3) User calls AudioInputStream::Start() => implementation calls
//      AgcAudioStream<AudioInputStream>::StartAgc() which detects that AGC
//      is enabled and then starts the periodic AGC timer.
//   4) Microphone volume samples are now taken and included in all
//      AudioInputCallback::OnData() callbacks.
//   5) User calls AudioInputStream::Stop() => implementation calls
//      AgcAudioStream<AudioInputStream>::StopAgc() which stops the timer.
//
// Note that, calling AudioInputStream::SetAutomaticGainControl(false) while
// AGC measurements are active will not have an effect until StopAgc(),
// StartAgc() are called again since SetAutomaticGainControl() only sets a
// a state.
//
// Calling SetAutomaticGainControl(true) enables the AGC and StartAgc() starts
// a periodic timer which calls QueryAndStoreNewMicrophoneVolume()
// approximately once every second. QueryAndStoreNewMicrophoneVolume() asks
// the actual microphone about its current volume level. This value is
// normalized and stored so it can be read by GetAgcVolume() when the real-time
// audio thread needs the value. The main idea behind this scheme is to avoid
// accessing the audio hardware from the real-time audio thread and to ensure
// that we don't take new microphone-level samples too often (~1 Hz is a
// suitable compromise). The timer will be active until StopAgc() is called.
//
// This class should be created and destroyed on the audio manager thread and
// a thread checker is added to ensure that this is the case (uses DCHECK).
// All methods except GetAgcVolume() should be called on the creating thread
// as well to ensure that thread safety is maintained. It will also guarantee
// that the periodic timer runs on the audio manager thread.
// |normalized_volume_|, which is updated by QueryAndStoreNewMicrophoneVolume()
// and read in GetAgcVolume(), is atomic to ensure that it can be accessed from
// any real-time audio thread that needs it to update the its AGC volume.

namespace media {

template <typename AudioInterface>
class MEDIA_EXPORT AgcAudioStream : public AudioInterface {
 public:
  // Time between two successive timer events.
  static constexpr base::TimeDelta kIntervalBetweenVolumeUpdates =
      base::Milliseconds(1000);

  AgcAudioStream()
      : agc_is_enabled_(false), max_volume_(0.0), normalized_volume_(0.0) {
  }

  AgcAudioStream(const AgcAudioStream&) = delete;
  AgcAudioStream& operator=(const AgcAudioStream&) = delete;

  virtual ~AgcAudioStream() {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

 protected:
  // Starts the periodic timer which periodically checks and updates the
  // current microphone volume level.
  // The timer is only started if AGC mode is first enabled using the
  // SetAutomaticGainControl() method.
  void StartAgc() {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!agc_is_enabled_ || timer_.IsRunning())
      return;

    max_volume_ = static_cast<AudioInterface*>(this)->GetMaxVolume();
    if (max_volume_ <= 0) {
      DLOG(WARNING) << "Failed to get max volume from hardware. Won't provide "
                    << "normalized volume.";
      return;
    }

    // Query and cache the volume to avoid sending 0 as volume to AGC at the
    // beginning of the audio stream, otherwise AGC will try to raise the
    // volume from 0.
    QueryAndStoreNewMicrophoneVolume();

    timer_.Start(FROM_HERE, kIntervalBetweenVolumeUpdates, this,
                 &AgcAudioStream::QueryAndStoreNewMicrophoneVolume);
  }

  // Stops the periodic timer which periodically checks and updates the
  // current microphone volume level.
  void StopAgc() {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (timer_.IsRunning())
      timer_.Stop();
  }

  // Stores a new microphone volume level by checking the audio input device.
  // Called on the audio manager thread.
  void UpdateAgcVolume() {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (!timer_.IsRunning())
      return;

    // We take new volume samples once every second when the AGC is enabled.
    // To ensure that a new setting has an immediate effect, the new volume
    // setting is cached here. It will ensure that the next OnData() callback
    // will contain a new valid volume level. If this approach was not taken,
    // we could report invalid volume levels to the client for a time period
    // of up to one second.
    QueryAndStoreNewMicrophoneVolume();
  }

  // Gets the latest stored volume level if AGC is enabled.
  // Called at each capture callback on a real-time capture thread (platform
  // dependent).
  void GetAgcVolume(double* normalized_volume) {
    *normalized_volume = normalized_volume_.load(std::memory_order_relaxed);
  }

  // Gets the current automatic gain control state.
  bool GetAutomaticGainControl() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return agc_is_enabled_;
  }

 private:
  // Sets the automatic gain control (AGC) to on or off. When AGC is enabled,
  // the microphone volume is queried periodically and the volume level can
  // be read in each AudioInputCallback::OnData() callback and fed to the
  // render-side AGC. User must call StartAgc() as well to start measuring
  // the microphone level.
  bool SetAutomaticGainControl(bool enabled) override {
    DVLOG(1) << "SetAutomaticGainControl(enabled=" << enabled << ")";
    DCHECK(thread_checker_.CalledOnValidThread());
    agc_is_enabled_ = enabled;
    return true;
  }

  // Takes a new microphone volume sample and stores it in |normalized_volume_|.
  // Range is normalized to [0.0,1.0] or [0.0, 1.5] on Linux.
  // This method is called periodically when AGC is enabled and always on the
  // audio manager thread. We use it to read the current microphone level and
  // to store it so it can be read by the main capture thread. By using this
  // approach, we can avoid accessing audio hardware from a real-time audio
  // thread and it leads to a more stable capture performance.
  void QueryAndStoreNewMicrophoneVolume() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_GT(max_volume_, 0.0);

    // Retrieve the current volume level by asking the audio hardware.
    // Range is normalized to [0.0,1.0] or [0.0, 1.5] on Linux.
    double normalized_volume =
        static_cast<AudioInterface*>(this)->GetVolume() / max_volume_;
    normalized_volume_.store(normalized_volume, std::memory_order_relaxed);
  }

  // Ensures that this class is created and destroyed on the same thread.
  base::ThreadChecker thread_checker_;

  // Repeating timer which cancels itself when it goes out of scope.
  // Used to check the microphone volume periodically.
  base::RepeatingTimer timer_;

  // True when automatic gain control is enabled, false otherwise.
  bool agc_is_enabled_;

  // Stores the maximum volume which is used for normalization to a volume
  // range of [0.0, 1.0].
  double max_volume_;

  // Contains last result of internal call to GetVolume(). We save resources
  // by not querying the capture volume for each callback. The range is
  // normalized to [0.0, 1.0].
  std::atomic<double> normalized_volume_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AGC_AUDIO_STREAM_H_
