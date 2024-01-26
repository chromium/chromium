// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_AUDIO_AUDIO_PLAYBACK_SINK_IOS_H_
#define REMOTING_IOS_AUDIO_AUDIO_PLAYBACK_SINK_IOS_H_

#include <AudioToolbox/AudioToolbox.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/audio/audio_playback_sink.h"

namespace remoting {

// This is the iOS AudioPlaybackSink implementation that uses AudioQueue for
// playback.
class AudioPlaybackSinkIos : public AudioPlaybackSink {
 public:
  AudioPlaybackSinkIos();

  AudioPlaybackSinkIos(const AudioPlaybackSinkIos&) = delete;
  AudioPlaybackSinkIos& operator=(const AudioPlaybackSinkIos&) = delete;

  ~AudioPlaybackSinkIos() override;

  // AudioPlaybackSink implementations.
  void SetDataSupplier(AsyncAudioDataSupplier* supplier) override;
  void ResetStreamFormat(const AudioStreamFormat& format) override;

 private:
  // STOPPED <-----------------------------+------------+
  //    | Received packet                  |            |
  // (Start playback)--------+             |            |
  //    | Failed             | Succeeded   | Buffer     |
  //    v                    v             | Underrun   |
  // SCHEDULED_TO_RESET    RUNNING---------+            |
  //    |                    |                          |
  //    |                    | Sink destructing or      | Audio queue
  //    |                    | format resetting         | destroyed
  //    +--------------------+--------------------------+
  enum class State {
    STOPPED,
    SCHEDULED_TO_RESET,
    RUNNING,
  };

  // Asks |supplier_| to fill audio data into the given buffer.
  void AsyncGetAudioData(AudioQueueBufferRef buffer);

  // Callback called when |supplier_| has finished filling data for |buffer|.
  void OnAudioDataReceived(AudioQueueBufferRef buffer);

  // Callback called when the AudioQueue API finished consuming the audio data.
  static void OnBufferDequeued(void* context,
                               AudioQueueRef outAQ,
                               AudioQueueBufferRef buffer);

  // Starts playback immediately. Posts task to reset the output queue if it
  // fails to start.
  void StartPlayback();

  // Stops playback immediately.
  void StopPlayback();

  // Disposes the current output queue and its buffers, creates a new queue
  // and buffers, and immediately request for audio data from |supplier_|.
  void ResetOutputQueue();

  // Disposes the current output queue and its buffers.
  void DisposeOutputQueue();

  // If |err| is not no-error, prints an error log at DFATAL level and disposes
  // the current output queue. The sink will then not be running until
  // ResetStreamFormat() is called again.
  // Returns true if error occurs and the output queue has been disposed.
  bool HandleError(OSStatus err, const char* function_name);

  THREAD_CHECKER(thread_checker_);

  raw_ptr<AsyncAudioDataSupplier> supplier_ = nullptr;

  // Number of buffers that are currently transferred to |supplier_| for
  // priming.
  size_t priming_buffers_count_ = 0;

  // The current stream format.
  AudioStreamBasicDescription stream_format_;

  // The output queue. nullptr if ResetStreamFormat() has not been called.
  AudioQueueRef output_queue_ = nullptr;

  // The current state.
  State state_ = State::STOPPED;

  base::WeakPtrFactory<AudioPlaybackSinkIos> weak_factory_;
};

}  // namespace remoting

#endif  // REMOTING_IOS_AUDIO_AUDIO_PLAYBACK_SINK_IOS_H_
