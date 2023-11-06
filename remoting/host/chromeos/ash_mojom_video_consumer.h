// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_ASH_MOJOM_VIDEO_CONSUMER_H_
#define REMOTING_HOST_CHROMEOS_ASH_MOJOM_VIDEO_CONSUMER_H_

#include <memory>
#include <vector>

#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace remoting {

// This class implements the FrameSinkVideoConsumer interface, binds with a
// remote FrameSinkVideoCapturer and provides a webrtc::DesktopFrame from the
// captured data.
class AshMojomVideoConsumer : public viz::mojom::FrameSinkVideoConsumer {
 public:
  AshMojomVideoConsumer();
  AshMojomVideoConsumer(const AshMojomVideoConsumer&) = delete;
  AshMojomVideoConsumer& operator=(const AshMojomVideoConsumer&) = delete;
  ~AshMojomVideoConsumer() override;

  mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumer> Bind();

  std::unique_ptr<webrtc::DesktopFrame> GetLatestFrame(gfx::Point origin);

 private:
  // A single frame received from the FrameSinkVideoCapturer.
  // Will release the memory of |pixels| in its destructor (by invoking the
  // |done_callback_|).
  class Frame;
  // This helper class will aggregate all updated regions of all frames that
  // were captured by the frame sink capturer but not consumed by WebRTC/CRD.
  // Without this WebRTC would not be aware of updated regions that were part of
  // frames it never consumed,leading to ghosting image issues when the frame is
  // changing fast.
  class UpdatedRegionAggregator {
   public:
    UpdatedRegionAggregator();
    ~UpdatedRegionAggregator();

    webrtc::DesktopRegion TakeUpdatedRegion();
    void AddUpdatedRect(webrtc::DesktopRect updated_rect);
    void HandleSizeChange(gfx::Size new_size);

   private:
    gfx::Size current_frame_size_;
    webrtc::DesktopRegion desktop_region_;
  };

  // viz::mojom::FrameSinkVideoConsumer implementation:
  void OnFrameCaptured(
      media::mojom::VideoBufferHandlePtr data,
      media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnStopped() override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnLog(const std::string& message) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;

  std::unique_ptr<Frame> latest_frame_;
  UpdatedRegionAggregator updated_region_aggregator_;
  mojo::Receiver<viz::mojom::FrameSinkVideoConsumer> receiver_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_ASH_MOJOM_VIDEO_CONSUMER_H_
