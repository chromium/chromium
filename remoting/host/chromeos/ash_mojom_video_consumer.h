// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_ASH_MOJOM_VIDEO_CONSUMER_H_
#define REMOTING_HOST_CHROMEOS_ASH_MOJOM_VIDEO_CONSUMER_H_

#include <memory>

#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

#include "ui/gfx/geometry/rect.h"

namespace remoting {
using viz::mojom::FrameSinkVideoConsumerFrameCallbacks;

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

  std::unique_ptr<webrtc::DesktopFrame> GetLatestFrame();

 private:
  // A single frame received from the FrameSinkVideoCapturer.
  // Will release the memory of |pixels| in its destructor (by invoking the
  // |done_callback_|).
  class Frame;

  // viz::mojom::FrameSinkVideoConsumer implementation:
  void OnFrameCaptured(media::mojom::VideoBufferHandlePtr data,
                       media::mojom::VideoFrameInfoPtr info,
                       const gfx::Rect& content_rect,
                       mojo::PendingRemote<FrameSinkVideoConsumerFrameCallbacks>
                           callbacks) override;
  void OnStopped() override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnLog(const std::string& message) override;
  void OnNewCropVersion(uint32_t crop_version) override;

  std::unique_ptr<Frame> latest_frame_;
  mojo::Receiver<viz::mojom::FrameSinkVideoConsumer> receiver_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_ASH_MOJOM_VIDEO_CONSUMER_H_
