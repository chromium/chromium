// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_CONCURRENT_STREAM_METRIC_REPORTER_H_
#define SERVICES_AUDIO_CONCURRENT_STREAM_METRIC_REPORTER_H_

namespace audio {

class InputStreamActivityMonitor {
 public:
  // Called when an input stream starts recording.
  virtual void OnInputStreamActive() = 0;
  // Called when an input stream stops recording.
  virtual void OnInputStreamInactive() = 0;
};

class OutputStreamActivityMonitor {
 public:
  // Called when an output stream starts playing audio.
  virtual void OnOutputStreamActive() = 0;
  // Called when an output stream stops playing audio.
  virtual void OnOutputStreamInactive() = 0;
};

// ConcurrentStreamMetricReporter monitors input and output streams and reports
// the maximum number of concurrent active OutputStreams observed during active
// InputStream recording. This provides an estimate of how many audio streams
// need to be mixed for echo cancellation.
//
// This class is not thread safe.
class ConcurrentStreamMetricReporter final
    : public InputStreamActivityMonitor,
      public OutputStreamActivityMonitor {
 public:
  // InputStreamActivityMonitor implementation.
  void OnInputStreamActive() override;
  void OnInputStreamInactive() override;

  // OutputStreamActivityMonitor implementation.
  void OnOutputStreamActive() override;
  void OnOutputStreamInactive() override;

 private:
  int active_input_stream_count_ = 0;
  int active_output_stream_count_ = 0;
  int max_concurrent_output_streams_metric_ = 0;
};
}  // namespace audio

#endif  // SERVICES_AUDIO_CONCURRENT_STREAM_METRIC_REPORTER_H_
