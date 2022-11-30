// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CONTENT_ANDROID_THREAD_SAFE_CAPTURE_ORACLE_H_
#define MEDIA_CAPTURE_CONTENT_ANDROID_THREAD_SAFE_CAPTURE_ORACLE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/capture/capture_export.h"
#include "media/capture/content/video_capture_oracle.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/video_capture_device.h"

namespace base {
class Location;
}  // namespace base

namespace media {

struct VideoCaptureParams;
class VideoFrame;
struct VideoFrameMetadata;

// Thread-safe, refcounted proxy to the VideoCaptureOracle.  This proxy wraps
// the VideoCaptureOracle, which decides which frames to capture, and a
// VideoCaptureDevice::Client, which allocates and receives the captured
// frames, in a lock to synchronize state between the two.
class CAPTURE_EXPORT ThreadSafeCaptureOracle
    : public base::RefCountedThreadSafe<ThreadSafeCaptureOracle> {
 public:
  ThreadSafeCaptureOracle(std::unique_ptr<VideoCaptureDevice::Client> client,
                          const VideoCaptureParams& params);

  // Called when a captured frame is available or an error has occurred.
  // If |success| is true then |frame| is valid and |timestamp| indicates when
  // the frame was painted.
  // If |success| is false, all other parameters are invalid.
  typedef base::OnceCallback<void(scoped_refptr<VideoFrame> frame,
                                  base::TimeTicks timestamp,
                                  bool success)>
      CaptureFrameCallback;

  // Record a change |event| along with its |damage_rect| and |event_time|, and
  // then make a decision whether to proceed with capture. The decision is based
  // on recent event history, capture activity, and the availability of
  // resources.
  //
  // If this method returns false, the caller should take no further action.
  // Otherwise, |storage| is set to the destination for the video frame capture
  // and the caller should initiate capture.  Then, once the video frame has
  // been populated with its content, or if capture failed, the |callback|
  // should be run.
  bool ObserveEventAndDecideCapture(VideoCaptureOracle::Event event,
                                    const gfx::Rect& damage_rect,
                                    base::TimeTicks event_time,
                                    scoped_refptr<VideoFrame>* storage,
                                    CaptureFrameCallback* callback);

  // Returns the current capture resolution.
  gfx::Size GetCaptureSize() const;

  // Updates capture resolution based on the supplied source size and the
  // maximum frame size.
  void UpdateCaptureSize(const gfx::Size& source_size);

  // Stop new captures from happening (but doesn't forget the client).
  void Stop();

  // Signal an error to the client.
  void ReportError(media::VideoCaptureError error,
                   const base::Location& from_here,
                   const std::string& reason);

  // Signal device started to the client.
  void ReportStarted();

  void OnConsumerReportingUtilization(
      int frame_number,
      const media::VideoCaptureFeedback& feedback);

 private:
  // Helper struct to hold the many arguments needed by DidCaptureFrame(), and
  // also ensure that teardown of these objects happens in the correct order if
  // bound to an aborted callback.
  struct InFlightFrameCapture;

  friend class base::RefCountedThreadSafe<ThreadSafeCaptureOracle>;
  virtual ~ThreadSafeCaptureOracle();

  // Callback invoked on completion of all captures.
  void DidCaptureFrame(std::unique_ptr<InFlightFrameCapture> capture,
                       scoped_refptr<VideoFrame> frame,
                       base::TimeTicks reference_time,
                       bool success);

  // Callback invoked once all consumers have finished with a delivered video
  // frame.  Consumer feedback signals are scanned from the frame's |metadata|.
  void DidConsumeFrame(int frame_number,
                       const media::VideoFrameMetadata* metadata);

  // Protects everything below it.
  mutable base::Lock lock_;

  // Recipient of our capture activity.
  std::unique_ptr<VideoCaptureDevice::Client> client_;

  // Makes the decision to capture a frame.
  VideoCaptureOracle oracle_;

  // The video capture parameters used to construct the oracle proxy.
  const VideoCaptureParams params_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_CONTENT_ANDROID_THREAD_SAFE_CAPTURE_ORACLE_H_
