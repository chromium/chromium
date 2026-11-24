// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a DirectShow input pin used for receiving captured frames from
// a DirectShow Capture filter.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_SINK_INPUT_PIN_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_SINK_INPUT_PIN_WIN_H_

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/win/pin_base_win.h"
#include "media/capture/video/win/sink_filter_win.h"
#include "media/capture/video_capture_types.h"

namespace media {

// Const used for converting Seconds to REFERENCE_TIME.
extern const REFERENCE_TIME kSecondsToReferenceTime;

// Input pin of the SinkFilter.
class SinkInputPin : public PinBase {
 public:
  SinkInputPin() = delete;

  SinkInputPin(IBaseFilter* filter, SinkFilterObserver* observer);

  SinkInputPin(const SinkInputPin&) = delete;
  SinkInputPin& operator=(const SinkInputPin&) = delete;

  void SetRequestedMediaFormat(VideoPixelFormat pixel_format,
                               float frame_rate,
                               const BITMAPINFOHEADER& info_header);

  // Detaches the observer. Waits for a Receive() call in progress on a
  // streaming thread to return; later calls become no-ops.
  void Shutdown();

  // Implement PinBase.
  bool IsMediaTypeValid(const AM_MEDIA_TYPE* media_type) override;
  bool GetValidMediaType(int index, AM_MEDIA_TYPE* media_type) override;
  IFACEMETHODIMP ReceiveConnection(IPin* connector,
                                   const AM_MEDIA_TYPE* media_type) override;
  IFACEMETHODIMP Disconnect() override;
  IFACEMETHODIMP Receive(IMediaSample* media_sample) override;

 private:
  ~SinkInputPin() override;

  // Guards the members below. Receive() holds it for its whole duration,
  // including calls into the IMediaSample and |observer_|, so that Shutdown()
  // and Disconnect() wait for an in-flight call to return. They block the
  // calling thread (the device's STA thread) without pumping messages until
  // then, which includes any time the upstream filter spends in its sample
  // methods.
  //
  // Lock order: |sink_lock_| is acquired before any lock taken by |observer_|
  // (e.g. VideoCaptureDeviceWin::lock_). Never call into this pin while holding
  // such a lock. |observer_| must not call back into this pin, block on the
  // device thread, or make COM calls that need that thread to pump messages.
  // This lock is separate from PinBase's private lock.
  base::Lock sink_lock_;
  VideoPixelFormat requested_pixel_format_ GUARDED_BY(sink_lock_);
  float requested_frame_rate_ GUARDED_BY(sink_lock_);
  BITMAPINFOHEADER requested_info_header_ GUARDED_BY(sink_lock_);
  VideoCaptureFormat resulting_format_ GUARDED_BY(sink_lock_);
  bool flip_y_ GUARDED_BY(sink_lock_);
  raw_ptr<SinkFilterObserver> observer_ GUARDED_BY(sink_lock_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_SINK_INPUT_PIN_WIN_H_
