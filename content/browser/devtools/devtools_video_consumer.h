// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_VIDEO_CONSUMER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_VIDEO_CONSUMER_H_

#include <memory>

#include "base/time/time.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/geometry/size.h"

class SkBitmap;

namespace content {

// This class is the video consumer to FrameSinkVideoCapturerImpl. This class,
// in turn sends video frames to its host via the OnFrameCapturedCallback. Used
// when the VizDisplayCompositor feature is enabled.
// TODO(crbug.com/41391202): This class can probably be merged into
// viz::ClientFrameSinkVideoCapturer.
class CONTENT_EXPORT DevToolsVideoConsumer
    : public viz::mojom::FrameSinkVideoConsumer {
 public:
  using OnFrameCapturedCallback =
      base::RepeatingCallback<void(scoped_refptr<media::VideoFrame> frame)>;

  explicit DevToolsVideoConsumer(OnFrameCapturedCallback callback);

  DevToolsVideoConsumer(const DevToolsVideoConsumer&) = delete;
  DevToolsVideoConsumer& operator=(const DevToolsVideoConsumer&) = delete;

  ~DevToolsVideoConsumer() override;

  // Copies |frame| onto a SkBitmap and returns it.
  static SkBitmap GetSkBitmapFromFrame(scoped_refptr<media::VideoFrame> frame);

  // If not currently capturing, this creates the capturer and starts capturing.
  void StartCapture();

  // Stops capturing and resets |capturer_|.
  void StopCapture();

  // These functions cache the values passed to them and if we're currently
  // capturing, they call the corresponding |capturer_| functions.
  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id);
  void SetMinCapturePeriod(base::TimeDelta min_capture_period);
  void SetMinAndMaxFrameSize(gfx::Size min_frame_size,
                             gfx::Size max_frame_size);
  void SetFormat(media::VideoPixelFormat format);

 private:
  friend class DevToolsVideoConsumerTest;

  // Sets |capturer_|, sends capture parameters, and starts capture. Normally,
  // CreateCapturer produces the |capturer|, but unittests can provide a mock.
  void InnerStartCapture(
      std::unique_ptr<viz::ClientFrameSinkVideoCapturer> capturer);

  // Checks that |min_frame_size| and |max_frame_size| are in the expected
  // range. Limits are specified in media::limits.
  bool IsValidMinAndMaxFrameSize(gfx::Size min_frame_size,
                                 gfx::Size max_frame_size);

  // viz::mojom::FrameSinkVideoConsumer:
  void OnFrameCaptured(
      ::media::mojom::VideoBufferHandlePtr data,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override {}
  void OnFrameWithEmptyRegionCapture() override {}
  void OnStopped() override;
  void OnLog(const std::string& /*message*/) override {}

  // Default min frame size is 1x1, as otherwise, nothing would be captured.
  static constexpr gfx::Size kDefaultMinFrameSize = gfx::Size(1, 1);

  // Using an arbitrary default max frame size of 500x500.
  static constexpr gfx::Size kDefaultMaxFrameSize = gfx::Size(500, 500);

  // Callback that is run when a frame is received.
  const OnFrameCapturedCallback callback_;

  // Capture parameters.
  base::TimeDelta min_capture_period_;
  gfx::Size min_frame_size_;
  gfx::Size max_frame_size_;
  viz::FrameSinkId frame_sink_id_;
  media::VideoPixelFormat pixel_format_;

  // If |capturer_| is alive, then we are currently capturing.
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> capturer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_VIDEO_CONSUMER_H_
