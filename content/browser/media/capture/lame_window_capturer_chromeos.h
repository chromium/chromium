// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_LAME_WINDOW_CAPTURER_CHROMEOS_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_LAME_WINDOW_CAPTURER_CHROMEOS_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "content/browser/media/capture/lame_capture_overlay_chromeos.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class CopyOutputResult;
}

namespace content {

// A minimal FrameSinkVideoCapturer implementation for aura::Window video
// capture on ChromeOS (i.e., not desktop capture, and not WebContents capture),
// in cases where a Window does not host a compositor frame sink. This is far
// less efficient than, and far under-performs, the normal
// FrameSinkVideoCapturer provided by the VIZ service, as it lacks multiple
// design features that would be required for low CPU use and high
// pixels-per-sec throughput. It is a placeholder, until the necessary
// infrastructure exists to provide VIZ FrameSinkVideoCapturer the compositor
// frame sink it needs for aura::Windows in the middle of the window tree
// hierarchy.
//
// As this is not meant to be a full-fledged, long-term implementation, it only
// supports the production of I420-format video (Rec. 709 color space) at a
// maximum rate of 5 FPS, and only a maximum of 3 frames can be in-flight at any
// one time. In addition, since source content changes cannot be detected, this
// capturer indefinitely produces frames at a constant framerate while it is
// running.
//
// TODO(crbug/806366): The goal is to remove this code by 2019.
class LameWindowCapturerChromeOS : public viz::mojom::FrameSinkVideoCapturer,
                                   public LameCaptureOverlayChromeOS::Owner,
                                   public aura::WindowObserver {
 public:
  explicit LameWindowCapturerChromeOS(aura::Window* target);
  ~LameWindowCapturerChromeOS() final;

  // viz::mojom::FrameSinkVideoCapturer implementation.
  void SetFormat(media::VideoPixelFormat format,
                 const gfx::ColorSpace& color_space) final;
  void SetMinCapturePeriod(base::TimeDelta min_capture_period) final;
  void SetMinSizeChangePeriod(base::TimeDelta min_period) final;
  void SetResolutionConstraints(const gfx::Size& min_size,
                                const gfx::Size& max_size,
                                bool use_fixed_aspect_ratio) final;
  void SetAutoThrottlingEnabled(bool enabled) final;
  void ChangeTarget(
      const base::Optional<viz::FrameSinkId>& frame_sink_id) final;
  void Start(
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumer> consumer) final;
  void Stop() final;
  void RequestRefreshFrame() final;
  void CreateOverlay(
      int32_t stacking_index,
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay> receiver)
      final;

 private:
  // Represents an in-flight frame, being populated by this capturer and then
  // delivered to the consumer. When the consumer is done with the frame, this
  // returns the buffer back to the pool.
  class InFlightFrame;

  // LameWindowCapturerChromeOS::Owner implementation.
  void OnOverlayConnectionLost(LameCaptureOverlayChromeOS* overlay) final;

  // Initiates capture of the next frame. This is called periodically by the
  // |timer_|.
  void CaptureNextFrame();

  // Populates the frame from the CopyOutputResult and then calls DeliverFrame.
  void DidCopyFrame(std::unique_ptr<InFlightFrame> in_flight_frame,
                    std::unique_ptr<viz::CopyOutputResult> result);

  // Delivers the frame to the consumer, and sets up the notification path for
  // when the consumer is done with the frame.
  void DeliverFrame(std::unique_ptr<InFlightFrame> in_flight_frame);

  // aura::WindowObserver override.
  void OnWindowDestroying(aura::Window* window) final;

  // The window being captured. If the window is destroyed, this is set to null
  // and only blank black frames will be produced thereafter.
  aura::Window* target_;

  // Capture parameters. The defaults are according to the mojo interface
  // definition comments for viz::mojom::FrameSinkVideoCapturer.
  base::TimeDelta capture_period_ = kAbsoluteMinCapturePeriod;
  gfx::Size capture_size_ = gfx::Size(640, 360);

  // The current consumer. This is set by Start() and cleared by Stop().
  mojo::Remote<viz::mojom::FrameSinkVideoConsumer> consumer_;

  // A timer that calls CaptureNextFrame() periodically, according to the
  // currently-set |capture_period_|. This timer is only running while a
  // consumer is present.
  base::RepeatingTimer timer_;

  // A pool of shared memory buffers for re-use.
  std::vector<base::MappedReadOnlyRegion> buffer_pool_;

  // The current number of frames in-flight. If incrementing this would be
  // exceed kMaxInFlightFrames, frame capture is not attempted.
  int in_flight_count_ = 0;

  // Tick clock time of the first frame since Start() was called. This is used
  // for generating "media offset" VideoFrame timestamps.
  base::TimeTicks first_frame_reference_time_;

  // A value provided in the copy requests to enable VIZ to optimize around
  // video capture.
  const base::UnguessableToken copy_request_source_;

  // An optional overlay to be rendered over each captured video frame.
  std::unique_ptr<LameCaptureOverlayChromeOS> overlay_;

  // Used for cancelling any outstanding activities' results, once Stop() is
  // called and there is no longer a consumer to receive another frame.
  base::WeakPtrFactory<LameWindowCapturerChromeOS> weak_factory_{this};

  // Enforce a very low maximum frame rate (5 FPS), due to the lack of
  // design optimizations. See top-level class comments.
  static constexpr base::TimeDelta kAbsoluteMinCapturePeriod =
      base::TimeDelta::FromMilliseconds(200);

  // The maximum number of frames in-flight at any one time.
  static constexpr int kMaxFramesInFlight = 3;

  DISALLOW_COPY_AND_ASSIGN(LameWindowCapturerChromeOS);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_LAME_WINDOW_CAPTURER_CHROMEOS_H_
