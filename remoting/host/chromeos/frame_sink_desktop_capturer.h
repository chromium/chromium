// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_FRAME_SINK_DESKTOP_CAPTURER_H_
#define REMOTING_HOST_CHROMEOS_FRAME_SINK_DESKTOP_CAPTURER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "remoting/host/chromeos/ash_mojom_video_consumer.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "ui/aura/scoped_window_capture_request.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"

namespace remoting {
using viz::mojom::FrameSinkVideoCapturer;
// This class is used to capture the current desktop via |video_consumer_| and
// signal that a new DesktopFrame is ready via |callback_|.
// This class is not thread-safe and must run on the UI thread.
// The registered |callback_| must outlive this class.
class FrameSinkDesktopCapturer : public webrtc::DesktopCapturer {
 public:
  FrameSinkDesktopCapturer();
  explicit FrameSinkDesktopCapturer(AshProxy& ash_proxy);

  FrameSinkDesktopCapturer(const FrameSinkDesktopCapturer&) = delete;
  FrameSinkDesktopCapturer& operator=(const FrameSinkDesktopCapturer&) = delete;

  ~FrameSinkDesktopCapturer() override;

  // webrtc::DesktopCapturer implementation.
  void Start(DesktopCapturer::Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  void BindRemote(
      mojo::PendingReceiver<FrameSinkVideoCapturer> pending_receiver);
  const display::Display* GetSourceDisplay();

 private:
  std::optional<viz::ClientFrameSinkVideoCapturer> video_capturer_;
  const raw_ref<AshProxy> ash_;
  AshMojomVideoConsumer video_consumer_;
  raw_ptr<DesktopCapturer::Callback> callback_ = nullptr;

  DisplayId source_display_id_ = display::kInvalidDisplayId;
  // Helper object that makes our source window in the selected display
  // capturable.
  aura::ScopedWindowCaptureRequest scoped_window_capture_request_;

  base::WeakPtrFactory<FrameSinkDesktopCapturer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_FRAME_SINK_DESKTOP_CAPTURER_H_
