// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AVFOUNDATION_OUTPUT_STREAM_H_
#define MEDIA_AUDIO_MAC_AVFOUNDATION_OUTPUT_STREAM_H_

#include <CoreAudio/CoreAudio.h>
#include <CoreMedia/CoreMedia.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/free_deleter.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerMac;

// Implementation of AudioOutputStream for macOS using their AVFoundation APIs.
// The main reason to use AVFoundationOutputStream over AUHALStream is that this
// stream can handle multichannel (e.g. 5.1, 7.1) audio inputs and
// enable Apple's Spatial Audio on compatible Airpods. The end result for the
// user is that they should be able to switch their Airpods' Spatial Audio mode
// (i.e. Off, Fixed, Head Tracking) within the control center or by selecting
// the Airpods Menu Icon in the Menu Bar.
//
//
// Overview of Operations:
// An AVFoundationOutputStream is created and owned by AudioManagerMac. All
// control methods (Open, Close, Start, Stop) must be called on the manager's
// thread.
//
// Open() initializes the underlying AVFoundation objects. Start(callback)
// begins playback and initiates requests for audio data via the provided
// `callback`. Stop() halts playback and data requests. Close() releases all
// system resources and notifies AudioManagerMac to destroy this stream object.
//
// The `FeedCallback` method is the data rendering loop. It is not
// called directly but is invoked by the OS on a high-priority background
// queue whenever the audio renderer requests for more data. Its job is to pull
// new audio samples from the `AudioSourceCallback` provided in `Start()`,
// package them into a `CMSampleBuffer`, and enqueue them for playback.

class MEDIA_EXPORT AVFoundationOutputStream : public AudioOutputStream {
 public:
  // `manager` creates this object.
  // `device_uid` is required for the renderer to properly attach to the correct
  // output device. This value is typically human readable, in formats such as
  // "9C-F3-AC-51-A0-B9:output" or "BuiltInSpeakerDevice".
  AVFoundationOutputStream(AudioManagerMac* manager,
                           const AudioParameters& params,
                           std::string_view device_uid);
  AVFoundationOutputStream(const AVFoundationOutputStream&) = delete;
  AVFoundationOutputStream& operator=(const AVFoundationOutputStream&) = delete;
  ~AVFoundationOutputStream() override;

  // AudioOutputStream implementation.
  bool Open() override;
  void Close() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void Flush() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

 private:
  void HandleError();
  void FeedCallback();

  SEQUENCE_CHECKER(sequence_checker_);

  // The Audio Manager needs to be notified when we close.
  const raw_ref<AudioManagerMac> audio_manager_;

  const AudioParameters params_;

  const std::string device_uid_;

  base::Lock lock_;

  // Pointer to the object that will provide the audio samples.
  raw_ptr<AudioSourceCallback> callback_ GUARDED_BY(lock_);

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
  base::apple::ScopedCFTypeRef<CMAudioFormatDescriptionRef> format_description_;

  // Temporary buffer for audio data from the audio source.
  std::unique_ptr<AudioBus> audio_bus_ GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AVFOUNDATION_OUTPUT_STREAM_H_
